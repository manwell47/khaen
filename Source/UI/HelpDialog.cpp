#include "HelpDialog.h"

//==============================================================================
namespace
{
    static const juce::Colour kBg        = juce::Colour(0xF00A0A0F);
    static const juce::Colour kPanel     = juce::Colour(0xFF14141E);
    static const juce::Colour kCopper    = juce::Colour(0xFFC8965A);
    static const juce::Colour kCopperDim = juce::Colour(0xFF8B6914);
    static const juce::Colour kGold      = juce::Colour(0xFFE8C97A);
    static const juce::Colour kCyan      = juce::Colour(0xFF00E5FF);
    static const juce::Colour kTextPrim  = juce::Colour(0xFFE0E0E8);
    static const juce::Colour kTextDim   = juce::Colour(0xFF9A9AAC);
    static const juce::Colour kBorder    = juce::Colour(0xFF2B2B3C);
}

//==============================================================================
HelpDialog::HelpDialog()
{
    addAndMakeVisible(m_closeBtn);
    m_closeBtn.addListener(this);
    m_closeBtn.setColour(juce::TextButton::buttonColourId,   kPanel.brighter(0.1f));
    m_closeBtn.setColour(juce::TextButton::textColourOffId,  kCopper);

    addAndMakeVisible(m_tabIntro);
    addAndMakeVisible(m_tabPerform);
    addAndMakeVisible(m_tabParams);
    addAndMakeVisible(m_tabFxVis);

    m_tabIntro.addListener(this);
    m_tabPerform.addListener(this);
    m_tabParams.addListener(this);
    m_tabFxVis.addListener(this);

    addAndMakeVisible(m_contentArea);
    m_contentArea.setMultiLine(true);
    m_contentArea.setReadOnly(true);
    m_contentArea.setCaretVisible(false);

    m_contentArea.setColour(juce::TextEditor::backgroundColourId, kPanel);
    m_contentArea.setColour(juce::TextEditor::textColourId,       kTextPrim);
    m_contentArea.setColour(juce::TextEditor::outlineColourId,    kBorder);
    m_contentArea.setColour(juce::TextEditor::shadowColourId,     juce::Colours::transparentBlack);

    switchSection(0);
}

HelpDialog::~HelpDialog()
{
    m_closeBtn.removeListener(this);
    m_tabIntro.removeListener(this);
    m_tabPerform.removeListener(this);
    m_tabParams.removeListener(this);
    m_tabFxVis.removeListener(this);
}

//==============================================================================
void HelpDialog::buttonClicked (juce::Button* b)
{
    if (b == &m_closeBtn)
    {
        if (m_closeCallback != nullptr)
            m_closeCallback();
    }
    else if (b == &m_tabIntro)   switchSection(0);
    else if (b == &m_tabPerform) switchSection(1);
    else if (b == &m_tabParams)  switchSection(2);
    else if (b == &m_tabFxVis)   switchSection(3);
}

void HelpDialog::switchSection (int idx)
{
    m_activeSection = idx;
    m_tabIntro.setToggleState(idx == 0, juce::dontSendNotification);
    m_tabPerform.setToggleState(idx == 1, juce::dontSendNotification);
    m_tabParams.setToggleState(idx == 2, juce::dontSendNotification);
    m_tabFxVis.setToggleState(idx == 3, juce::dontSendNotification);

    auto styleTab = [](juce::TextButton& btn, bool active)
    {
        btn.setColour(juce::TextButton::buttonColourId,  active ? kCopper.withAlpha(0.25f) : kPanel);
        btn.setColour(juce::TextButton::textColourOffId, active ? kGold : kTextDim);
    };

    styleTab(m_tabIntro,   idx == 0);
    styleTab(m_tabPerform, idx == 1);
    styleTab(m_tabParams,  idx == 2);
    styleTab(m_tabFxVis,   idx == 3);

    updateTextContent();
}

