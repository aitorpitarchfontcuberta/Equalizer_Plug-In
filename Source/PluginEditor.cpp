/*
  ==============================================================================
    PluginEditor.cpp
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
//  Sistema de SKINS  (Standard / Dark / Light)
//==============================================================================
//  Cada skin es un set completo de colores. La GUI consulta getSkin() en cada
//  paint() y dibuja con esos valores. Cambiar de skin = setCurrentSkin(id) +
//  repintar el editor: no hay que tocar la lógica del audio ni los parámetros.
//==============================================================================
struct Skin
{
    // --- Editor (fondo principal y separadores entre las 5 secciones) ---
    juce::Colour pluginBg;
    juce::Colour sectionSeparator;
    juce::Colour topBarBg;        // fondo de la franja superior (botones)
    juce::Colour topBarSeparator; // línea fina bajo la franja

    // --- Área del gráfico ---
    juce::Colour graphBg;
    juce::Colour gridMajor;       // 100 / 1k / 10k
    juce::Colour gridMinor;       // resto de líneas verticales
    juce::Colour gridLabelMajor;
    juce::Colour gridLabelMinor;
    juce::Colour gridHorizontal;  // líneas dB horizontales
    juce::Colour axisLabel;       // texto -54 / -36 / +12 / 0 ...

    // --- Espectro y curva ---
    juce::Colour fftFill;         // relleno del FFT (con alpha)
    juce::Colour fftStroke;       // contorno del FFT
    juce::Colour filterCurve;     // curva blanca/negra/cream

    // --- Puntos y peaks ---
    juce::Colour peak1, peak2, peak3;

    // --- Knobs ---
    juce::Colour knobBg;
    juce::Colour knobBorder;
    juce::Colour knobTrack;
    juce::Colour knobDefault;     // acento de LowCut/HighCut (no-peak)
    juce::Colour knobBypass;
    juce::Colour knobIndicator;
    juce::Colour knobValueText;
    juce::Colour knobLabelText;

    // --- Controles JUCE (combos, toggles, botones, popup) ---
    juce::Colour controlBg;          // fondo de combos/botones
    juce::Colour controlText;        // texto en controles y label "Skin"
    juce::Colour controlOutline;     // borde / "relieve oscuro"
    juce::Colour controlAccent;      // tick del toggle, flecha del combo
    juce::Colour popupBg;            // fondo del popup desplegado
    juce::Colour popupText;          // texto del popup
    juce::Colour popupHighlight;     // fondo de la opción resaltada
    juce::Colour popupHighlightText; // texto de la opción resaltada
};

// ---------- Standard (look actual: Studio dark) -----------------------------
static const Skin kSkinStandard =
{
    juce::Colour(18, 18, 22),       // pluginBg
    juce::Colour(75, 75, 85),       // sectionSeparator
    juce::Colour(26, 26, 32),       // topBarBg (un punto más claro)
    juce::Colour(58, 58, 68),       // topBarSeparator
    juce::Colours::black,           // graphBg
    juce::Colours::dimgrey,         // gridMajor
    juce::Colours::dimgrey,         // gridMinor
    juce::Colours::white,           // gridLabelMajor
    juce::Colours::lightgrey,       // gridLabelMinor
    juce::Colours::dimgrey,         // gridHorizontal
    juce::Colours::lightgrey,       // axisLabel
    juce::Colours::cyan,            // fftFill
    juce::Colours::cyan,            // fftStroke
    juce::Colours::white,           // filterCurve
    juce::Colour(220, 80, 80),      // peak1 rojo
    juce::Colour(100, 200, 100),    // peak2 verde
    juce::Colour(220, 180, 60),     // peak3 amarillo
    juce::Colour(35, 35, 40),       // knobBg
    juce::Colour(75, 75, 85),       // knobBorder
    juce::Colour(55, 55, 60),       // knobTrack
    juce::Colour(0, 175, 215),      // knobDefault cyan
    juce::Colour(80, 80, 90),       // knobBypass
    juce::Colour(210, 210, 210),    // knobIndicator
    juce::Colour(200, 200, 200),    // knobValueText
    juce::Colour(120, 120, 130),    // knobLabelText
    juce::Colour(35, 35, 40),       // controlBg
    juce::Colour(200, 200, 200),    // controlText
    juce::Colour(75, 75, 85),       // controlOutline
    juce::Colour(0, 175, 215),      // controlAccent cyan
    juce::Colour(35, 35, 40),       // popupBg
    juce::Colour(200, 200, 200),    // popupText
    juce::Colour(0, 175, 215),      // popupHighlight
    juce::Colours::white            // popupHighlightText
};

// ---------- Dark (Vintage analog: madera oscura + latón) --------------------
static const Skin kSkinDark =
{
    juce::Colour(0x2a, 0x1f, 0x15), // pluginBg madera
    juce::Colour(0x7a, 0x5a, 0x30), // sectionSeparator marrón
    juce::Colour(0x1d, 0x16, 0x0e), // topBarBg madera oscura
    juce::Colour(0x7a, 0x5a, 0x30), // topBarSeparator marrón
    juce::Colour(0x15, 0x10, 0x0a), // graphBg muy oscuro
    juce::Colour(0x9a, 0x70, 0x3c), // gridMajor marrón claro
    juce::Colour(0x6a, 0x4a, 0x28), // gridMinor marrón medio
    juce::Colour(0xf0, 0xd9, 0xa8), // gridLabelMajor crema
    juce::Colour(0xc4, 0xa6, 0x78), // gridLabelMinor crema apagado
    juce::Colour(0x6a, 0x4a, 0x28), // gridHorizontal
    juce::Colour(0xd4, 0xba, 0x8a), // axisLabel
    juce::Colour(0xc8, 0x86, 0x2c), // fftFill cobre
    juce::Colour(0xe8, 0xa0, 0x4b), // fftStroke cobre claro
    juce::Colour(0xf0, 0xd9, 0xa8), // filterCurve crema
    juce::Colour(0xb3, 0x40, 0x2a), // peak1 terracota
    juce::Colour(0x7a, 0x8e, 0x3c), // peak2 sage
    juce::Colour(0xc8, 0x95, 0x18), // peak3 mostaza
    juce::Colour(0x1d, 0x16, 0x0e), // knobBg
    juce::Colour(0xc9, 0xa9, 0x61), // knobBorder latón
    juce::Colour(0x4a, 0x35, 0x1c), // knobTrack
    juce::Colour(0xc9, 0xa9, 0x61), // knobDefault latón
    juce::Colour(0x6a, 0x55, 0x35), // knobBypass marrón apagado
    juce::Colour(0xf0, 0xd9, 0xa8), // knobIndicator crema
    juce::Colour(0xd4, 0xba, 0x8a), // knobValueText
    juce::Colour(0xa6, 0x8a, 0x60), // knobLabelText
    juce::Colour(0x1d, 0x16, 0x0e), // controlBg madera oscura
    juce::Colour(0xd4, 0xba, 0x8a), // controlText crema
    juce::Colour(0xc9, 0xa9, 0x61), // controlOutline latón
    juce::Colour(0xc9, 0xa9, 0x61), // controlAccent latón
    juce::Colour(0x2a, 0x1f, 0x15), // popupBg
    juce::Colour(0xd4, 0xba, 0x8a), // popupText
    juce::Colour(0xc9, 0xa9, 0x61), // popupHighlight latón
    juce::Colour(0x2a, 0x1f, 0x15)  // popupHighlightText (madera sobre latón)
};

// ---------- Light (Material restrained: blanco + 3 acentos vivos) -----------
static const Skin kSkinLight =
{
    juce::Colour(0xf6, 0xf5, 0xf1), // pluginBg off-white cálido
    juce::Colour(0xc8, 0xc5, 0xbc), // sectionSeparator
    juce::Colour(0xec, 0xeb, 0xe5), // topBarBg algo más gris
    juce::Colour(0xc8, 0xc5, 0xbc), // topBarSeparator
    juce::Colour(0xf0, 0xef, 0xe9), // graphBg
    juce::Colour(0x88, 0x86, 0x80), // gridMajor gris medio
    juce::Colour(0xc8, 0xc5, 0xbc), // gridMinor gris claro
    juce::Colour(0x2a, 0x2a, 0x26), // gridLabelMajor casi negro
    juce::Colour(0x6a, 0x68, 0x60), // gridLabelMinor
    juce::Colour(0xc8, 0xc5, 0xbc), // gridHorizontal
    juce::Colour(0x2a, 0x2a, 0x26), // axisLabel
    juce::Colour(0x5a, 0x58, 0x54), // fftFill gris oscuro
    juce::Colour(0x2a, 0x2a, 0x26), // fftStroke
    juce::Colour(0x0a, 0x0a, 0x0a), // filterCurve negro
    juce::Colour(0xd3, 0x2f, 0x2f), // peak1 material red 700
    juce::Colour(0x38, 0x8e, 0x3c), // peak2 material green 700
    juce::Colour(0xf5, 0x7c, 0x00), // peak3 material orange 700
    juce::Colour(0xff, 0xff, 0xff), // knobBg blanco
    juce::Colour(0xc8, 0xc5, 0xbc), // knobBorder gris claro
    juce::Colour(0xe4, 0xe2, 0xdc), // knobTrack
    juce::Colour(0x2a, 0x2a, 0x2a), // knobDefault casi negro
    juce::Colour(0xb0, 0xae, 0xa8), // knobBypass
    juce::Colour(0x2a, 0x2a, 0x26), // knobIndicator
    juce::Colour(0x2a, 0x2a, 0x26), // knobValueText
    juce::Colour(0x6a, 0x68, 0x60), // knobLabelText
    juce::Colour(0xff, 0xff, 0xff), // controlBg blanco
    juce::Colour(0x2a, 0x2a, 0x26), // controlText oscuro
    juce::Colour(0x4a, 0x48, 0x44), // controlOutline relieve oscuro
    juce::Colour(0x2a, 0x2a, 0x26), // controlAccent oscuro (tick visible)
    juce::Colour(0xff, 0xff, 0xff), // popupBg blanco
    juce::Colour(0x2a, 0x2a, 0x26), // popupText oscuro
    juce::Colour(0xd0, 0xd6, 0xe0), // popupHighlight gris-azulado claro
    juce::Colour(0x2a, 0x2a, 0x26)  // popupHighlightText oscuro
};

enum class SkinId { Standard = 0, Dark = 1, Light = 2 };

// Puntero global al skin activo. Solo se modifica desde el GUI thread
// (callback del ComboBox) y solo se lee desde el GUI thread (paint()).
static const Skin* currentSkin   = &kSkinStandard;
static SkinId      currentSkinId = SkinId::Standard;

inline const Skin& getSkin() { return *currentSkin; }

static void setCurrentSkin(SkinId id)
{
    switch (id)
    {
        case SkinId::Standard: currentSkin = &kSkinStandard; break;
        case SkinId::Dark:     currentSkin = &kSkinDark;     break;
        case SkinId::Light:    currentSkin = &kSkinLight;    break;
    }
    currentSkinId = id;
}

//==============================================================================
//  Helpers de layout  (frecuencia / posición X logarítmica)
//==============================================================================

static float mapXToFreq(float x, float leftX, float width)
{
    static const float logMin = std::log10(20.0f);
    static const float logMax = std::log10(20000.0f);
    float normalizedX = (x - leftX) / width;
    return std::pow(10.0f, logMin + normalizedX * (logMax - logMin));
}

static float mapFreqToX(float freq, float leftX, float width)
{
    // Eje X: log10(20..20000) → 0..width
    static const float logMin = std::log10(20.0f);
    static const float logMax = std::log10(20000.0f);
    float logFreq = std::log10(juce::jlimit(20.0f, 20000.0f, freq));
    return leftX + width * (logFreq - logMin) / (logMax - logMin);
}

// Inverso del mapeo dB→Y de updateFilterCurve():
//   y = jmap(dB, 24, -24, graphTop, graphBottom)
// Por tanto:
//   dB = jmap(y, graphTop, graphBottom, 24, -24)
static float mapYToGain(float y, float graphTop, float graphBottom)
{
    return juce::jmap(y, graphTop, graphBottom, 24.0f, -24.0f);
}

// Tabla con los nombres de los parámetros para cada peak (0,1,2),
// para que los handlers del ratón no se llenen de switch/if.
static const char* const kPeakParamNames[3][3] =
{
    { "Peak Frequency",  "Peak Gain",  "Peak Quality"  },
    { "Peak2 Frequency", "Peak2 Gain", "Peak2 Quality" },
    { "Peak3 Frequency", "Peak3 Gain", "Peak3 Quality" }
};

//==============================================================================
//  RotarySliderWithLabels — LookAndFeel
//==============================================================================
void RotarySliderWithLabels::LookAndFeel::drawRotarySlider(
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider)
{
    using namespace juce;
    const auto& s = getSkin();

    // Reservamos 16px abajo para el nombre del parámetro
    auto labelH = 16;
    auto bounds = Rectangle<float>(x, y, width, height - labelH);
    auto radius = jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto center = bounds.getCentre();

    // Determinar el color según el tipo de slider (lee del skin actual)
    Colour accentColor = s.knobDefault;
    bool isBypassed = false;

    if (auto* rsw = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        isBypassed = rsw->isBypassed;

        if (isBypassed)
        {
            accentColor = s.knobBypass;
        }
        else
        {
            switch (rsw->sliderType)
            {
                case RotarySliderWithLabels::Peak1: accentColor = s.peak1; break;
                case RotarySliderWithLabels::Peak2: accentColor = s.peak2; break;
                case RotarySliderWithLabels::Peak3: accentColor = s.peak3; break;
                default: break;
            }
        }
    }

    // --- Fondo del knob ---
    g.setColour(s.knobBg);
    g.fillEllipse(bounds.withSizeKeepingCentre(radius * 2.0f, radius * 2.0f));

    // --- Borde exterior ---
    g.setColour(s.knobBorder);
    g.drawEllipse(bounds.withSizeKeepingCentre(radius * 2.0f, radius * 2.0f), 1.5f);

    // --- Arco de recorrido total ---
    auto trackR = radius - 5.0f;
    Path arcBg;
    arcBg.addCentredArc(center.x, center.y, trackR, trackR,
        0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(s.knobTrack);
    g.strokePath(arcBg, PathStrokeType(3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    // --- Arco de valor (color según tipo de slider) ---
    float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    Path arcVal;
    arcVal.addCentredArc(center.x, center.y, trackR, trackR,
        0.0f, rotaryStartAngle, angle, true);

    if (isBypassed) g.setColour(accentColor.withAlpha(0.3f));
    else            g.setColour(accentColor);

    g.strokePath(arcVal, PathStrokeType(3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    // --- Línea indicadora (desde centro hasta borde interior) ---
    auto thumbR = radius - 11.0f;
    Point<float> thumbPt(center.x + thumbR * std::sin(angle),
        center.y - thumbR * std::cos(angle));
    g.setColour(s.knobIndicator);
    g.drawLine(Line<float>(center, thumbPt), 2.0f);

    // --- Punto central (color según tipo de slider) ---
    if (isBypassed) g.setColour(accentColor.withAlpha(0.3f));
    else            g.setColour(accentColor);

    g.fillEllipse(Rectangle<float>(5.0f, 5.0f).withCentre(center));

    // --- Valor numérico (centrado en el knob) ---
    if (auto* rsw = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        if (isBypassed) g.setColour(s.knobLabelText.withAlpha(0.4f));
        else            g.setColour(s.knobValueText);

        g.setFont(FontOptions(10.5f));
        g.drawFittedText(rsw->getDisplayString(),
            bounds.withSizeKeepingCentre(radius * 1.4f, radius * 0.6f).toNearestInt(),
            Justification::centred, 1);

        // --- Nombre del parámetro debajo del knob ---
        if (isBypassed) g.setColour(s.knobLabelText.withAlpha(0.4f));
        else            g.setColour(s.knobLabelText);

        g.setFont(FontOptions(10.0f));
        auto labelBounds = Rectangle<int>(x, y + height - labelH, width, labelH);
        g.drawFittedText(rsw->param->getName(24), labelBounds, Justification::centred, 1);
    }
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choice->getCurrentChoiceName();

    juce::String str;
    bool addK = false;
    float val = (float)getValue();

    if (std::abs(val) >= 1000.0f)
    {
        val /= 1000.0f;
        addK = true;
    }

    // 2 decimales si < 10, 1 si < 100, 0 en adelante
    int decimals = (std::abs(val) < 10.0f) ? 2 : ((std::abs(val) < 100.0f) ? 1 : 0);
    str = juce::String(val, decimals);
    if (addK)  str << "k";
    if (suffix.isNotEmpty()) str << " " << suffix;
    return str;
}

//==============================================================================
//  ResponseCurveComponent
//==============================================================================
ResponseCurveComponent::ResponseCurveComponent(EQAudioProcessor& p)
    : audioProcessor(p),
    leftSpectrumAnalyzer(p.leftChannelFifo)
{
    // Suscribirse a cambios de parámetros
    const auto& params = audioProcessor.getParameters();
    for (auto* param : params)
        param->addListener(this);

    updateFilterCurve();
    startTimerHz(60);  // Timer para repintar cuando cambia un parámetro
}

ResponseCurveComponent::~ResponseCurveComponent()
{
    const auto& params = audioProcessor.getParameters();
    for (auto* param : params)
        param->removeListener(this);
}

void ResponseCurveComponent::parameterValueChanged(int, float)
{
    parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback()
{
    // Procesamos el FFT con el sampleRate real del procesador
    leftSpectrumAnalyzer.process(audioProcessor.getSampleRate());

    if (parametersChanged.compareAndSetBool(false, true))
        updateFilterCurve();

    repaint();
}

void ResponseCurveComponent::updateFilterCurve()
{
    // -------------------------------------------------------------------
    //  recalculamos los coeficientes DESDE EL APVTS,
    //  sin tocar los objetos de filtro del audio thread.
    //  Así evitamos cualquier data-race.
    // -------------------------------------------------------------------
    auto chainSettings = getChainSettings(audioProcessor.apvts);
    double sampleRate = audioProcessor.getSampleRate();
    if (sampleRate <= 0.0) sampleRate = 44100.0;

    auto bounds = getLocalBounds().toFloat();
    auto width = bounds.getWidth();

    // Aplicamos el mismo margen inferior que en paint() para que la curva se dibuje en la misma área
    float graphHeight = bounds.getHeight() - BOTTOM_MARGIN;
    // Asegurarse de que graphHeight nunca sea negativo (evita assertion en jlimit)
    if (graphHeight < 1.0f) graphHeight = 1.0f;
    float graphTop = bounds.getY();
    float graphBottom = graphTop + graphHeight;

    filterCurvePath.clear();

    // Para cada píxel X calculamos la magnitud total de la cadena de filtros
    for (int px = 0; px < (int)width; ++px)
    {
        double mag = 1.0;

        // Frecuencia correspondiente a este píxel (escala log)
        float freq = std::pow(10.0f,
            juce::jmap((float)px, 0.0f, width - 1,
                std::log10(20.0f), std::log10(20000.0f)));

        // --- Peak filter --- (solo si NO está en bypass)
        if (!chainSettings.peakBypass)
        {
            auto peakCoeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, chainSettings.peakFreq, chainSettings.peakQuality,
                juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
            mag *= peakCoeff->getMagnitudeForFrequency(freq, sampleRate);
        }

        // --- Peak2 filter --- (solo si NO está en bypass)
        if (!chainSettings.peak2Bypass)
        {
            auto peak2Coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, chainSettings.peak2Freq, chainSettings.peak2Quality,
                juce::Decibels::decibelsToGain(chainSettings.peak2GainInDecibels));
            mag *= peak2Coeff->getMagnitudeForFrequency(freq, sampleRate);
        }

        // --- Peak3 filter --- (solo si NO está en bypass)
        if (!chainSettings.peak3Bypass)
        {
            auto peak3Coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, chainSettings.peak3Freq, chainSettings.peak3Quality,
                juce::Decibels::decibelsToGain(chainSettings.peak3GainInDecibels));
            mag *= peak3Coeff->getMagnitudeForFrequency(freq, sampleRate);
        }

        // --- LowCut filter --- (solo si NO está en bypass)
        if (!chainSettings.lowCutBypass)
        {
            auto lowCutCoeffs = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
                chainSettings.lowCutFreq, sampleRate, 2 * (chainSettings.lowCutSlope + 1));
            for (int stage = 0; stage <= chainSettings.lowCutSlope; ++stage)
                mag *= lowCutCoeffs[stage]->getMagnitudeForFrequency(freq, sampleRate);
        }

        // --- HighCut filter --- (solo si NO está en bypass)
        if (!chainSettings.highCutBypass)
        {
            auto highCutCoeffs = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
                chainSettings.highCutFreq, sampleRate, 2 * (chainSettings.highCutSlope + 1));
            for (int stage = 0; stage <= chainSettings.highCutSlope; ++stage)
                mag *= highCutCoeffs[stage]->getMagnitudeForFrequency(freq, sampleRate);
        }

        // Convierte magnitud a dB y mapea al eje Y (24..−24 dB range)
        float dB = (float)juce::Decibels::gainToDecibels(mag);
        float y = juce::jmap(dB, 24.0f, -24.0f, graphTop, graphBottom);

        // Climpear y para asegurar que siempre está dentro del área gráfica visible
        y = juce::jlimit(graphTop, graphBottom, y);

        if (px == 0)
            filterCurvePath.startNewSubPath(bounds.getX(), y);
        else
            filterCurvePath.lineTo(bounds.getX() + px, y);
    }

    // ===================================================================
    // Calcular las posiciones de los puntos de los peaks en la curva
    // ===================================================================

    // Peak 1 (100Hz)
    if (!chainSettings.peakBypass)
    {
        float peakFreq = chainSettings.peakFreq;
        float peakPx = juce::jmap(std::log10(peakFreq), std::log10(20.0f), std::log10(20000.0f), 0.0f, width - 1);

        double peakMag = 1.0;
        // Calcular la magnitud en esta frecuencia considerando todos los filtros
        auto peakCoeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, chainSettings.peakFreq, chainSettings.peakQuality,
            juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
        peakMag *= peakCoeff->getMagnitudeForFrequency(peakFreq, sampleRate);

        if (!chainSettings.peak2Bypass)
        {
            auto peak2Coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, chainSettings.peak2Freq, chainSettings.peak2Quality,
                juce::Decibels::decibelsToGain(chainSettings.peak2GainInDecibels));
            peakMag *= peak2Coeff->getMagnitudeForFrequency(peakFreq, sampleRate);
        }

        if (!chainSettings.peak3Bypass)
        {
            auto peak3Coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, chainSettings.peak3Freq, chainSettings.peak3Quality,
                juce::Decibels::decibelsToGain(chainSettings.peak3GainInDecibels));
            peakMag *= peak3Coeff->getMagnitudeForFrequency(peakFreq, sampleRate);
        }

        if (!chainSettings.lowCutBypass)
        {
            auto lowCutCoeffs = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
                chainSettings.lowCutFreq, sampleRate, 2 * (chainSettings.lowCutSlope + 1));
            for (int stage = 0; stage <= chainSettings.lowCutSlope; ++stage)
                peakMag *= lowCutCoeffs[stage]->getMagnitudeForFrequency(peakFreq, sampleRate);
        }

        if (!chainSettings.highCutBypass)
        {
            auto highCutCoeffs = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
                chainSettings.highCutFreq, sampleRate, 2 * (chainSettings.highCutSlope + 1));
            for (int stage = 0; stage <= chainSettings.highCutSlope; ++stage)
                peakMag *= highCutCoeffs[stage]->getMagnitudeForFrequency(peakFreq, sampleRate);
        }

        float peakDB = (float)juce::Decibels::gainToDecibels(peakMag);
        float peakPy = juce::jmap(peakDB, 24.0f, -24.0f, graphTop, graphBottom);
        peakPy = juce::jlimit(graphTop, graphBottom, peakPy);

        peakPointX = bounds.getX() + peakPx;
        peakPointY = peakPy;
    }

    // Peak 2 (750Hz)
    if (!chainSettings.peak2Bypass)
    {
        float peak2Freq = chainSettings.peak2Freq;
        float peak2Px = juce::jmap(std::log10(peak2Freq), std::log10(20.0f), std::log10(20000.0f), 0.0f, width - 1);

        double peak2Mag = 1.0;
        auto peak2Coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, chainSettings.peak2Freq, chainSettings.peak2Quality,
            juce::Decibels::decibelsToGain(chainSettings.peak2GainInDecibels));
        peak2Mag *= peak2Coeff->getMagnitudeForFrequency(peak2Freq, sampleRate);

        if (!chainSettings.peakBypass)
        {
            auto peakCoeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, chainSettings.peakFreq, chainSettings.peakQuality,
                juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
            peak2Mag *= peakCoeff->getMagnitudeForFrequency(peak2Freq, sampleRate);
        }

        if (!chainSettings.peak3Bypass)
        {
            auto peak3Coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, chainSettings.peak3Freq, chainSettings.peak3Quality,
                juce::Decibels::decibelsToGain(chainSettings.peak3GainInDecibels));
            peak2Mag *= peak3Coeff->getMagnitudeForFrequency(peak2Freq, sampleRate);
        }

        if (!chainSettings.lowCutBypass)
        {
            auto lowCutCoeffs = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
                chainSettings.lowCutFreq, sampleRate, 2 * (chainSettings.lowCutSlope + 1));
            for (int stage = 0; stage <= chainSettings.lowCutSlope; ++stage)
                peak2Mag *= lowCutCoeffs[stage]->getMagnitudeForFrequency(peak2Freq, sampleRate);
        }

        if (!chainSettings.highCutBypass)
        {
            auto highCutCoeffs = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
                chainSettings.highCutFreq, sampleRate, 2 * (chainSettings.highCutSlope + 1));
            for (int stage = 0; stage <= chainSettings.highCutSlope; ++stage)
                peak2Mag *= highCutCoeffs[stage]->getMagnitudeForFrequency(peak2Freq, sampleRate);
        }

        float peak2DB = (float)juce::Decibels::gainToDecibels(peak2Mag);
        float peak2Py = juce::jmap(peak2DB, 24.0f, -24.0f, graphTop, graphBottom);
        peak2Py = juce::jlimit(graphTop, graphBottom, peak2Py);

        peak2PointX = bounds.getX() + peak2Px;
        peak2PointY = peak2Py;
    }

    // Peak 3 (5kHz)
    if (!chainSettings.peak3Bypass)
    {
        float peak3Freq = chainSettings.peak3Freq;
        float peak3Px = juce::jmap(std::log10(peak3Freq), std::log10(20.0f), std::log10(20000.0f), 0.0f, width - 1);

        double peak3Mag = 1.0;
        auto peak3Coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, chainSettings.peak3Freq, chainSettings.peak3Quality,
            juce::Decibels::decibelsToGain(chainSettings.peak3GainInDecibels));
        peak3Mag *= peak3Coeff->getMagnitudeForFrequency(peak3Freq, sampleRate);

        if (!chainSettings.peakBypass)
        {
            auto peakCoeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, chainSettings.peakFreq, chainSettings.peakQuality,
                juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
            peak3Mag *= peakCoeff->getMagnitudeForFrequency(peak3Freq, sampleRate);
        }

        if (!chainSettings.peak2Bypass)
        {
            auto peak2Coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                sampleRate, chainSettings.peak2Freq, chainSettings.peak2Quality,
                juce::Decibels::decibelsToGain(chainSettings.peak2GainInDecibels));
            peak3Mag *= peak2Coeff->getMagnitudeForFrequency(peak3Freq, sampleRate);
        }

        if (!chainSettings.lowCutBypass)
        {
            auto lowCutCoeffs = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
                chainSettings.lowCutFreq, sampleRate, 2 * (chainSettings.lowCutSlope + 1));
            for (int stage = 0; stage <= chainSettings.lowCutSlope; ++stage)
                peak3Mag *= lowCutCoeffs[stage]->getMagnitudeForFrequency(peak3Freq, sampleRate);
        }

        if (!chainSettings.highCutBypass)
        {
            auto highCutCoeffs = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
                chainSettings.highCutFreq, sampleRate, 2 * (chainSettings.highCutSlope + 1));
            for (int stage = 0; stage <= chainSettings.highCutSlope; ++stage)
                peak3Mag *= highCutCoeffs[stage]->getMagnitudeForFrequency(peak3Freq, sampleRate);
        }

        float peak3DB = (float)juce::Decibels::gainToDecibels(peak3Mag);
        float peak3Py = juce::jmap(peak3DB, 24.0f, -24.0f, graphTop, graphBottom);
        peak3Py = juce::jlimit(graphTop, graphBottom, peak3Py);

        peak3PointX = bounds.getX() + peak3Px;
        peak3PointY = peak3Py;
    }
}

void ResponseCurveComponent::paint(juce::Graphics& g)
{
    using namespace juce;
    const auto& s = getSkin();
    g.fillAll(s.graphBg);

    auto w = getWidth();
    auto h = getHeight();

    // =====================================================================
    // Reservamos BOTTOM_MARGIN píxeles abajo para las etiquetas del eje X
    // (mismo valor que se usa en updateFilterCurve())
    // =====================================================================
    float graphHeight = (float)(h - BOTTOM_MARGIN);

    auto sampleRate = audioProcessor.getSampleRate();

    // =====================================================================
    // 1. REJILLA Y TEXTOS (Ejes X e Y funcionales)
    // =====================================================================
    g.setFont(12.0f);

    // --- Eje X (Líneas de frecuencia y etiquetas) ---
    std::vector<float> freqs = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (auto f : freqs)
    {
        auto x = mapFreqToX(f, 0.0f, (float)w);

        // Identificamos las frecuencias principales para resaltarlas
        bool isMajor = (f == 100.0f || f == 1000.0f || f == 10000.0f);

        // La línea será más visible si es 100, 1k o 10k
        g.setColour(isMajor ? s.gridMajor.withAlpha(0.6f) : s.gridMinor.withAlpha(0.2f));
        g.drawVerticalLine((int)x, 0.0f, graphHeight);

        // Solo dibujamos el texto para las frecuencias principales (más 20 y 20k) para no saturar
        if (isMajor || f == 20 || f == 20000) {
            g.setColour(isMajor ? s.gridLabelMajor : s.gridLabelMinor);
            juce::String label = (f >= 1000) ? juce::String(f / 1000, 0) + "k" : juce::String(f, 0);
            g.drawText(label, (int)x - 15, (int)graphHeight + 2, 30, 14, juce::Justification::centred);
        }
    }

    // --- Eje Y Dual (FFT Izquierda: 0..-72dB / Filtros Derecha: 24..-24dB) ---
    std::vector<int> fftLabels = { 0, -18, -36, -54, -72 };
    std::vector<int> eqLabels = { 24, 12, 0, -12, -24 };

    for (size_t i = 0; i < fftLabels.size(); ++i)
    {
        float y = graphHeight * ((float)i / 4.0f);
        if (i == 4) y = graphHeight - 1.0f;

        g.setColour(s.gridHorizontal.withAlpha(0.5f));
        g.drawHorizontalLine((int)y, 0.0f, (float)w);

        g.setColour(s.axisLabel);
        g.drawText(juce::String(fftLabels[i]), 4, (int)y - 14, 30, 14, juce::Justification::left);
        g.drawText(juce::String(eqLabels[i]), w - 34, (int)y - 14, 30, 14, juce::Justification::right);
    }

    if (sampleRate <= 0) return;

    // =====================================================================
    // 2. DIBUJAR LA FFT (ALGORITMO SUAVE Y PIXEL-PERFECT)
    // =====================================================================
    auto analyzerData = leftSpectrumAnalyzer.getDrawingData();
    int numBins = FFT_SIZE / 2;

    Path fftPath;
    bool firstFFTPoint = true;

    for (int x = 0; x < w; ++x)
    {
        float freq = mapXToFreq((float)x, 0.0f, (float)w);
        float binIndex = freq * (float)FFT_SIZE / (float)sampleRate;

        int binLower = jlimit(0, numBins - 2, (int)binIndex);
        int binUpper = binLower + 1;
        float fraction = binIndex - (float)binLower;

        float valLower = analyzerData[binLower];
        float valUpper = analyzerData[binUpper];
        float smoothedVal = valLower + fraction * (valUpper - valLower);

        // Mapeamos al alto de la gráfica (graphHeight) en vez del alto del componente
        float y = jmap(smoothedVal, 0.0f, 1.0f, graphHeight, 0.0f);

        if (firstFFTPoint) {
            fftPath.startNewSubPath((float)x, y);
            firstFFTPoint = false;
        }
        else {
            fftPath.lineTo((float)x, y);
        }
    }

    // Cerramos la curva hasta el límite de graphHeight
    fftPath.lineTo((float)w, graphHeight);
    fftPath.lineTo(0.0f, graphHeight);
    fftPath.closeSubPath();

    // Gradiente vertical: opaco arriba, transparente abajo
    ColourGradient gradient(s.fftFill.withAlpha(0.6f), 0, graphHeight - 150,
        s.fftFill.withAlpha(0.0f), 0, graphHeight, false);
    g.setGradientFill(gradient);
    g.fillPath(fftPath);

    g.setColour(s.fftStroke.withAlpha(0.9f));
    g.strokePath(fftPath, PathStrokeType(1.0f));
    // =====================================================================
    // 3. CURVA DE FILTROS (EQ COMPONENT)
    // =====================================================================
    // Dibujamos filterCurvePath, que fue calculado de forma segura en updateFilterCurve()
    // sin acceder a objetos modificados desde el audio thread.
    g.setColour(s.filterCurve);
    g.strokePath(filterCurvePath, juce::PathStrokeType(2.0f));

    // =====================================================================
    // 4. PUNTOS DE LOS PEAKS
    // =====================================================================
    float pointRadius = 5.0f;

    // Peak 1
    auto chainSettings = getChainSettings(audioProcessor.apvts);
    if (!chainSettings.peakBypass)
    {
        g.setColour(s.peak1);
        g.fillEllipse(peakPointX - pointRadius, peakPointY - pointRadius,
                      pointRadius * 2.0f, pointRadius * 2.0f);
        g.setColour(s.peak1.withAlpha(0.3f));
        g.drawEllipse(peakPointX - pointRadius, peakPointY - pointRadius,
                      pointRadius * 2.0f, pointRadius * 2.0f, 1.5f);
    }

    // Peak 2
    if (!chainSettings.peak2Bypass)
    {
        g.setColour(s.peak2);
        g.fillEllipse(peak2PointX - pointRadius, peak2PointY - pointRadius,
                      pointRadius * 2.0f, pointRadius * 2.0f);
        g.setColour(s.peak2.withAlpha(0.3f));
        g.drawEllipse(peak2PointX - pointRadius, peak2PointY - pointRadius,
                      pointRadius * 2.0f, pointRadius * 2.0f, 1.5f);
    }

    // Peak 3
    if (!chainSettings.peak3Bypass)
    {
        g.setColour(s.peak3);
        g.fillEllipse(peak3PointX - pointRadius, peak3PointY - pointRadius,
                      pointRadius * 2.0f, pointRadius * 2.0f);
        g.setColour(s.peak3.withAlpha(0.3f));
        g.drawEllipse(peak3PointX - pointRadius, peak3PointY - pointRadius,
                      pointRadius * 2.0f, pointRadius * 2.0f, 1.5f);
    }
}

void ResponseCurveComponent::resized()
{
    updateFilterCurve();
}

//==============================================================================
//  Interacción ratón → parámetros (Release 2)
//==============================================================================
//
//  • findPeakAt(): hit-test contra los 3 puntos almacenados (peakPointX/Y,
//                  peak2PointX/Y, peak3PointX/Y); descarta peaks en bypass.
//  • mouseDown:    detecta el peak activo y abre un "gesture" de
//                  automatización sobre Freq+Gain.
//  • mouseDrag:    X→Freq (mapXToFreq) e Y→Gain (mapYToGain). Actualiza
//                  el APVTS vía setValueNotifyingHost(convertTo0to1(...)).
//                  El listener del propio componente se encarga de
//                  redibujar la curva, los attachments mueven los knobs
//                  y el audio thread aplica los nuevos coeficientes.
//  • mouseUp:      cierra el gesto.
//  • mouseWheel:   Q multiplicativa con dos constantes de sensibilidad
//                  (kWheelStep + kWheelGain). Con Ctrl, exponente ×3.
//  • mouseMove:    feedback de cursor (DraggingHand sobre un peak).
//==============================================================================

// --- Sensibilidad de la rueda --------------------------------------
// kWheelStep   = factor base por "click equivalente"
// kWheelGain   = amplifica deltaY (los ratones reportan ~0.15 por muesca)
// Con Ctrl pulsado el exponente se DIVIDE por 3 → modo lento/preciso.
static constexpr float kWheelStep = 1.5f;
static constexpr float kWheelGain = 8.0f;
static constexpr float kSlowMult  = 1.0f / 3.0f;
// -------------------------------------------------------------------

int ResponseCurveComponent::findPeakAt(juce::Point<float> pos) const
{
    auto chainSettings = getChainSettings(audioProcessor.apvts);

    struct Candidate { int idx; float x, y; bool active; };
    const Candidate peaks[3] =
    {
        { 0, peakPointX,  peakPointY,  !chainSettings.peakBypass  },
        { 1, peak2PointX, peak2PointY, !chainSettings.peak2Bypass },
        { 2, peak3PointX, peak3PointY, !chainSettings.peak3Bypass }
    };

    constexpr float hitRadius = 12.0f;  // Algo mayor que el radio visual (5px)
    int   best     = -1;
    float bestDist = hitRadius;

    for (const auto& p : peaks)
    {
        if (!p.active) continue;
        float d = pos.getDistanceFrom({ p.x, p.y });
        if (d < bestDist)
        {
            bestDist = d;
            best     = p.idx;
        }
    }
    return best;
}

void ResponseCurveComponent::beginGestureForPeak(int peak)
{
    if (peak < 0 || peak > 2) return;
    // Solo abrimos gesto en Freq y Gain — la Q se gestiona en mouseWheelMove.
    for (int i = 0; i < 2; ++i)
        if (auto* p = audioProcessor.apvts.getParameter(kPeakParamNames[peak][i]))
            p->beginChangeGesture();
}

void ResponseCurveComponent::endGestureForPeak(int peak)
{
    if (peak < 0 || peak > 2) return;
    for (int i = 0; i < 2; ++i)
        if (auto* p = audioProcessor.apvts.getParameter(kPeakParamNames[peak][i]))
            p->endChangeGesture();
}

void ResponseCurveComponent::mouseMove(const juce::MouseEvent& e)
{
    int peak = findPeakAt(e.position);
    setMouseCursor(peak >= 0 ? juce::MouseCursor::DraggingHandCursor
                             : juce::MouseCursor::NormalCursor);
}

void ResponseCurveComponent::mouseDown(const juce::MouseEvent& e)
{
    activeDragPeak = findPeakAt(e.position);
    if (activeDragPeak >= 0)
    {
        beginGestureForPeak(activeDragPeak);
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
}

void ResponseCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (activeDragPeak < 0) return;

    auto bounds       = getLocalBounds().toFloat();
    auto width        = bounds.getWidth();
    float graphHeight = bounds.getHeight() - BOTTOM_MARGIN;
    if (graphHeight < 1.0f) graphHeight = 1.0f;
    float graphTop    = bounds.getY();
    float graphBottom = graphTop + graphHeight;

    // 1. X del ratón → frecuencia objetivo (escala logarítmica)
    float freq = mapXToFreq(e.position.x, bounds.getX(), width);
    freq = juce::jlimit(20.0f, 20000.0f, freq);

    // 2. Y del ratón → dB objetivo donde queremos que aparezca el círculo
    float yClamped = juce::jlimit(graphTop, graphBottom, e.position.y);
    float targetDB = mapYToGain(yClamped, graphTop, graphBottom);

    // 3. Calcular la contribución de los DEMÁS filtros (LowCut, HighCut y
    //    los otros 2 peaks) en la frecuencia objetivo. La Y dibujada del
    //    círculo es: peakGain + otherDB. Para que el círculo siga al
    //    cursor, el peakGain a aplicar debe ser: targetDB - otherDB
    //    (saturado a ±24 dB).
    auto chainSettings = getChainSettings(audioProcessor.apvts);
    double sampleRate  = audioProcessor.getSampleRate();
    if (sampleRate <= 0.0) sampleRate = 44100.0;

    double otherMag = 1.0;

    auto multiplyByPeak = [&](float pf, float pq, float pgDB)
    {
        auto coeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, pf, pq, juce::Decibels::decibelsToGain(pgDB));
        otherMag *= coeff->getMagnitudeForFrequency((double)freq, sampleRate);
    };

    if (activeDragPeak != 0 && !chainSettings.peakBypass)
        multiplyByPeak(chainSettings.peakFreq,  chainSettings.peakQuality,  chainSettings.peakGainInDecibels);
    if (activeDragPeak != 1 && !chainSettings.peak2Bypass)
        multiplyByPeak(chainSettings.peak2Freq, chainSettings.peak2Quality, chainSettings.peak2GainInDecibels);
    if (activeDragPeak != 2 && !chainSettings.peak3Bypass)
        multiplyByPeak(chainSettings.peak3Freq, chainSettings.peak3Quality, chainSettings.peak3GainInDecibels);

    if (!chainSettings.lowCutBypass)
    {
        auto coeffs = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
            chainSettings.lowCutFreq, sampleRate, 2 * (chainSettings.lowCutSlope + 1));
        for (int stage = 0; stage <= chainSettings.lowCutSlope; ++stage)
            otherMag *= coeffs[stage]->getMagnitudeForFrequency((double)freq, sampleRate);
    }
    if (!chainSettings.highCutBypass)
    {
        auto coeffs = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
            chainSettings.highCutFreq, sampleRate, 2 * (chainSettings.highCutSlope + 1));
        for (int stage = 0; stage <= chainSettings.highCutSlope; ++stage)
            otherMag *= coeffs[stage]->getMagnitudeForFrequency((double)freq, sampleRate);
    }

    float otherDB = (float)juce::Decibels::gainToDecibels(otherMag);

    // 4. Compensación: si los cuts atenúan -X dB, subimos el peak gain
    //    en X dB para que el círculo aparezca exactamente en el cursor.
    //    Si la compensación necesaria excede ±24, queda saturada (el
    //    círculo no podrá seguir al cursor más allá de ese límite).
    float requiredGain = juce::jlimit(-24.0f, 24.0f, targetDB - otherDB);

    auto* freqParam = audioProcessor.apvts.getParameter(kPeakParamNames[activeDragPeak][0]);
    auto* gainParam = audioProcessor.apvts.getParameter(kPeakParamNames[activeDragPeak][1]);

    if (freqParam) freqParam->setValueNotifyingHost(freqParam->convertTo0to1(freq));
    if (gainParam) gainParam->setValueNotifyingHost(gainParam->convertTo0to1(requiredGain));
}

void ResponseCurveComponent::mouseUp(const juce::MouseEvent&)
{
    if (activeDragPeak >= 0)
    {
        endGestureForPeak(activeDragPeak);
        activeDragPeak = -1;
    }
}

void ResponseCurveComponent::mouseWheelMove(const juce::MouseEvent& e,
                                            const juce::MouseWheelDetails& wheel)
{
    int peak = findPeakAt(e.position);
    if (peak < 0) return;

    auto* qParam = audioProcessor.apvts.getParameter(kPeakParamNames[peak][2]);
    if (qParam == nullptr) return;

    // Q es logarítmica por naturaleza → ajuste multiplicativo.
    // Sensibilidad alta para que cada muesca de rueda mueva la Q de
    // forma claramente visible. Con Ctrl, el exponente se divide por 3
    // (modo lento/preciso, útil para ajuste fino).
    float currentQ  = audioProcessor.apvts.getRawParameterValue(kPeakParamNames[peak][2])->load();
    float speedMult = e.mods.isCtrlDown() ? kSlowMult : 1.0f;
    float exponent  = wheel.deltaY * kWheelGain * speedMult;
    float newQ      = juce::jlimit(0.1f, 10.0f, currentQ * std::pow(kWheelStep, exponent));

    qParam->beginChangeGesture();
    qParam->setValueNotifyingHost(qParam->convertTo0to1(newQ));
    qParam->endChangeGesture();
}

//==============================================================================
//  EQAudioProcessorEditor
//==============================================================================
EQAudioProcessorEditor::EQAudioProcessorEditor(EQAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    responseCurveComponent(p),

    // Sliders: le pasamos el parámetro del APVTS y la unidad a mostrar
    peakFreqSlider(*p.apvts.getParameter("Peak Frequency"), "Hz", RotarySliderWithLabels::Peak1),
    peakGainSlider(*p.apvts.getParameter("Peak Gain"), "dB", RotarySliderWithLabels::Peak1),
    peakQualitySlider(*p.apvts.getParameter("Peak Quality"), "", RotarySliderWithLabels::Peak1),
    peak2FreqSlider(*p.apvts.getParameter("Peak2 Frequency"), "Hz", RotarySliderWithLabels::Peak2),
    peak2GainSlider(*p.apvts.getParameter("Peak2 Gain"), "dB", RotarySliderWithLabels::Peak2),
    peak2QualitySlider(*p.apvts.getParameter("Peak2 Quality"), "", RotarySliderWithLabels::Peak2),
    peak3FreqSlider(*p.apvts.getParameter("Peak3 Frequency"), "Hz", RotarySliderWithLabels::Peak3),
    peak3GainSlider(*p.apvts.getParameter("Peak3 Gain"), "dB", RotarySliderWithLabels::Peak3),
    peak3QualitySlider(*p.apvts.getParameter("Peak3 Quality"), "", RotarySliderWithLabels::Peak3),
    lowCutFreqSlider(*p.apvts.getParameter("LowCut Frequency"), "Hz", RotarySliderWithLabels::Default),
    highCutFreqSlider(*p.apvts.getParameter("HighCut Frequency"), "Hz", RotarySliderWithLabels::Default),

    // Attachments: sincronizan slider y APVTS automáticamente
    peakFreqAttachment(p.apvts, "Peak Frequency", peakFreqSlider),
    peakGainAttachment(p.apvts, "Peak Gain", peakGainSlider),
    peakQualityAttachment(p.apvts, "Peak Quality", peakQualitySlider),
    peak2FreqAttachment(p.apvts, "Peak2 Frequency", peak2FreqSlider),
    peak2GainAttachment(p.apvts, "Peak2 Gain", peak2GainSlider),
    peak2QualityAttachment(p.apvts, "Peak2 Quality", peak2QualitySlider),
    peak3FreqAttachment(p.apvts, "Peak3 Frequency", peak3FreqSlider),
    peak3GainAttachment(p.apvts, "Peak3 Gain", peak3GainSlider),
    peak3QualityAttachment(p.apvts, "Peak3 Quality", peak3QualitySlider),
    lowCutFreqAttachment(p.apvts, "LowCut Frequency", lowCutFreqSlider),
    highCutFreqAttachment(p.apvts, "HighCut Frequency", highCutFreqSlider),
    lowCutSlopeAttachment(p.apvts, "LowCut Slope", lowCutSlopeBox),
    highCutSlopeAttachment(p.apvts, "HighCut Slope", highCutSlopeBox)
{
    // El editor tiene su propio LookAndFeel para que combos/toggles/popups
    // se puedan tematizar sin afectar al LookAndFeel global de JUCE.
    setLookAndFeel(&editorLnF);

    // Añadir todos los hijos
    addAndMakeVisible(responseCurveComponent);
    addAndMakeVisible(defaultButton);

    // --- Selector de Skin (Standard / Dark / Light) ---
    skinLabel.setText("Skin", juce::dontSendNotification);
    skinLabel.setJustificationType(juce::Justification::centredRight);
    skinLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(skinLabel);

    skinBox.addItem("Standard", 1);
    skinBox.addItem("Dark",     2);
    skinBox.addItem("Light",    3);
    skinBox.setSelectedItemIndex(0, juce::dontSendNotification);  // Standard por defecto
    skinBox.onChange = [this]()
    {
        int idx = skinBox.getSelectedItemIndex();
        if (idx < 0) return;
        setCurrentSkin(static_cast<SkinId>(idx));
        applySkinToControls();

        // Repintar todo el árbol de componentes para que el nuevo skin se aplique
        std::function<void(juce::Component&)> repaintAll = [&](juce::Component& c)
        {
            c.repaint();
            for (auto* child : c.getChildren()) repaintAll(*child);
        };
        repaintAll(*this);
    };
    addAndMakeVisible(skinBox);

    // Añadir botones On/Off
    addAndMakeVisible(lowCutToggle);
    addAndMakeVisible(peakToggle);
    addAndMakeVisible(peak2Toggle);
    addAndMakeVisible(peak3Toggle);
    addAndMakeVisible(highCutToggle);

    for (auto* slider : { &peakFreqSlider, &peakGainSlider, &peakQualitySlider,
                          &peak2FreqSlider, &peak2GainSlider, &peak2QualitySlider,
                          &peak3FreqSlider, &peak3GainSlider, &peak3QualitySlider,
                          &lowCutFreqSlider, &highCutFreqSlider })
        addAndMakeVisible(slider);

    addAndMakeVisible(lowCutSlopeBox);
    addAndMakeVisible(highCutSlopeBox);

    // Configurar el botón Default
    defaultButton.onClick = [this] { resetToDefaults(); };

    // Conectar los botones toggle con los parámetros de bypass
    lowCutToggle.setToggleState(!audioProcessor.apvts.getRawParameterValue("LowCut Bypass")->load(), false);
    lowCutToggle.onClick = [this]() { 
        audioProcessor.apvts.getParameter("LowCut Bypass")->setValueNotifyingHost(!lowCutToggle.getToggleState());
        bool isBypassed = !lowCutToggle.getToggleState();  // Invertir porque el toggle aún no ha cambiado
        lowCutFreqSlider.setBypassState(isBypassed);
    };

    peakToggle.setToggleState(!audioProcessor.apvts.getRawParameterValue("Peak Bypass")->load(), false);
    peakToggle.onClick = [this]() { 
        audioProcessor.apvts.getParameter("Peak Bypass")->setValueNotifyingHost(!peakToggle.getToggleState());
        bool isBypassed = !peakToggle.getToggleState();  // Invertir porque el toggle aún no ha cambiado
        peakFreqSlider.setBypassState(isBypassed);
        peakGainSlider.setBypassState(isBypassed);
        peakQualitySlider.setBypassState(isBypassed);
    };

    peak2Toggle.setToggleState(!audioProcessor.apvts.getRawParameterValue("Peak2 Bypass")->load(), false);
    peak2Toggle.onClick = [this]() { 
        audioProcessor.apvts.getParameter("Peak2 Bypass")->setValueNotifyingHost(!peak2Toggle.getToggleState());
        bool isBypassed = !peak2Toggle.getToggleState();  // Invertir porque el toggle aún no ha cambiado
        peak2FreqSlider.setBypassState(isBypassed);
        peak2GainSlider.setBypassState(isBypassed);
        peak2QualitySlider.setBypassState(isBypassed);
    };

    peak3Toggle.setToggleState(!audioProcessor.apvts.getRawParameterValue("Peak3 Bypass")->load(), false);
    peak3Toggle.onClick = [this]() { 
        audioProcessor.apvts.getParameter("Peak3 Bypass")->setValueNotifyingHost(!peak3Toggle.getToggleState());
        bool isBypassed = !peak3Toggle.getToggleState();  // Invertir porque el toggle aún no ha cambiado
        peak3FreqSlider.setBypassState(isBypassed);
        peak3GainSlider.setBypassState(isBypassed);
        peak3QualitySlider.setBypassState(isBypassed);
    };

    // Establecer el estado inicial de bypass para cada slider
    peakFreqSlider.setBypassState(audioProcessor.apvts.getRawParameterValue("Peak Bypass")->load() > 0.5f);
    peakGainSlider.setBypassState(audioProcessor.apvts.getRawParameterValue("Peak Bypass")->load() > 0.5f);
    peakQualitySlider.setBypassState(audioProcessor.apvts.getRawParameterValue("Peak Bypass")->load() > 0.5f);

    peak2FreqSlider.setBypassState(audioProcessor.apvts.getRawParameterValue("Peak2 Bypass")->load() > 0.5f);
    peak2GainSlider.setBypassState(audioProcessor.apvts.getRawParameterValue("Peak2 Bypass")->load() > 0.5f);
    peak2QualitySlider.setBypassState(audioProcessor.apvts.getRawParameterValue("Peak2 Bypass")->load() > 0.5f);

    peak3FreqSlider.setBypassState(audioProcessor.apvts.getRawParameterValue("Peak3 Bypass")->load() > 0.5f);
    peak3GainSlider.setBypassState(audioProcessor.apvts.getRawParameterValue("Peak3 Bypass")->load() > 0.5f);
    peak3QualitySlider.setBypassState(audioProcessor.apvts.getRawParameterValue("Peak3 Bypass")->load() > 0.5f);

    lowCutFreqSlider.setBypassState(audioProcessor.apvts.getRawParameterValue("LowCut Bypass")->load() > 0.5f);
    highCutFreqSlider.setBypassState(audioProcessor.apvts.getRawParameterValue("HighCut Bypass")->load() > 0.5f);

    highCutToggle.setToggleState(!audioProcessor.apvts.getRawParameterValue("HighCut Bypass")->load(), false);
    highCutToggle.onClick = [this]() { 
        audioProcessor.apvts.getParameter("HighCut Bypass")->setValueNotifyingHost(!highCutToggle.getToggleState());
        bool isBypassed = !highCutToggle.getToggleState();  // Invertir porque el toggle aún no ha cambiado
        highCutFreqSlider.setBypassState(isBypassed);
    };

    // Poblar los ComboBoxes con las opciones del parámetro
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(
        p.apvts.getParameter("LowCut Slope")))
    {
        for (int i = 0; i < param->choices.size(); ++i)
            lowCutSlopeBox.addItem(param->choices[i], i + 1);
    }

    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(
        p.apvts.getParameter("HighCut Slope")))
    {
        for (int i = 0; i < param->choices.size(); ++i)
            highCutSlopeBox.addItem(param->choices[i], i + 1);
    }

    // ---------------------------------------------------------------
    //  Botón INPUT — abre FileChooser y carga el fichero en el processor
    // ---------------------------------------------------------------
    addAndMakeVisible(inputButton);
    inputButton.onClick = [this]()
    {
        // fileChooser se guarda como miembro para que no se destruya antes del callback
        fileChooser = std::make_unique<juce::FileChooser>(
            "Selecciona un fichero de audio",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg;*.m4a");

        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result.existsAsFile())
                {
                    bool ok = audioProcessor.loadAudioFile(result);
                    if (ok)
                    {
                        // Actualiza el texto del botón con el nombre del fichero
                        inputButton.setButtonText(result.getFileNameWithoutExtension());
                        playStopButton.setButtonText("Play");
                        playStopButton.setVisible(true);
                        fileVolumeSlider.setVisible(true);
                        resized(); // reposicionar por si los botones cambiaron de tamaño
                    }
                }
            });
    };

    // ---------------------------------------------------------------
    //  Botón PLAY/STOP
    // ---------------------------------------------------------------
    addAndMakeVisible(playStopButton);
    playStopButton.setVisible(audioProcessor.hasFileLoaded());
    playStopButton.onClick = [this]()
    {
        if (audioProcessor.isFilePlayerPlaying())
        {
            audioProcessor.filePlayerStop();
            playStopButton.setButtonText("Play");
        }
        else
        {
            audioProcessor.filePlayerPlay();
            playStopButton.setButtonText("Stop");
        }
    };

    // ---------------------------------------------------------------
    //  Slider de volumen del fichero
    // ---------------------------------------------------------------
    fileVolumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    fileVolumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    fileVolumeSlider.setRange(0.0, 1.0);
    fileVolumeSlider.setValue(audioProcessor.getFilePlayerGain());
    fileVolumeSlider.setVisible(audioProcessor.hasFileLoaded());
    fileVolumeSlider.onValueChange = [this]()
    {
        audioProcessor.setFilePlayerGain((float)fileVolumeSlider.getValue());
    };
    addAndMakeVisible(fileVolumeSlider);

    // Aplicar el skin inicial (Standard) a los controles JUCE
    applySkinToControls();

    setSize(900, 628);
}

EQAudioProcessorEditor::~EQAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void EQAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto& s = getSkin();
    g.fillAll(s.pluginBg);

    constexpr int kTopBarH = 28;

    // ---- Franja superior (botones) -------------------------------------
    g.setColour(s.topBarBg);
    g.fillRect(0, 0, getWidth(), kTopBarH);

    // Línea fina que separa la franja del resto
    g.setColour(s.topBarSeparator);
    g.fillRect(0, kTopBarH, getWidth(), 1);

    // ---- Líneas verticales que separan las 5 secciones de parámetros ----
    // Calculadas con el mismo criterio que resized(): área útil = todo lo
    // que hay debajo de la franja, reducido 8 px por los lados.
    auto area = getLocalBounds().withTrimmedTop(kTopBarH).reduced(8);
    auto displayArea = area.withHeight(juce::roundToInt(area.getHeight() * 0.65f));
    int graphBottom = displayArea.getBottom();

    int totalWidth = getWidth() - 16;  // Restamos los 8 px de margen en ambos lados
    int sectionWidth = totalWidth / 5;

    g.setColour(s.sectionSeparator.withAlpha(0.6f));

    for (int i = 1; i < 5; ++i) {
        int x = 8 + i * sectionWidth;
        g.drawVerticalLine(x, (float)graphBottom, (float)getHeight());
    }
}

void EQAudioProcessorEditor::resized()
{
    constexpr int kTopBarH = 28;  // altura de la franja superior

    // ---- Franja superior: todos los botones a y=2, h=24 ----------------
    int btnY = 2;  // 2 px de aire arriba/abajo dentro de la franja de 28 px
    int btnH = 24;

    defaultButton.setBounds(8, btnY, 80, btnH);

    int skinLabelW = 32;
    int skinBoxW   = 90;
    int skinX      = 8 + 80 + 8;  // tras el botón Default + gap
    skinLabel.setBounds(skinX, btnY, skinLabelW, btnH);
    skinBox  .setBounds(skinX + skinLabelW + 4, btnY, skinBoxW, btnH);

    int rightEdge = getWidth() - 8;

    // Slider de volumen del fichero
    int volW = 80;
    fileVolumeSlider.setBounds(rightEdge - volW, btnY, volW, btnH);

    // Play/Stop a la izquierda del slider
    int playW = 50;
    playStopButton.setBounds(rightEdge - volW - 4 - playW, btnY, playW, btnH);

    // Input a la izquierda del Play/Stop
    int inputW = 120;
    inputButton.setBounds(rightEdge - volW - 4 - playW - 4 - inputW, btnY, inputW, btnH);

    // ---- Resto del layout: bajo la franja, con 8 px de margen -----------
    auto area = getLocalBounds().withTrimmedTop(kTopBarH).reduced(8);

    // 1. El visualizador ocupa el 65% superior del área útil
    auto displayArea = area.removeFromTop(juce::roundToInt(area.getHeight() * 0.65f));
    responseCurveComponent.setBounds(displayArea);

    // 2. Separación extra entre la gráfica y los controles

    // 3. Área de controles
    auto controlsArea = area;
    int maxSliderHeight = 130;
    if (controlsArea.getHeight() > maxSliderHeight) {
        controlsArea = controlsArea.withSizeKeepingCentre(controlsArea.getWidth(), maxSliderHeight);
    }

    // 4. Dividimos en 5 secciones iguales
    int sectionWidth = controlsArea.getWidth() / 5;

   
    int buttonAreaHeight = 35;
    int buttonY = displayArea.getBottom();  // justo debajo del gráfico, en el límite

    auto sliderArea = controlsArea;

    // Posicionamiento de botones On/Off
    int buttonSize = 24;
    int buttonPadding = 3;
    int buttonToggleY = buttonY + buttonPadding;

    // Botón On/Off para LowCut (sección 1, esquina superior izquierda)
    int section1LeftX = controlsArea.getX() + buttonPadding;
    lowCutToggle.setBounds(section1LeftX, buttonToggleY, buttonSize, buttonSize);

    // Botón On/Off para Peak (sección 2, esquina superior izquierda)
    int section2LeftX = controlsArea.getX() + sectionWidth + buttonPadding;
    peakToggle.setBounds(section2LeftX, buttonToggleY, buttonSize, buttonSize);

    // Botón On/Off para Peak2 (sección 3, esquina superior izquierda)
    int section3LeftX = controlsArea.getX() + 2 * sectionWidth + buttonPadding;
    peak2Toggle.setBounds(section3LeftX, buttonToggleY, buttonSize, buttonSize);

    // Botón On/Off para Peak3 (sección 4, esquina superior izquierda)
    int section4LeftX = controlsArea.getX() + 3 * sectionWidth + buttonPadding;
    peak3Toggle.setBounds(section4LeftX, buttonToggleY, buttonSize, buttonSize);

    // Botón On/Off para HighCut (sección 5, esquina superior izquierda)
    int section5LeftX = controlsArea.getX() + 4 * sectionWidth + buttonPadding;
    highCutToggle.setBounds(section5LeftX, buttonToggleY, buttonSize, buttonSize);

    // Margen superior para separar knobs del borde de la zona de controles
    int topMargin = 12;
    auto sliderAreaWithMargin = sliderArea.withTop(sliderArea.getY() + topMargin);

    // Sección 1: LowCut Filter
    auto section1 = sliderAreaWithMargin.withLeft(sliderAreaWithMargin.getX()).withWidth(sectionWidth);
    lowCutFreqSlider.setBounds(section1);

    // Combobox de LowCut Slope debajo
    auto lowCutBounds = lowCutFreqSlider.getBounds();
    lowCutSlopeBox.setBounds(lowCutBounds.getX() + 5, lowCutBounds.getBottom() + 5, lowCutBounds.getWidth() - 10, 20);

    // Sección 2: Peak Filter - Disposición triangular (2 arriba, 1 abajo)
    auto section2 = sliderAreaWithMargin.withLeft(sliderAreaWithMargin.getX() + sectionWidth).withWidth(sectionWidth);
    auto peakSection = section2.reduced(5);

    int peakSliderSize = 65;
    int peakGap = 5;

    // Calcular posición para centrar triangularmente
    int totalWidth = peakSliderSize * 2 + peakGap;
    int leftOffset = (peakSection.getWidth() - totalWidth) / 2;
    int topOffset = (peakSection.getHeight() - peakSliderSize - peakGap - peakSliderSize) / 2;

    // Primera fila: Freq (izquierda) y Gain (derecha)
    juce::Rectangle<int> freqBounds(peakSection.getX() + leftOffset, 
                                      peakSection.getY() + topOffset, 
                                      peakSliderSize, peakSliderSize);
    peakFreqSlider.setBounds(freqBounds);

    juce::Rectangle<int> gainBounds(peakSection.getX() + leftOffset + peakSliderSize + peakGap, 
                                     peakSection.getY() + topOffset, 
                                     peakSliderSize, peakSliderSize);
    peakGainSlider.setBounds(gainBounds);

    // Segunda fila: Quality (centrado abajo)
    int qualityLeftOffset = (peakSection.getWidth() - peakSliderSize) / 2;
    juce::Rectangle<int> qualityBounds(peakSection.getX() + qualityLeftOffset, 
                                        peakSection.getY() + topOffset + peakSliderSize + peakGap, 
                                        peakSliderSize, peakSliderSize);
    peakQualitySlider.setBounds(qualityBounds);

    // Sección 3: Peak2 Filter - Disposición triangular (2 arriba, 1 abajo)
    auto section3 = sliderAreaWithMargin.withLeft(sliderAreaWithMargin.getX() + 2 * sectionWidth).withWidth(sectionWidth);
    auto peak2Section = section3.reduced(5);

    // Calcular posición para centrar triangularmente
    int peak2Offset = (peak2Section.getWidth() - totalWidth) / 2;
    int peak2TopOffset = (peak2Section.getHeight() - peakSliderSize - peakGap - peakSliderSize) / 2;

    // Primera fila: Freq (izquierda) y Gain (derecha)
    juce::Rectangle<int> freq2Bounds(peak2Section.getX() + peak2Offset, 
                                       peak2Section.getY() + peak2TopOffset, 
                                       peakSliderSize, peakSliderSize);
    peak2FreqSlider.setBounds(freq2Bounds);

    juce::Rectangle<int> gain2Bounds(peak2Section.getX() + peak2Offset + peakSliderSize + peakGap, 
                                      peak2Section.getY() + peak2TopOffset, 
                                      peakSliderSize, peakSliderSize);
    peak2GainSlider.setBounds(gain2Bounds);

    // Segunda fila: Quality (centrado abajo)
    int quality2LeftOffset = (peak2Section.getWidth() - peakSliderSize) / 2;
    juce::Rectangle<int> quality2Bounds(peak2Section.getX() + quality2LeftOffset, 
                                         peak2Section.getY() + peak2TopOffset + peakSliderSize + peakGap, 
                                         peakSliderSize, peakSliderSize);
    peak2QualitySlider.setBounds(quality2Bounds);

    // Sección 4: Peak3 Filter - Disposición triangular (2 arriba, 1 abajo)
    auto section4 = sliderAreaWithMargin.withLeft(sliderAreaWithMargin.getX() + 3 * sectionWidth).withWidth(sectionWidth);
    auto peak3Section = section4.reduced(5);

    // Calcular posición para centrar triangularmente
    int peak3Offset = (peak3Section.getWidth() - totalWidth) / 2;
    int peak3TopOffset = (peak3Section.getHeight() - peakSliderSize - peakGap - peakSliderSize) / 2;

    // Primera fila: Freq (izquierda) y Gain (derecha)
    juce::Rectangle<int> freq3Bounds(peak3Section.getX() + peak3Offset, 
                                       peak3Section.getY() + peak3TopOffset, 
                                       peakSliderSize, peakSliderSize);
    peak3FreqSlider.setBounds(freq3Bounds);

    juce::Rectangle<int> gain3Bounds(peak3Section.getX() + peak3Offset + peakSliderSize + peakGap, 
                                      peak3Section.getY() + peak3TopOffset, 
                                      peakSliderSize, peakSliderSize);
    peak3GainSlider.setBounds(gain3Bounds);

    // Segunda fila: Quality (centrado abajo)
    int quality3LeftOffset = (peak3Section.getWidth() - peakSliderSize) / 2;
    juce::Rectangle<int> quality3Bounds(peak3Section.getX() + quality3LeftOffset, 
                                         peak3Section.getY() + peak3TopOffset + peakSliderSize + peakGap, 
                                         peakSliderSize, peakSliderSize);
    peak3QualitySlider.setBounds(quality3Bounds);

    // Sección 5: HighCut Filter
    auto section5 = sliderAreaWithMargin.withLeft(sliderAreaWithMargin.getX() + 4 * sectionWidth).withWidth(sectionWidth);
    highCutFreqSlider.setBounds(section5);

    // Combobox de HighCut Slope debajo, también bajado
    auto highCutBounds = highCutFreqSlider.getBounds();
    highCutSlopeBox.setBounds(highCutBounds.getX() + 5, highCutBounds.getBottom() + 5, highCutBounds.getWidth() - 10, 20);
}

    //==========================================================================
    //  applySkinToControls()
    //  Vuelca los colores del skin actual sobre los controles JUCE del editor
    //  (toggles, combos, label, botones) y sobre el LookAndFeel propio
    //  (PopupMenu del combo). Llamada en el ctor y al cambiar de skin.
    //==========================================================================
    void EQAudioProcessorEditor::applySkinToControls()
    {
        const auto& s = getSkin();

        // ---- LookAndFeel del editor: popups y defaults globales del LnF ----
        editorLnF.setColour(juce::PopupMenu::backgroundColourId,           s.popupBg);
        editorLnF.setColour(juce::PopupMenu::textColourId,                 s.popupText);
        editorLnF.setColour(juce::PopupMenu::headerTextColourId,           s.popupText);
        editorLnF.setColour(juce::PopupMenu::highlightedBackgroundColourId, s.popupHighlight);
        editorLnF.setColour(juce::PopupMenu::highlightedTextColourId,      s.popupHighlightText);

        // ---- Label "Skin" ------------------------------------------------
        skinLabel.setColour(juce::Label::textColourId,       s.controlText);
        skinLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);

        // ---- ComboBoxes (Skin + LowCut Slope + HighCut Slope) -----------
        auto applyCombo = [&](juce::ComboBox& cb)
        {
            cb.setColour(juce::ComboBox::backgroundColourId,     s.controlBg);
            cb.setColour(juce::ComboBox::textColourId,           s.controlText);
            cb.setColour(juce::ComboBox::outlineColourId,        s.controlOutline);
            cb.setColour(juce::ComboBox::buttonColourId,         s.controlOutline);
            cb.setColour(juce::ComboBox::arrowColourId,          s.controlAccent);
            cb.setColour(juce::ComboBox::focusedOutlineColourId, s.controlOutline);
        };
        applyCombo(skinBox);
        applyCombo(lowCutSlopeBox);
        applyCombo(highCutSlopeBox);

        // ---- ToggleButtons On/Off -----------------------------------------
        // tickColourId         = aspa cuando está activo
        // tickDisabledColourId = borde de la caja
        // textColourId         = label "LowCut" / "Peak" / etc.
        for (auto* tb : { &lowCutToggle, &peakToggle, &peak2Toggle, &peak3Toggle, &highCutToggle })
        {
            tb->setColour(juce::ToggleButton::textColourId,         s.controlText);
            tb->setColour(juce::ToggleButton::tickColourId,         s.controlAccent);
            tb->setColour(juce::ToggleButton::tickDisabledColourId, s.controlOutline);
        }

        // ---- TextButtons (Default / Input / PlayStop) ---------------------
        for (auto* btn : { &defaultButton, &inputButton, &playStopButton })
        {
            btn->setColour(juce::TextButton::buttonColourId,   s.controlBg);
            btn->setColour(juce::TextButton::buttonOnColourId, s.controlOutline);
            btn->setColour(juce::TextButton::textColourOffId,  s.controlText);
            btn->setColour(juce::TextButton::textColourOnId,   s.controlText);
        }

        // ---- Slider de volumen del fichero --------------------------------
        fileVolumeSlider.setColour(juce::Slider::trackColourId,         s.controlAccent);
        fileVolumeSlider.setColour(juce::Slider::backgroundColourId,    s.controlBg);
        fileVolumeSlider.setColour(juce::Slider::thumbColourId,         s.controlAccent);
        fileVolumeSlider.setColour(juce::Slider::textBoxTextColourId,   s.controlText);
    }

    void EQAudioProcessorEditor::resetToDefaults()
    {
        // Resetear los parámetros flotantes a sus valores por defecto
        audioProcessor.apvts.getParameter("LowCut Frequency")->setValueNotifyingHost(0.0f);   // 20 Hz
        audioProcessor.apvts.getParameter("HighCut Frequency")->setValueNotifyingHost(1.0f);  // 20000 Hz
        audioProcessor.apvts.getParameter("Peak Frequency")->setValueNotifyingHost(audioProcessor.apvts.getParameter("Peak Frequency")->getDefaultValue());
        audioProcessor.apvts.getParameter("Peak Gain")->setValueNotifyingHost(0.5f);          // 0 dB (centro del rango)
        audioProcessor.apvts.getParameter("Peak Quality")->setValueNotifyingHost(audioProcessor.apvts.getParameter("Peak Quality")->getDefaultValue());

        audioProcessor.apvts.getParameter("Peak2 Frequency")->setValueNotifyingHost(audioProcessor.apvts.getParameter("Peak2 Frequency")->getDefaultValue());
        audioProcessor.apvts.getParameter("Peak2 Gain")->setValueNotifyingHost(0.5f);          // 0 dB (centro del rango)
        audioProcessor.apvts.getParameter("Peak2 Quality")->setValueNotifyingHost(audioProcessor.apvts.getParameter("Peak2 Quality")->getDefaultValue());

        audioProcessor.apvts.getParameter("Peak3 Frequency")->setValueNotifyingHost(audioProcessor.apvts.getParameter("Peak3 Frequency")->getDefaultValue());
        audioProcessor.apvts.getParameter("Peak3 Gain")->setValueNotifyingHost(0.5f);          // 0 dB (centro del rango)
        audioProcessor.apvts.getParameter("Peak3 Quality")->setValueNotifyingHost(audioProcessor.apvts.getParameter("Peak3 Quality")->getDefaultValue());

        // Resetear los slopes a 12 dB/Oct (índice 0)
        audioProcessor.apvts.getParameter("LowCut Slope")->setValueNotifyingHost(0.0f);
        audioProcessor.apvts.getParameter("HighCut Slope")->setValueNotifyingHost(0.0f);

        // Activar todos los filtros (bypass = false/0.0f)
        audioProcessor.apvts.getParameter("LowCut Bypass")->setValueNotifyingHost(0.0f);
        audioProcessor.apvts.getParameter("Peak Bypass")->setValueNotifyingHost(0.0f);
        audioProcessor.apvts.getParameter("Peak2 Bypass")->setValueNotifyingHost(0.0f);
        audioProcessor.apvts.getParameter("Peak3 Bypass")->setValueNotifyingHost(0.0f);
        audioProcessor.apvts.getParameter("HighCut Bypass")->setValueNotifyingHost(0.0f);

        // Actualizar estado visual de los botones toggle (inverso del bypass)
        lowCutToggle.setToggleState(true, false);
        peakToggle.setToggleState(true, false);
        peak2Toggle.setToggleState(true, false);
        peak3Toggle.setToggleState(true, false);
        highCutToggle.setToggleState(true, false);

        // Actualizar estado visual de los sliders (no están en bypass)
        peakFreqSlider.setBypassState(false);
        peakGainSlider.setBypassState(false);
        peakQualitySlider.setBypassState(false);

        peak2FreqSlider.setBypassState(false);
        peak2GainSlider.setBypassState(false);
        peak2QualitySlider.setBypassState(false);

        peak3FreqSlider.setBypassState(false);
        peak3GainSlider.setBypassState(false);
        peak3QualitySlider.setBypassState(false);

        lowCutFreqSlider.setBypassState(false);
        highCutFreqSlider.setBypassState(false);
    }