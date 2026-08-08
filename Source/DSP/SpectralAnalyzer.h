#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

//==============================================================================
/**
 * SpectralAnalyzer — High-Performance Multi-Resolution FFT & 3D Waterfall Engine
 *
 * Provides ultra-high-resolution real-time spectral analysis for the futuristic
 * Aurora RTA, 3D Modal Resonator, and Cyber-Bamboo 3D Pipe visualizers.
 *
 * Architecture:
 * - Audio Thread: 100% lock-free sample ingestion into 32768-sample power-of-2 ring buffer.
 * - UI Thread: Multi-Resolution FFT (MR-FFT):
 *     * Low-Band (f < 250 Hz): 8192-point FFT (order 13) providing ~5.38 Hz bin resolution
 *       for distinct, non-flattened sub-bass and bass fundamental modes.
 *     * High-Band (f >= 250 Hz): 2048-point FFT (order 11) for fast transient response,
 *       crisp high-frequency visual tracking, and minimal CPU footprint.
 *     * Smooth Hermite Crossover: Seamless 180 Hz - 320 Hz cubic blending with calibrated
 *       coherent gain normalization.
 *     * Constant-Q Fractional Bin Integration: Matches logarithmic X-axis canvas geometry.
 *     * 20-slice 3D temporal waterfall history.
 */
class SpectralAnalyzer
{
public:
    static constexpr int kFftOrderLow   = 13;
    static constexpr int kFftSizeLow    = 1 << kFftOrderLow;   // 8192
    static constexpr int kScopeSizeLow  = kFftSizeLow / 2;     // 4096

    static constexpr int kFftOrderHigh  = 11;
    static constexpr int kFftSizeHigh   = 1 << kFftOrderHigh;  // 2048
    static constexpr int kScopeSizeHigh = kFftSizeHigh / 2;    // 1024

    static constexpr int kNumBands      = 128;                 // 128 High-Definition Log frequency bands
    static constexpr int kDepthSlices   = 20;                  // 20 depth slices for 3D Waterfall
    static constexpr int kNumPipes      = 16;
    static constexpr int kRingBufSize   = 32768;               // Power of 2 for lock-free bitmasking
    static constexpr int kRingBufMask   = kRingBufSize - 1;

    SpectralAnalyzer()
        : m_fftLow(kFftOrderLow),
          m_windowLow(static_cast<size_t>(kFftSizeLow), juce::dsp::WindowingFunction<float>::hann),
          m_fftHigh(kFftOrderHigh),
          m_windowHigh(static_cast<size_t>(kFftSizeHigh), juce::dsp::WindowingFunction<float>::hann)
    {
        m_ringBuffer.fill(0.0f);
        m_fftDataLow.fill(0.0f);
        m_fftDataHigh.fill(0.0f);
        m_currentSpectrum.fill(0.0f);
        m_pipeAmplitudes.fill(0.0f);

        for (auto& slice : m_historySlices)
            slice.fill(0.0f);
    }

    // Audio Thread: Lock-free sample ingestion
    void pushAudioBlock (const float* const* channelData, int numChannels, int numSamples)
    {
        if (channelData == nullptr || numChannels <= 0 || numSamples <= 0)
            return;

        int wp = m_writePos.load(std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                sum += channelData[ch][i];

            m_ringBuffer[static_cast<size_t>(wp)] = sum / static_cast<float>(numChannels);
            wp = (wp + 1) & kRingBufMask;
        }

        m_writePos.store(wp, std::memory_order_release);
        m_samplesPushed.fetch_add(numSamples, std::memory_order_relaxed);
    }

