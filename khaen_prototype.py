"""
khaen_prototype.py — Prototipo portátil del sintetizador Khaen
=============================================================
Implementa el modelo físico de caña libre (Digital Waveguide) en Python puro.
Requiere solo: numpy, scipy, sounddevice (o simpleaudio)

Instalación rápida:
    pip install numpy scipy sounddevice

Uso:
    python khaen_prototype.py            # Reproduce demo interactiva
    python khaen_prototype.py --save     # Guarda demo.wav
    python khaen_prototype.py --gui      # Abre GUI con sliders (requiere tkinter)
"""

import numpy as np
from scipy.io import wavfile
import argparse
import sys
import os

# ─── CONSTANTES ───────────────────────────────────────────────────────────────
SAMPLE_RATE = 44100
MAX_DELAY   = 65536

# ─── PALETA DE MATERIALES ─────────────────────────────────────────────────────
MATERIALS = {
    "Bambú":         0.960,
    "Bambú Añejado": 0.945,
    "Jade":          0.975,
    "Latón":         0.980,
    "PVC":           0.940,
    "Madera Oscura": 0.950,
    "Metal":         0.985,
}

# ─── 10 PRESETS ───────────────────────────────────────────────────────────────
PRESETS = [
    {"name": "Khaen Clásico",   "stiffness": 0.65, "bore_mm": 14, "pressure": 0.55, "material": "Bambú"},
    {"name": "Breath of Laos",  "stiffness": 0.50, "bore_mm": 16, "pressure": 0.65, "material": "Bambú Añejado"},
    {"name": "Jade Reed",       "stiffness": 0.80, "bore_mm": 12, "pressure": 0.50, "material": "Jade"},
    {"name": "Copper Tongue",   "stiffness": 0.90, "bore_mm": 10, "pressure": 0.60, "material": "Latón"},
    {"name": "Ghost Pipe",      "stiffness": 0.30, "bore_mm": 20, "pressure": 0.40, "material": "PVC"},
    {"name": "Deep Resonance",  "stiffness": 0.45, "bore_mm": 22, "pressure": 0.70, "material": "Madera Oscura"},
    {"name": "Crystal Air",     "stiffness": 0.70, "bore_mm": 13, "pressure": 0.45, "material": "Bambú"},
    {"name": "Storm Reed",      "stiffness": 0.95, "bore_mm": 11, "pressure": 0.80, "material": "Metal"},
    {"name": "Silk Breath",     "stiffness": 0.40, "bore_mm": 18, "pressure": 0.48, "material": "Bambú"},
    {"name": "Void Drone",      "stiffness": 0.20, "bore_mm": 28, "pressure": 0.30, "material": "PVC"},
]


# ══════════════════════════════════════════════════════════════════════════════
#  MOTOR DSP — Digital Waveguide Free Reed Model
# ══════════════════════════════════════════════════════════════════════════════