void HelpDialog::updateTextContent()
{
    juce::String t;

    if (m_activeSection == 0)
    {
        t << "===============================================================================\n"
          << "                   KHAEN - SINTETIZADOR DE MODELADO FISICO                     \n"
          << "===============================================================================\n\n"
          << "1. INTRODUCCION AL KHAEN\n"
          << "-------------------------\n"
          << "El Khaen es el organo de boca tradicional de lengueta libre originario de Laos\n"
          << "y de la region del Isan (Tailandia). Construido artesanalmente uniendo catorce o dieciseis\n"
          << "tubos de bambu con una camara de aire central (Tao) esculpida en madera de frondosas, el\n"
          << "instrumento es una joya acustica Patrimonio Cultural Inmaterial de la Humanidad por la UNESCO.\n\n"
          << "2. MODELADO FISICO SINTETICO (IWK VIENA / MDW RESEARCH)\n"
          << "--------------------------------------------------------\n"
          << "A diferencia de los muestreadores convencionales (samplers), KHAEN es un sintetizador de\n"
          << "modelado fisico puro en tiempo real que simula el acoplamiento aeroelastico no lineal entre\n"
          << "las lenguetas de bronce (Free Reeds) y la resonancia de la columna de aire en los tubos de bambu:\n\n"
          << " * Ecuaciones no lineales de flujo de masa a traves del area de abertura de la lengueta.\n"
          << " * Modelo de impacto Hunt-Crossley para el choque y amortiguacion de lengueta (Slap Tongue).\n"
          << " * Guias de onda digitales (Digital Waveguides) con perdidas viscotermicas en los tubos.\n"
          << " * Simulacion de cavidad vocal y garganta del interprete (Vocal Tract Formant Filtering).\n\n"
          << "El resultado es una expresividad dinamica viva donde la presion de respiracion, la tecnica\n"
          << "de mordida y las articulaciones responden de forma continua al teclado y controladores MIDI.\n";
    }
    else if (m_activeSection == 1)
    {
        t << "===============================================================================\n"
          << "                      GUIA DE INTERPRETACION Y KEYSWITCHES                     \n"
          << "===============================================================================\n\n"
          << "1. DINAMICA DE RESPIRACION Y POLIFONIA\n"
          << "----------------------------------------\n"
          << "El Khaen real produce sonido tanto al exhalar (soplar) como al inhalar (aspirar). KHAEN\n"
          << "soporta polifonia completa y libre resoplado:\n\n"
          << " * Soplado Exhalado (Blow): Produce el ataque calido y brillante de melodia individual.\n"
          << " * Succion Inhalada (Inhale): Genera un flujo de aire continuo que sostiene acordes de dron.\n\n"
          << "2. KEYSWITCHES DE DINAMICA (NOTAS BAJAS C1 Y D1)\n"
          << "--------------------------------------------------\n"
          << "Puedes controlar el flujo de aire y la respiracion del interprete utilizando las notas MIDI:\n\n"
          << " * MIDI C1 (Nota 24): Keyswitch de Exhalacion (Blow Pulse / Accent).\n"
          << " * MIDI D1 (Nota 26): Keyswitch de Inhalacion (Suction Pulse / Continuous Drone Hold).\n\n"
          << "3. MODOS DE ARTICULACION\n"
          << "-------------------------\n"
          << " * Legato: Transiciones suaves sin re-ataque brusco de lengueta.\n"
          << " * Portato: Ataque articulado con leve separacion respiratoria entre notas.\n"
          << " * Staccato: Notas cortas con amortiguacion rapida por contacto de lengua.\n"
          << " * Slap Tongue: Ataque percusivo fuerte simulando el chasquido seco de lengueta contra el canal.\n\n"
          << "4. FUNCION AUTO-RIFF\n"
          << "---------------------\n"
          << "Al activar 'AUTO-RIFF', el motor sintetiza de forma autonoma los trinos y adornos rapidos\n"
          << "tipicos del estilo Lao Molam cuando se mantienen notas melodicas junto a notas de dron.\n";
    }
    else if (m_activeSection == 2)
    {
        t << "===============================================================================\n"
          << "                     MANUAL DE INSTRUCCIONES DE PARAMETROS                     \n"
          << "===============================================================================\n\n"
          << "1. CONTROLES MACRO (PANEL IZQUIERDO)\n"
          << "-------------------------------------\n"
          << " * AIR (Macro Air): Controla el flujo global de aire, la fuerza de presion y el soplado.\n"
          << " * EMBOUCHURE (Embocadura): Ajusta el sellado de los labios y la apertura del canal de aire.\n"
          << " * RESONANCE (Resonancia): Controla la reflectividad acustica interna de las columnas de bambu.\n\n"
          << "2. MODELO FISICO Y FISICA DE REED (PANEL DERECHO)\n"
          << "--------------------------------------------------\n"
          << " * STIFFNESS (0.00 - 1.00): Rigidez de la lengueta de bronce. Valores altos aumentan la agudeza.\n"
          << " * BORE DIAMETER (8.0 - 30.0 mm): Diametro interno del tubo de bambu. Cambia el timbre y volumen.\n"
          << " * BREATH SPEED (0.20 - 3.00): Velocidad de reaccion del aire ante cambios de presion MIDI.\n"
          << " * SUCTION BITE (0.00 - 1.00): Intensidad del chasquido al iniciar la succion de inhalacion.\n"
          << " * VOCAL TRACT (0.00 - 1.00): Modulacion de formantes de la cavidad bucal del interprete.\n"
          << " * TONGUE TIME (10.0 - 100.0 ms): Tiempo de amortiguacion en ataques staccato y slap.\n"
          << " * VELOCITY SENS (0.00 - 1.00): Sensibilidad dinamica a la velocidad de pulsacion MIDI.\n"
          << " * MATERIAL: Bambu, Bambu anejado, Jade, Laton, PVC, Madera oscura o Metal.\n"
          << " * ARTICULATION: Legato, Portato, Staccato o Slap Tongue.\n\n"
          << "3. CONTROL MASTER\n"
          << "------------------\n"
          << " * MASTER VOLUME (-24.0 a +6.0 dB): Nivel de salida principal del plugin.\n";
    }
    else if (m_activeSection == 3)
    {
        t << "===============================================================================\n"
          << "                     RACK DE EFECTOS Y VISUALIZADOR 2D                         \n"
          << "===============================================================================\n\n"
          << "1. TAPE DELAY (DELAY DE CINTA ANALOGICO)\n"
          << "-----------------------------------------\n"
          << " * TIME / SYNC / DIVISION: Tiempo de retardo en ms o sincronizado al tempo del host.\n"
          << " * TAP TEMPO: Boton para ajustar manualmente el tempo ritmico.\n"
          << " * FEEDBACK & MIX: Realimentacion de ecos y mezcla seco/mojado.\n"
          << " * WOW & FLUTTER: Modulacion de cinta analogica para fluidez armonica lo-fi.\n"
          << " * PING-PONG & BYPASS: Alternancia estereo entre canales y bypass directo.\n\n"
          << "2. HARMONIZER (VOCES ARMONICAS)\n"
          << "---------------------------------\n"
          << " * SHIFT (-24 a +24 st): Transposicion de intervalo musical en formato ingles (+7st, +12st, etc.).\n"
          << " * DETUNE & VOICES: Desafinacion fina en cents y numero de voces armonicas.\n"
          << " * MIX: Balance de mezcla del armonizador.\n\n"
          << "3. CRYSTALLIZER (GRAIN PITCH-SHIFTER & REVERSE)\n"
          << "------------------------------------------------\n"
          << " * GRAIN MS & DELAY MS: Tamano de los granulos de audio y retardo inicial.\n"
          << " * PITCH SHIFT & FREEZE: Transposicion de granulos y congelado infinito de textura.\n\n"
          << "4. LAOS REVERB (5 ESPACIOS MISTICOS DE LAOS)\n"
          << "---------------------------------------------\n"
          << " * Presets: Vang Vieng Cave, Pha That Luang, Mekong River, Luang Prabang Temple, Wat Phou.\n"
          << " * SIZE, DECAY, DAMP, WIDTH, MIX: Tamano, tiempo de reverberacion, amortiguamiento y ancho.\n\n"
          << "5. MODOS DEL VISUALIZADOR\n"
          << "--------------------------\n"
          << " * 3D MODAL: Cascada de respuesta espectral en frecuencia CQT.\n"
          << " * AURORA RTA: Espectrograma continuo con seguimiento de formantes F1, F2 y F3.\n"
          << " * AERO-VECTORIAL: Siluetas 2D de tubos con brillo neon respiratorio y oscilacion de pared.\n"
          << " * REED MATRIX: Vista isometrica 2D del windchest con halos neon de presion por nota.\n";
    }

    m_contentArea.setText(t, false);
}