    // UI Thread: Process Multi-Resolution FFT, Constant-Q Integration & Hermite Stitching
    void processSpectralData (double sampleRate = 44100.0)
    {
        if (sampleRate <= 0.0)
            sampleRate = 44100.0;

        int wp = m_writePos.load(std::memory_order_acquire);

        // 1. Ingest & Window Low-Frequency Buffer (8192 samples -> ~5.38 Hz resolution at 44.1kHz)
        for (int i = 0; i < kFftSizeLow; ++i)
        {
            int readIdx = (wp - kFftSizeLow + i) & kRingBufMask;
            m_fftDataLow[static_cast<size_t>(i)] = m_ringBuffer[static_cast<size_t>(readIdx)];
        }
        std::fill(m_fftDataLow.begin() + kFftSizeLow, m_fftDataLow.end(), 0.0f);
        m_windowLow.multiplyWithWindowingTable(m_fftDataLow.data(), static_cast<size_t>(kFftSizeLow));
        m_fftLow.performFrequencyOnlyForwardTransform(m_fftDataLow.data());

        // 2. Ingest & Window High-Frequency Buffer (2048 samples -> fast transient response)
        for (int i = 0; i < kFftSizeHigh; ++i)
        {
            int readIdx = (wp - kFftSizeHigh + i) & kRingBufMask;
            m_fftDataHigh[static_cast<size_t>(i)] = m_ringBuffer[static_cast<size_t>(readIdx)];
        }
        std::fill(m_fftDataHigh.begin() + kFftSizeHigh, m_fftDataHigh.end(), 0.0f);
        m_windowHigh.multiplyWithWindowingTable(m_fftDataHigh.data(), static_cast<size_t>(kFftSizeHigh));
        m_fftHigh.performFrequencyOnlyForwardTransform(m_fftDataHigh.data());

        // 3. Constant-Q Extraction & Normalization Helper
        auto sampleFftMagnitude = [](const float* data, int fftSize, int scopeSize, float binWidth, float fCenter) -> float
        {
            float exactBin = fCenter / binWidth;
            if (exactBin < 0.5f)
                return data[1] * std::max(0.0f, exactBin * 2.0f) / static_cast<float>(fftSize * 0.16f);

            // Constant-Q integration bandwidth proportional to frequency (Q ~ 22)
            float qWidth = std::max(0.75f, exactBin * 0.045f);
            int binStart = juce::jlimit(1, scopeSize - 1, static_cast<int>(std::floor(exactBin - qWidth * 0.5f)));
            int binEnd   = juce::jlimit(binStart, scopeSize - 1, static_cast<int>(std::ceil(exactBin + qWidth * 0.5f)));

            float peakMag = 0.0f;
            if (binStart == binEnd)
            {
                int b0 = juce::jlimit(1, scopeSize - 2, static_cast<int>(std::floor(exactBin)));
                float frac = exactBin - static_cast<float>(b0);
                peakMag = data[b0] * (1.0f - frac) + data[b0 + 1] * frac;
            }
            else
            {
                for (int b = binStart; b <= binEnd; ++b)
                    peakMag = std::max(peakMag, data[b]);
            }

            // Coherent gain normalization for Hann window
            return peakMag / static_cast<float>(fftSize * 0.16f);
        };

        // 4. Map to 128 Logarithmic Frequency Bands (25 Hz to 20 kHz) with Hermite Crossover
        const float minFreq = 25.0f;
        const float maxFreq = std::min(20000.0f, static_cast<float>(sampleRate * 0.49));
        const float logMin  = std::log10(minFreq);
        const float logMax  = std::log10(maxFreq);

        const float binWidthLow  = static_cast<float>(sampleRate) / static_cast<float>(kFftSizeLow);
        const float binWidthHigh = static_cast<float>(sampleRate) / static_cast<float>(kFftSizeHigh);

        const float kCrossoverLow  = 180.0f;
        const float kCrossoverHigh = 320.0f;

        for (int b = 0; b < kNumBands; ++b)
        {
            float normPos = static_cast<float>(b) / static_cast<float>(kNumBands - 1);
            float fCenter = std::pow(10.0f, logMin + (logMax - logMin) * normPos);

            float normMag = 0.0f;

            if (fCenter < kCrossoverLow)
            {
                // Pure High-Resolution Low-Frequency FFT (8192 pt)
                normMag = sampleFftMagnitude(m_fftDataLow.data(), kFftSizeLow, kScopeSizeLow, binWidthLow, fCenter);
            }
            else if (fCenter > kCrossoverHigh)
            {
                // Pure Fast-Response High-Frequency FFT (2048 pt)
                normMag = sampleFftMagnitude(m_fftDataHigh.data(), kFftSizeHigh, kScopeSizeHigh, binWidthHigh, fCenter);
            }
            else
            {
                // Smooth Hermite Crossover Blending [180 Hz .. 320 Hz]
                float magLow  = sampleFftMagnitude(m_fftDataLow.data(), kFftSizeLow, kScopeSizeLow, binWidthLow, fCenter);
                float magHigh = sampleFftMagnitude(m_fftDataHigh.data(), kFftSizeHigh, kScopeSizeHigh, binWidthHigh, fCenter);

                float t = (fCenter - kCrossoverLow) / (kCrossoverHigh - kCrossoverLow);
                float blend = t * t * (3.0f - 2.0f * t); // Smoothstep Hermite
                normMag = (1.0f - blend) * magLow + blend * magHigh;
            }

            // Logarithmic amplitude scaling (-60dB to 0dB -> [0.0 .. 1.0])
            float db = juce::Decibels::gainToDecibels(normMag + 1e-6f);
            float norm = juce::jlimit(0.0f, 1.0f, juce::jmap(db, -60.0f, 0.0f, 0.0f, 1.0f));

            // Natural smoothstep sub-bass rolloff below 35 Hz for clean baseline aesthetics
            if (fCenter < 35.0f)
            {
                float subTaper = juce::jlimit(0.0f, 1.0f, (fCenter - 22.0f) / 13.0f);
                subTaper = subTaper * subTaper * (3.0f - 2.0f * subTaper);
                norm *= subTaper;
            }

            // Fast responsive attack (0.80), smooth musical decay (0.20)
            if (norm > m_currentSpectrum[static_cast<size_t>(b)])
                m_currentSpectrum[static_cast<size_t>(b)] = m_currentSpectrum[static_cast<size_t>(b)] * 0.20f + norm * 0.80f;
            else
                m_currentSpectrum[static_cast<size_t>(b)] = m_currentSpectrum[static_cast<size_t>(b)] * 0.80f + norm * 0.20f;
        }

        // 5. Push current spectrum into 3D Depth Waterfall History
        m_historyWritePos = (m_historyWritePos + 1) % kDepthSlices;
        m_historySlices[static_cast<size_t>(m_historyWritePos)] = m_currentSpectrum;

        // 6. Extract 16 pipe modal energies based on authentic Khaen pipe frequencies
        static const float pipeFreqs[kNumPipes] = {
            220.00f, 261.63f, 293.66f, 329.63f, 392.00f, 440.00f, 493.88f, 523.25f,
            587.33f, 659.25f, 739.99f, 783.99f, 880.00f, 987.77f, 1046.50f, 1174.66f
        };

        for (int p = 0; p < kNumPipes; ++p)
        {
            float f = pipeFreqs[p];
            float normMag = 0.0f;
            if (f < kCrossoverLow)
                normMag = sampleFftMagnitude(m_fftDataLow.data(), kFftSizeLow, kScopeSizeLow, binWidthLow, f);
            else if (f > kCrossoverHigh)
                normMag = sampleFftMagnitude(m_fftDataHigh.data(), kFftSizeHigh, kScopeSizeHigh, binWidthHigh, f);
            else
            {
                float mLow  = sampleFftMagnitude(m_fftDataLow.data(), kFftSizeLow, kScopeSizeLow, binWidthLow, f);
                float mHigh = sampleFftMagnitude(m_fftDataHigh.data(), kFftSizeHigh, kScopeSizeHigh, binWidthHigh, f);
                float t = (f - kCrossoverLow) / (kCrossoverHigh - kCrossoverLow);
                normMag = (1.0f - (t * t * (3.0f - 2.0f * t))) * mLow + (t * t * (3.0f - 2.0f * t)) * mHigh;
            }

            // Also check octave harmonic
            float f2 = f * 2.0f;
            float mag2 = sampleFftMagnitude(m_fftDataHigh.data(), kFftSizeHigh, kScopeSizeHigh, binWidthHigh, f2) * 0.8f;
            normMag = std::max(normMag, mag2);

            float db = juce::Decibels::gainToDecibels(normMag + 1e-6f);
            float rawNorm = juce::jlimit(0.0f, 1.0f, juce::jmap(db, -38.0f, -4.0f, 0.0f, 1.0f));
            float norm = rawNorm * rawNorm; // Clean quadratic response curve prevents false continuous glowing

            if (norm > m_pipeAmplitudes[static_cast<size_t>(p)])
                m_pipeAmplitudes[static_cast<size_t>(p)] = m_pipeAmplitudes[static_cast<size_t>(p)] * 0.15f + norm * 0.85f;
            else
                m_pipeAmplitudes[static_cast<size_t>(p)] = m_pipeAmplitudes[static_cast<size_t>(p)] * 0.82f + norm * 0.18f;
        }
    }