class FreeReedModel:
    """
    Modelo físico de caña libre con resonador de tubo cilíndrico.
    Basado en Digital Waveguide Synthesis (Julius O. Smith III).
    """

    def __init__(self, sample_rate: int = SAMPLE_RATE):
        self.sr = sample_rate
        self._fwd = np.zeros(MAX_DELAY, dtype=np.float32)  # forward wave
        self._bwd = np.zeros(MAX_DELAY, dtype=np.float32)  # backward wave
        self._wp  = 0        # write pointer
        self._lp_state = 0.0 # loss filter state
        self._reed_disp = 0.0
        self._reed_vel  = 0.0

        # Parámetros por defecto
        self.reed_stiffness  = 0.65
        self.bore_diameter   = 14.0   # mm
        self.blow_pressure   = 0.55
        self.lip_damping     = 0.30
        self.bore_reflection = 0.97
        self.loss_coeff      = 0.960  # por material

        # Pitch actual
        self._delay_samples  = 200.0
        self._frac_delay     = 0.0

    def set_pitch(self, midi_note: float):
        """Convierte MIDI note → delay del waveguide."""
        freq = 440.0 * (2.0 ** ((midi_note - 69.0) / 12.0))
        self._delay_samples = max(4.0, self.sr / freq - 2.0)
        self._frac_delay    = self._delay_samples % 1.0

    def set_material(self, name: str):
        self.loss_coeff = MATERIALS.get(name, 0.960)
        # Ajuste por diámetro del tubo
        bore_factor = np.interp(self.bore_diameter, [8, 30], [0.97, 0.99])
        self.loss_coeff = min(0.999, self.loss_coeff * bore_factor)

    def _reed_flow(self, delta_p: float) -> float:
        """Función de flujo no lineal de Hirschberg."""
        threshold = 0.1 + self.reed_stiffness * 0.4
        if delta_p < -threshold:
            return 0.0
        flow = np.copysign(np.sqrt(abs(delta_p) + 1e-9), delta_p)
        return np.tanh(flow * 2.0) * 0.5

    def _read(self, buf: np.ndarray, delay_smp: float) -> float:
        """Lectura con interpolación lineal."""
        int_d = int(delay_smp)
        frac  = delay_smp - int_d
        i0 = (self._wp - int_d - 1) % MAX_DELAY
        i1 = (self._wp - int_d - 2) % MAX_DELAY
        return float(buf[i0]) * (1 - frac) + float(buf[i1]) * frac

    def process_sample(self, breath_pressure: float = None) -> float:
        """Genera un sample de audio del modelo físico."""
        Pm = (breath_pressure if breath_pressure is not None
              else self.blow_pressure)

        # 1. Leer reflexión del resonador
        half_del = self._delay_samples * 0.5
        p_reflect = self._read(self._bwd, half_del)

        # 2. Excitador de caña no lineal
        delta_p  = Pm - p_reflect
        reed_flow = self._reed_flow(delta_p)
        stiff_coeff = 1.0 - self.reed_stiffness * 0.8
        reed_flow  *= stiff_coeff

        # Dinámica de la lengüeta
        lip_smt = 0.1 + 0.8 * 0.3  # usando lip_damping por defecto
        self._reed_vel  += (reed_flow - self._reed_disp) * lip_smt
        self._reed_vel  *= (1.0 - self.lip_damping * 0.2)
        self._reed_disp += self._reed_vel * 0.3
        excitation = self._reed_disp

        # 3. Inyectar en ola forward + aplicar reflexión del tubo
        fwd_input = excitation * 0.5 + p_reflect
        # Saturador suave para estabilidad
        fwd_input = np.tanh(fwd_input * 1.5) * 0.66
        self._fwd[self._wp % MAX_DELAY] = fwd_input

        fwd_smp = self._read(self._fwd, half_del)

        # Filtro de pérdidas (1 polo)
        self._lp_state = (fwd_smp * (1.0 - self.loss_coeff)
                          + self._lp_state * self.loss_coeff)

        # Reflexión extremo abierto (negativa)
        self._bwd[self._wp % MAX_DELAY] = -self._lp_state * self.bore_reflection

        self._wp += 1

        # 4. Salida = media de olas
        output = (fwd_smp + self._read(self._bwd, 1.0)) * 0.5
        return float(np.clip(output, -1.0, 1.0))

    def reset(self):
        self._fwd.fill(0)
        self._bwd.fill(0)
        self._wp = 0
        self._lp_state = 0.0
        self._reed_disp = 0.0
        self._reed_vel  = 0.0


# ══════════════════════════════════════════════════════════════════════════════
#  TAPE DELAY simple
# ══════════════════════════════════════════════════════════════════════════════