//==============================================================================
void HelpDialog::paint (juce::Graphics& g)
{
    // Backdrop dark overlay
    g.fillAll(kBg);

    auto b = getLocalBounds().reduced(24, 20);

    // Dialog background panel
    g.setColour(kPanel);
    g.fillRoundedRectangle(b.toFloat(), 10.0f);

    // Panel border outline
    g.setColour(kCopper.withAlpha(0.6f));
    g.drawRoundedRectangle(b.toFloat(), 10.0f, 1.5f);

    // Title area
    auto titleArea = b.removeFromTop(44);
    g.setFont(juce::Font(juce::FontOptions().withHeight(17.0f).withStyle("Bold")));
    g.setColour(kGold);
    g.drawText("KHAEN - MANUAL DE USUARIO & GUIA DE INSTRUCCIONES",
               titleArea.reduced(16, 0), juce::Justification::centredLeft, false);

    // Subtitle line
    g.setColour(kCopperDim.withAlpha(0.5f));
    g.drawHorizontalLine(titleArea.getBottom() - 2, (float)b.getX(), (float)b.getRight());
}

void HelpDialog::resized()
{
    auto b = getLocalBounds().reduced(24, 20);

    auto titleArea = b.removeFromTop(44);
    m_closeBtn.setBounds(titleArea.removeFromRight(36).reduced(4, 8));

    b.removeFromTop(6);

    // Tabs area
    auto tabArea = b.removeFromTop(32);
    int tabW = tabArea.getWidth() / 4;
    m_tabIntro.setBounds(tabArea.removeFromLeft(tabW).reduced(2, 2));
    m_tabPerform.setBounds(tabArea.removeFromLeft(tabW).reduced(2, 2));
    m_tabParams.setBounds(tabArea.removeFromLeft(tabW).reduced(2, 2));
    m_tabFxVis.setBounds(tabArea.reduced(2, 2));

    b.removeFromTop(8);

    // Content text editor area
    m_contentArea.setBounds(b.reduced(10, 10));
}