    // Accessors for UI Rendering
    const std::array<float, kNumBands>& getCurrentSpectrum() const noexcept { return m_currentSpectrum; }

    // Returns a history slice (0 = newest / front slice, kDepthSlices - 1 = oldest / back slice)
    const std::array<float, kNumBands>& getHistorySlice (int depthIndex) const noexcept
    {
        int idx = (m_historyWritePos - depthIndex) % kDepthSlices;
        if (idx < 0) idx += kDepthSlices;
        return m_historySlices[static_cast<size_t>(idx)];
    }

    const std::array<float, kNumPipes>& getPipeAmplitudes() const noexcept { return m_pipeAmplitudes; }

private:
    // Multi-Resolution FFT Engines & Hann Windowing
    juce::dsp::FFT m_fftLow;
    juce::dsp::WindowingFunction<float> m_windowLow;

    juce::dsp::FFT m_fftHigh;
    juce::dsp::WindowingFunction<float> m_windowHigh;

    // Lock-free circular sample ring buffer (32768 samples)
    std::array<float, kRingBufSize> m_ringBuffer;
    std::atomic<int> m_writePos { 0 };
    std::atomic<int> m_samplesPushed { 0 };

    // Processing buffers on UI / Analysis thread
    std::array<float, kFftSizeLow * 2>  m_fftDataLow;
    std::array<float, kFftSizeHigh * 2> m_fftDataHigh;
    std::array<float, kNumBands>        m_currentSpectrum;
    std::array<std::array<float, kNumBands>, kDepthSlices> m_historySlices;
    int m_historyWritePos = 0;
    std::array<float, kNumPipes>        m_pipeAmplitudes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralAnalyzer)
};