class TapeDelay:
    def __init__(self, sr: int = SAMPLE_RATE, max_delay_s: float = 2.0):
        self.sr     = sr
        self.buf    = np.zeros(int(sr * max_delay_s), dtype=np.float32)
        self.wp     = 0
        self.delay_ms = 400.0
        self.feedback = 0.35
        self.mix      = 0.25

    def process(self, x: float) -> float:
        delay_smp = self.delay_ms * 0.001 * self.sr
        int_d = int(delay_smp)
        frac  = delay_smp - int_d
        i0 = (self.wp - int_d - 1) % len(self.buf)
        i1 = (self.wp - int_d - 2) % len(self.buf)
        wet = self.buf[i0] * (1 - frac) + self.buf[i1] * frac
        self.buf[self.wp % len(self.buf)] = x + wet * self.feedback
        self.wp += 1
        return x * (1 - self.mix) + wet * self.mix


# ══════════════════════════════════════════════════════════════════════════════
#  RENDERIZADOR DE NOTAS
# ══════════════════════════════════════════════════════════════════════════════

def render_note(midi_note: float, duration_s: float,
                preset: dict = None,
                attack_s: float = 0.02,
                release_s: float = 0.15,
                delay_on: bool = True,
                sr: int = SAMPLE_RATE) -> np.ndarray:
    """Renderiza una nota completa con envolvente y delay opcional."""

    model = FreeReedModel(sr)
    delay = TapeDelay(sr) if delay_on else None

    if preset:
        model.reed_stiffness  = preset.get("stiffness", 0.65)
        model.bore_diameter   = preset.get("bore_mm", 14.0)
        model.blow_pressure   = preset.get("pressure", 0.55)
        model.set_material(preset.get("material", "Bambú"))

    model.set_pitch(midi_note)

    n_samples = int(duration_s * sr)
    n_attack  = int(attack_s * sr)
    n_release = int(release_s * sr)
    n_sustain = n_samples - n_attack - n_release

    # Envolvente ADSR simplificada
    env = np.concatenate([
        np.linspace(0, 1, n_attack),
        np.ones(max(0, n_sustain)),
        np.linspace(1, 0, n_release)
    ])[:n_samples]

    out = np.zeros(n_samples, dtype=np.float32)
    for i in range(n_samples):
        sample = model.process_sample(model.blow_pressure * env[i])
        if delay:
            sample = delay.process(sample)
        out[i] = sample

    return out


def render_demo(preset_idx: int = 0, sr: int = SAMPLE_RATE) -> np.ndarray:
    """Renderiza una demo musical con el preset seleccionado."""
    preset = PRESETS[preset_idx]
    print(f"\n🎋 Renderizando: {preset['name']}")
    print(f"   Reed: {preset['stiffness']} | Bore: {preset['bore_mm']}mm | Material: {preset['material']}")

    # Secuencia de notas (escala pentatónica del Khaen)
    # Notas tradicionales del khaen en MIDI aprox.
    sequence = [
        (60, 0.8),   # C4
        (62, 0.6),   # D4
        (64, 0.5),   # E4
        (67, 0.7),   # G4
        (69, 0.6),   # A4
        (72, 0.9),   # C5
        (71, 0.5),   # B4
        (69, 0.4),   # A4
        (67, 0.6),   # G4
        (64, 0.5),   # E4
        (62, 0.4),   # D4
        (60, 1.2),   # C4 (larga)
    ]

    chunks = []
    for i, (note, dur) in enumerate(sequence):
        print(f"   Nota {i+1}/{len(sequence)}: MIDI {note} ({dur:.1f}s)", end="\r")
        chunk = render_note(note, dur, preset, delay_on=(i % 3 == 0))
        # Pequeño silencio entre notas
        silence = np.zeros(int(0.05 * sr), dtype=np.float32)
        chunks.extend([chunk, silence])

    print()
    audio = np.concatenate(chunks)

    # Normalizar
    peak = np.max(np.abs(audio))
    if peak > 0.001:
        audio = audio / peak * 0.85

    return audio.astype(np.float32)


# ══════════════════════════════════════════════════════════════════════════════
#  GUI TKINTER (opcional)
# ══════════════════════════════════════════════════════════════════════════════

