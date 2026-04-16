/*
  ==============================================================================
    PluginEditor.cpp
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
//  Constantes de colores para los filtros peak
//==============================================================================
static const juce::Colour PEAK1_COLOR  = juce::Colour(220, 80, 80);   // Rojo saturado
static const juce::Colour PEAK2_COLOR  = juce::Colour(100, 200, 100); // Verde saturado
static const juce::Colour PEAK3_COLOR  = juce::Colour(220, 180, 60);  // Amarillo/Dorado saturado

//==============================================================================
//  Helpers de layout  (frecuencia → posición X logarítmica)
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

    // Reservamos 16px abajo para el nombre del parámetro
    auto labelH = 16;
    auto bounds = Rectangle<float>(x, y, width, height - labelH);
    auto radius = jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto center = bounds.getCentre();

    // Determinar el color según el tipo de slider
    Colour accentColor = Colour(0, 175, 215);  // Default azul-cian
    bool isBypassed = false;

    if (auto* rsw = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        isBypassed = rsw->isBypassed;

        if (isBypassed)
        {
            accentColor = Colour(80, 80, 90);  // Gris cuando está en bypass
        }
        else
        {
            switch (rsw->sliderType)
            {
                case RotarySliderWithLabels::Peak1:
                    accentColor = PEAK1_COLOR;
                    break;
                case RotarySliderWithLabels::Peak2:
                    accentColor = PEAK2_COLOR;
                    break;
                case RotarySliderWithLabels::Peak3:
                    accentColor = PEAK3_COLOR;
                    break;
                default:
                    break;
            }
        }
    }

    // --- Fondo del knob ---
    g.setColour(Colour(35, 35, 40));
    g.fillEllipse(bounds.withSizeKeepingCentre(radius * 2.0f, radius * 2.0f));

    // --- Borde exterior ---
    g.setColour(Colour(75, 75, 85));
    g.drawEllipse(bounds.withSizeKeepingCentre(radius * 2.0f, radius * 2.0f), 1.5f);

    // --- Arco de recorrido total (gris oscuro) ---
    auto trackR = radius - 5.0f;
    Path arcBg;
    arcBg.addCentredArc(center.x, center.y, trackR, trackR,
        0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(Colour(55, 55, 60));
    g.strokePath(arcBg, PathStrokeType(3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    // --- Arco de valor (color según tipo de slider) ---
    float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    Path arcVal;
    arcVal.addCentredArc(center.x, center.y, trackR, trackR,
        0.0f, rotaryStartAngle, angle, true);

    // Si está en bypass, usar opacidad baja
    if (isBypassed)
        g.setColour(accentColor.withAlpha(0.3f));
    else
        g.setColour(accentColor);

    g.strokePath(arcVal, PathStrokeType(3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    // --- Línea indicadora (desde centro hasta borde interior) ---
    auto thumbR = radius - 11.0f;
    Point<float> thumbPt(center.x + thumbR * std::sin(angle),
        center.y - thumbR * std::cos(angle));
    g.setColour(Colour(210, 210, 210));
    g.drawLine(Line<float>(center, thumbPt), 2.0f);

    // --- Punto central (color según tipo de slider) ---
    if (isBypassed)
        g.setColour(accentColor.withAlpha(0.3f));
    else
        g.setColour(accentColor);

    g.fillEllipse(Rectangle<float>(5.0f, 5.0f).withCentre(center));

    // --- Valor numérico (centrado en el knob) ---
    if (auto* rsw = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        if (isBypassed)
            g.setColour(Colour(120, 120, 130).withAlpha(0.4f));
        else
            g.setColour(Colour(200, 200, 200));

        g.setFont(FontOptions(10.5f));
        g.drawFittedText(rsw->getDisplayString(),
            bounds.withSizeKeepingCentre(radius * 1.4f, radius * 0.6f).toNearestInt(),
            Justification::centred, 1);

        // --- Nombre del parámetro debajo del knob ---
        if (isBypassed)
            g.setColour(Colour(100, 100, 110).withAlpha(0.4f));
        else
            g.setColour(Colour(120, 120, 130));

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
    //  IMPORTANTE: recalculamos los coeficientes DESDE EL APVTS,
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
    g.fillAll(Colours::black);

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
        g.setColour(isMajor ? juce::Colours::dimgrey.withAlpha(0.6f) : juce::Colours::dimgrey.withAlpha(0.2f));
        g.drawVerticalLine((int)x, 0.0f, graphHeight);

        // Solo dibujamos el texto para las frecuencias principales (más 20 y 20k) para no saturar
        if (isMajor || f == 20 || f == 20000) {
            g.setColour(isMajor ? juce::Colours::white : juce::Colours::lightgrey);
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

        g.setColour(juce::Colours::dimgrey.withAlpha(0.5f));
        g.drawHorizontalLine((int)y, 0.0f, (float)w);

        g.setColour(juce::Colours::lightgrey);
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

    // Ajustamos el gradiente también
    ColourGradient gradient(Colours::cyan.withAlpha(0.6f), 0, graphHeight - 150,
        Colours::cyan.withAlpha(0.0f), 0, graphHeight, false);
    g.setGradientFill(gradient);
    g.fillPath(fftPath);

    g.setColour(Colours::cyan.withAlpha(0.9f));
    g.strokePath(fftPath, PathStrokeType(1.0f));
    // =====================================================================
    // 3. CURVA DE FILTROS (EQ COMPONENT)
    // =====================================================================
    // Dibujamos filterCurvePath, que fue calculado de forma segura en updateFilterCurve()
    // sin acceder a objetos modificados desde el audio thread.
    g.setColour(juce::Colours::white);
    g.strokePath(filterCurvePath, juce::PathStrokeType(2.0f));

    // =====================================================================
    // 4. PUNTOS DE LOS PEAKS
    // =====================================================================
    float pointRadius = 5.0f;

    // Peak 1 (100Hz) - Color Azul
    auto chainSettings = getChainSettings(audioProcessor.apvts);
    if (!chainSettings.peakBypass)
    {
        g.setColour(PEAK1_COLOR);
        g.fillEllipse(peakPointX - pointRadius, peakPointY - pointRadius, 
                      pointRadius * 2.0f, pointRadius * 2.0f);
        g.setColour(PEAK1_COLOR.withAlpha(0.3f));
        g.drawEllipse(peakPointX - pointRadius, peakPointY - pointRadius, 
                      pointRadius * 2.0f, pointRadius * 2.0f, 1.5f);
    }

    // Peak 2 (750Hz) - Color Verde
    if (!chainSettings.peak2Bypass)
    {
        g.setColour(PEAK2_COLOR);
        g.fillEllipse(peak2PointX - pointRadius, peak2PointY - pointRadius, 
                      pointRadius * 2.0f, pointRadius * 2.0f);
        g.setColour(PEAK2_COLOR.withAlpha(0.3f));
        g.drawEllipse(peak2PointX - pointRadius, peak2PointY - pointRadius, 
                      pointRadius * 2.0f, pointRadius * 2.0f, 1.5f);
    }

    // Peak 3 (5kHz) - Color Amarillo/Dorado
    if (!chainSettings.peak3Bypass)
    {
        g.setColour(PEAK3_COLOR);
        g.fillEllipse(peak3PointX - pointRadius, peak3PointY - pointRadius, 
                      pointRadius * 2.0f, pointRadius * 2.0f);
        g.setColour(PEAK3_COLOR.withAlpha(0.3f));
        g.drawEllipse(peak3PointX - pointRadius, peak3PointY - pointRadius, 
                      pointRadius * 2.0f, pointRadius * 2.0f, 1.5f);
    }
}

void ResponseCurveComponent::resized()
{
    updateFilterCurve();
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

    // Attachments: sincronizan slider ↔ APVTS automáticamente
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
    // Añadir todos los hijos
    addAndMakeVisible(responseCurveComponent);
    addAndMakeVisible(defaultButton);

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

    setSize(900, 600);
}

EQAudioProcessorEditor::~EQAudioProcessorEditor() {}

//==============================================================================
void EQAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(18, 18, 22));

    // Dibujamos líneas verticales que separan las 5 secciones de parámetros
    // Las líneas van desde la parte superior del plugin hasta la inferior
    auto area = getLocalBounds().reduced(8);
    auto displayArea = area.withHeight(juce::roundToInt(area.getHeight() * 0.65f));
    int graphBottom = displayArea.getBottom();

    int totalWidth = getWidth() - 16;  // Restamos los 8px de margen en ambos lados
    int sectionWidth = totalWidth / 5;

    g.setColour(juce::Colour(75, 75, 85).withAlpha(0.6f));

    for (int i = 1; i < 5; ++i) {
        int x = 8 + i * sectionWidth;  // 8px de margen inicial
        g.drawVerticalLine(x, (float)graphBottom, (float)getHeight());
    }
}

void EQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(8);

    // 0. Botón Default en la esquina superior izquierda
    defaultButton.setBounds(8, 8, 80, 24);

    // 1. El visualizador ocupa el 65% superior
    auto displayArea = area.removeFromTop(juce::roundToInt(area.getHeight() * 0.65f));
    responseCurveComponent.setBounds(displayArea);

    // 2. Separación extra entre la gráfica y los controles
    // COMENTADO: movemos el buttonArea 35px más arriba eliminando esta separación
    // area.removeFromTop(15);

    // 3. Área de controles
    auto controlsArea = area;
    int maxSliderHeight = 130;
    if (controlsArea.getHeight() > maxSliderHeight) {
        controlsArea = controlsArea.withSizeKeepingCentre(controlsArea.getWidth(), maxSliderHeight);
    }

    // 4. Dividimos en 5 secciones iguales
    int sectionWidth = controlsArea.getWidth() / 5;

    // Los botones On/Off se solapan con la parte inferior del gráfico,
    // desplazados exactamente buttonAreaHeight píxeles hacia arriba.
    // Como son hijos del editor (no del responseCurveComponent),
    // se dibujan encima de él automáticamente.
    int buttonAreaHeight = 35;
    int buttonY = displayArea.getBottom();  // justo debajo del gráfico, en el límite

    // El área de sliders usa TODA la controlsArea (ya no cedemos 35px para botones)
    auto sliderArea = controlsArea;

    // Posicionamiento de botones On/Off
    // Aparecen sobre el gráfico (solapados), alineados con sus secciones
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

    // Combobox de LowCut Slope debajo, también bajado
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