def launch_gui():
    """Lanza una GUI interactiva con sliders usando tkinter."""
    try:
        import tkinter as tk
        from tkinter import ttk
        import threading
    except ImportError:
        print("tkinter no disponible. Usa --save para exportar WAV.")
        return

    try:
        import sounddevice as sd
    except ImportError:
        print("sounddevice no instalado. Ejecuta: pip install sounddevice")
        return

    model  = FreeReedModel()
    delay  = TapeDelay()
    stream_active = [False]
    stream = [None]

    root = tk.Tk()
    root.title("🎋 Khaen — Prototipo Portable")
    root.configure(bg="#0A0A0F")
    root.geometry("700x520")
    root.resizable(True, True)

    COPPER = "#C8965A"
    BG     = "#0A0A0F"
    PANEL  = "#1A1A26"
    TEXT   = "#DDDDDD"
    DIM    = "#555566"

    style = ttk.Style()
    style.theme_use("clam")
    style.configure("TScale",     background=PANEL,  troughcolor=PANEL,  sliderlength=18)
    style.configure("TLabel",     background=PANEL,  foreground=TEXT,    font=("Segoe UI", 9))
    style.configure("Header.TLabel", background=BG,  foreground=COPPER,  font=("Segoe UI", 14, "bold"))
    style.configure("Sub.TLabel",    background=BG,  foreground=DIM,     font=("Segoe UI", 8))
    style.configure("TCombobox",     fieldbackground=PANEL, background=PANEL, foreground=TEXT)
    style.configure("TButton",       background=PANEL, foreground=COPPER, font=("Segoe UI", 9))

    # ── Header ────────────────────────────────────────────────────────────────
    hdr = tk.Frame(root, bg=BG, pady=8)
    hdr.pack(fill="x")
    ttk.Label(hdr, text="K H A E N", style="Header.TLabel").pack()
    ttk.Label(hdr, text="PHYSICAL MODEL SYNTHESIZER — PROTOTIPO PORTABLE",
              style="Sub.TLabel").pack()

    # ── Preset selector ───────────────────────────────────────────────────────
    preset_frame = tk.Frame(root, bg=BG, pady=4)
    preset_frame.pack(fill="x", padx=16)
    ttk.Label(preset_frame, text="PRESET", style="Sub.TLabel").pack(side="left")
    preset_var = tk.StringVar(value=PRESETS[0]["name"])
    preset_cb  = ttk.Combobox(preset_frame, textvariable=preset_var,
                               values=[p["name"] for p in PRESETS], width=22,
                               style="TCombobox")
    preset_cb.pack(side="left", padx=8)

    def apply_preset(event=None):
        idx = preset_cb.current()
        if 0 <= idx < len(PRESETS):
            p = PRESETS[idx]
            sv_stiff.set(p["stiffness"])
            sv_bore.set(p["bore_mm"])
            sv_press.set(p["pressure"])
            mat_var.set(p["material"])
            update_model()
    preset_cb.bind("<<ComboboxSelected>>", apply_preset)

    # ── Sliders panel ─────────────────────────────────────────────────────────
    sliders_frame = tk.Frame(root, bg=PANEL, padx=16, pady=12)
    sliders_frame.pack(fill="x", padx=16, pady=8)

    def make_slider(parent, label, var, from_, to, row, col=0, fmt=".2f"):
        ttk.Label(parent, text=label, style="TLabel").grid(row=row, column=col, sticky="w", padx=4)
        sl = ttk.Scale(parent, from_=from_, to=to, orient="horizontal",
                       variable=var, length=180, command=lambda _: update_model())
        sl.grid(row=row, column=col+1, padx=8)
        val_lbl = ttk.Label(parent, text=f"{var.get():{fmt}}", style="TLabel", width=6)
        val_lbl.grid(row=row, column=col+2, padx=4)
        def refresh_val(*a):
            val_lbl.config(text=f"{var.get():{fmt}}")
        var.trace_add("write", lambda *a: refresh_val())
        return sl

    sv_stiff = tk.DoubleVar(value=0.65)
    sv_bore  = tk.DoubleVar(value=14.0)
    sv_press = tk.DoubleVar(value=0.55)
    sv_lip   = tk.DoubleVar(value=0.30)
    sv_dfb   = tk.DoubleVar(value=0.35)
    sv_dmx   = tk.DoubleVar(value=0.25)

    sliders_frame.columnconfigure(1, weight=1)

    make_slider(sliders_frame, "Rigidez Reed",  sv_stiff, 0.01, 1.00, 0, fmt=".2f")
    make_slider(sliders_frame, "Bore Ø (mm)",   sv_bore,  8.0,  30.0, 1, fmt=".1f")
    make_slider(sliders_frame, "Presión Aire",  sv_press, 0.01, 1.00, 2, fmt=".2f")
    make_slider(sliders_frame, "Lip Damping",   sv_lip,   0.00, 1.00, 3, fmt=".2f")
    make_slider(sliders_frame, "Delay Feedback",sv_dfb,   0.00, 0.95, 4, fmt=".2f")
    make_slider(sliders_frame, "Delay Mix",     sv_dmx,   0.00, 1.00, 5, fmt=".2f")

    # Material selector
    ttk.Label(sliders_frame, text="Material", style="TLabel").grid(row=6, column=0, sticky="w", padx=4, pady=4)
    mat_var = tk.StringVar(value="Bambú")
    mat_cb  = ttk.Combobox(sliders_frame, textvariable=mat_var,
                            values=list(MATERIALS.keys()), width=18, style="TCombobox")
    mat_cb.grid(row=6, column=1, padx=8)
    mat_cb.bind("<<ComboboxSelected>>", lambda _: update_model())

    def update_model():
        model.reed_stiffness  = sv_stiff.get()
        model.bore_diameter   = sv_bore.get()
        model.blow_pressure   = sv_press.get()
        model.lip_damping     = sv_lip.get()
        model.set_material(mat_var.get())
        delay.feedback = sv_dfb.get()
        delay.mix      = sv_dmx.get()

    # ── Teclado de notas ──────────────────────────────────────────────────────
    kb_frame = tk.Frame(root, bg=BG, pady=8)
    kb_frame.pack()

    note_labels = ["C4","D4","E4","F4","G4","A4","B4","C5","D5","E5"]
    note_midis  = [60,  62,  64,  65,  67,  69,  71,  72,  74,  76 ]

    def play_note(midi_note: int):
        update_model()
        model.reset()
        model.set_pitch(midi_note)
        try:
            import sounddevice as sd
            audio = render_note(midi_note, 1.5, delay_on=True)
            sd.play(audio, SAMPLE_RATE, blocking=False)
        except Exception as e:
            print(f"Audio error: {e}")

    for i, (label, midi) in enumerate(zip(note_labels, note_midis)):
        btn = tk.Button(kb_frame, text=label, width=5, height=2,
                        bg=PANEL, fg=COPPER, relief="flat",
                        activebackground="#2A2030", activeforeground=COPPER,
                        font=("Segoe UI", 9),
                        command=lambda m=midi: play_note(m))
        btn.grid(row=0, column=i, padx=2)

    # ── Botones acción ────────────────────────────────────────────────────────
    btn_frame = tk.Frame(root, bg=BG, pady=8)
    btn_frame.pack()

    def play_demo():
        def _run():
            try:
                import sounddevice as sd
                idx   = preset_cb.current() if preset_cb.current() >= 0 else 0
                audio = render_demo(idx)
                sd.play(audio, SAMPLE_RATE, blocking=True)
            except Exception as e:
                print(f"Error reproduciendo: {e}")
        threading.Thread(target=_run, daemon=True).start()

    def save_wav():
        from tkinter import filedialog
        path = filedialog.asksaveasfilename(
            defaultextension=".wav",
            filetypes=[("WAV files", "*.wav")],
            initialfile=f"khaen_{preset_var.get().replace(' ', '_')}.wav")
        if path:
            idx   = preset_cb.current() if preset_cb.current() >= 0 else 0
            audio = render_demo(idx)
            audio_int = (audio * 32767).astype(np.int16)
            wavfile.write(path, SAMPLE_RATE, audio_int)
            print(f"✅ Guardado: {path}")

    def stop_audio():
        try:
            import sounddevice as sd
            sd.stop()
        except Exception:
            pass

    tk.Button(btn_frame, text="▶  Demo completa", bg=PANEL, fg=COPPER,
              relief="flat", font=("Segoe UI", 10), padx=12, pady=4,
              activebackground="#2A2030", command=play_demo).grid(row=0, column=0, padx=6)
    tk.Button(btn_frame, text="⏹  Stop", bg=PANEL, fg=DIM,
              relief="flat", font=("Segoe UI", 10), padx=12, pady=4,
              command=stop_audio).grid(row=0, column=1, padx=6)
    tk.Button(btn_frame, text="💾  Guardar WAV", bg=PANEL, fg=COPPER,
              relief="flat", font=("Segoe UI", 10), padx=12, pady=4,
              activebackground="#2A2030", command=save_wav).grid(row=0, column=2, padx=6)

    # Status bar
    status = tk.Label(root, text="Listo. Click en una nota para probar el sonido.",
                      bg=BG, fg=DIM, font=("Segoe UI", 8), anchor="w")
    status.pack(fill="x", padx=16, pady=4)

    root.mainloop()


# ══════════════════════════════════════════════════════════════════════════════
#  MAIN
# ══════════════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="Khaen Physical Model Synth — Prototipo")
    parser.add_argument("--save",    action="store_true", help="Guardar demo.wav y salir")
    parser.add_argument("--gui",     action="store_true", help="Lanzar GUI interactiva")
    parser.add_argument("--preset",  type=int, default=0, help="Índice de preset (0-9)")
    parser.add_argument("--list",    action="store_true", help="Listar presets")
    args = parser.parse_args()

    if args.list:
        print("\n🎋 Presets disponibles:")
        for i, p in enumerate(PRESETS):
            print(f"  [{i:2d}] {p['name']:<20} "
                  f"Reed:{p['stiffness']:.2f} Bore:{p['bore_mm']:4.0f}mm {p['material']}")
        return

    if args.gui:
        launch_gui()
        return

    # CLI mode
    print("\n[KHAEN] Physical Model Synthesizer -- Prototipo Python")
    print("=" * 55)

    preset_idx = max(0, min(9, args.preset))
    audio = render_demo(preset_idx)

    if args.save:
        out_path = f"khaen_{PRESETS[preset_idx]['name'].replace(' ', '_')}.wav"
        audio_int = (audio * 32767).astype(np.int16)
        wavfile.write(out_path, SAMPLE_RATE, audio_int)
        print(f"\n[OK] Guardado: {out_path}  ({len(audio)/SAMPLE_RATE:.1f}s, {SAMPLE_RATE}Hz)")
        return

    # Reproducir en tiempo real
    try:
        import sounddevice as sd
        print(f"\n[>] Reproduciendo con sounddevice... (Ctrl+C para parar)")
        sd.play(audio, SAMPLE_RATE, blocking=True)
        print("[OK] Reproduccion completa.")
    except ImportError:
        # Fallback: guardar WAV automaticamente
        out_path = "khaen_demo.wav"
        audio_int = (audio * 32767).astype(np.int16)
        wavfile.write(out_path, SAMPLE_RATE, audio_int)
        print(f"\n[!] sounddevice no instalado. WAV guardado en: {out_path}")
        print(f"    Instala con: pip install sounddevice")
        print(f"    O abre {out_path} con cualquier reproductor.")


if __name__ == "__main__":
    main()
