/*
  ==============================================================================
    PluginEditor.cpp
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
//  Helpers de layout  (frecuencia → posición X logarítmica)
//==============================================================================
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
    auto bounds  = Rectangle<float>(x, y, width, height - labelH);
    auto radius  = jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto center  = bounds.getCentre();

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

    // --- Arco de valor (azul-cian) ---
    float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    Path arcVal;
    arcVal.addCentredArc(center.x, center.y, trackR, trackR,
                         0.0f, rotaryStartAngle, angle, true);
    g.setColour(Colour(0, 175, 215));
    g.strokePath(arcVal, PathStrokeType(3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    // --- Línea indicadora (desde centro hasta borde interior) ---
    auto thumbR = radius - 11.0f;
    Point<float> thumbPt(center.x + thumbR * std::sin(angle),
                         center.y - thumbR * std::cos(angle));
    g.setColour(Colour(210, 210, 210));
    g.drawLine(Line<float>(center, thumbPt), 2.0f);

    // --- Punto central ---
    g.setColour(Colour(180, 180, 180));
    g.fillEllipse(Rectangle<float>(5.0f, 5.0f).withCentre(center));

    // --- Valor numérico (centrado en el knob) ---
    if (auto* rsw = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        g.setColour(Colour(200, 200, 200));
        g.setFont(FontOptions(10.5f));
        g.drawFittedText(rsw->getDisplayString(),
                         bounds.withSizeKeepingCentre(radius * 1.4f, radius * 0.6f).toNearestInt(),
                         Justification::centred, 1);

        // --- Nombre del parámetro debajo del knob ---
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
        val  /= 1000.0f;
        addK  = true;
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
    double sampleRate  = audioProcessor.getSampleRate();
    if (sampleRate <= 0.0) sampleRate = 44100.0;

    auto bounds = getLocalBounds().toFloat();
    auto width  = bounds.getWidth();

    filterCurvePath.clear();

    // Para cada píxel X calculamos la magnitud total de la cadena de filtros
    for (int px = 0; px < (int)width; ++px)
    {
        double mag = 1.0;

        // Frecuencia correspondiente a este píxel (escala log)
        float freq = std::pow(10.0f,
            juce::jmap((float)px, 0.0f, width - 1,
                       std::log10(20.0f), std::log10(20000.0f)));

        // --- Peak filter ---
        auto peakCoeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate, chainSettings.peakFreq, chainSettings.peakQuality,
            juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
        mag *= peakCoeff->getMagnitudeForFrequency(freq, sampleRate);

        // --- LowCut filter (Butterworth, acumula los stages activos) ---
        auto lowCutCoeffs = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
            chainSettings.lowCutFreq, sampleRate, 2 * (chainSettings.lowCutSlope + 1));
        for (int stage = 0; stage <= chainSettings.lowCutSlope; ++stage)
            mag *= lowCutCoeffs[stage]->getMagnitudeForFrequency(freq, sampleRate);

        // --- HighCut filter ---
        auto highCutCoeffs = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
            chainSettings.highCutFreq, sampleRate, 2 * (chainSettings.highCutSlope + 1));
        for (int stage = 0; stage <= chainSettings.highCutSlope; ++stage)
            mag *= highCutCoeffs[stage]->getMagnitudeForFrequency(freq, sampleRate);

        // Convierte magnitud a dB y mapea al eje Y (±24 dB range)
        float dB = (float)juce::Decibels::gainToDecibels(mag);
        float y  = juce::jmap(dB, -24.0f, 24.0f,
                               bounds.getBottom(), bounds.getY());

        if (px == 0)
            filterCurvePath.startNewSubPath(bounds.getX(), y);
        else
            filterCurvePath.lineTo(bounds.getX() + px, y);
    }
}

void ResponseCurveComponent::paint(juce::Graphics& g)
{
    using namespace juce;

    auto bounds = getLocalBounds();
    auto w = (float)bounds.getWidth();
    auto h = (float)bounds.getHeight();

    // --- Fondo ---
    g.setColour(Colour(20, 20, 25));
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    // --- Grid de frecuencias (líneas verticales) ---
    g.setColour(Colour(50, 50, 55));
    const float gridFreqs[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    for (auto freq : gridFreqs)
    {
        float x = mapFreqToX(freq, 0.0f, w);
        g.drawVerticalLine((int)x, 0.0f, h);

        // Etiqueta
        g.setColour(Colour(70, 70, 75));
        g.setFont(FontOptions(9.0f));
        juce::String label = freq < 1000 ? juce::String((int)freq)
                                         : juce::String((int)(freq / 1000)) + "k";
        g.drawText(label, (int)x - 12, (int)h - 14, 24, 12, Justification::centred);
        g.setColour(Colour(50, 50, 55));
    }

    // --- Grid de ganancia (líneas horizontales: -24, -12, 0, +12, +24 dB) ---
    const float gridDBs[] = { -24.0f, -12.0f, 0.0f, 12.0f, 24.0f };
    for (auto dB : gridDBs)
    {
        float y = jmap(dB, -24.0f, 24.0f, h, 0.0f);
        g.setColour(dB == 0.0f ? Colour(80, 80, 90) : Colour(45, 45, 50));
        g.drawHorizontalLine((int)y, 0.0f, w);

        // Etiqueta
        g.setColour(Colour(70, 70, 75));
        g.setFont(FontOptions(9.0f));
        juce::String label = (dB > 0 ? "+" : "") + juce::String((int)dB) + "dB";
        g.drawText(label, 2, (int)y - 6, 28, 12, Justification::left);
    }

    // ---------------------------------------------------------------
    //  1. ESPECTRO FFT  (relleno semitransparente + borde brillante)
    // ---------------------------------------------------------------
    {
        auto& data = leftSpectrumAnalyzer.drawingData;
        int numBins = (int)data.size();  // FFT_SIZE / 2

        Path spectrumPath;
        bool started = false;

        for (int bin = 1; bin < numBins; ++bin)
        {
            float freq = leftSpectrumAnalyzer.binToFreq(bin);
            if (freq < 20.0f || freq > 20000.0f) continue;

            float x = mapFreqToX(freq, 0.0f, w);
            float y = jmap(data[bin], 0.0f, 1.0f, h, 0.0f);

            if (!started)
            {
                spectrumPath.startNewSubPath(x, h);
                spectrumPath.lineTo(x, y);
                started = true;
            }
            else
            {
                spectrumPath.lineTo(x, y);
            }
        }

        if (started)
        {
            spectrumPath.lineTo(w, h);
            spectrumPath.closeSubPath();

            // Relleno semitransparente (verde-azulado)
            g.setColour(Colour(0, 200, 150).withAlpha(0.12f));
            g.fillPath(spectrumPath);

            // Borde del espectro
            g.setColour(Colour(0, 220, 160).withAlpha(0.55f));
            g.strokePath(spectrumPath, PathStrokeType(1.0f));
        }
    }

    // ---------------------------------------------------------------
    //  2. CURVA DE FILTROS  (línea blanca sobre el espectro)
    // ---------------------------------------------------------------
    g.setColour(Colour(220, 220, 220));
    g.strokePath(filterCurvePath, PathStrokeType(2.0f,
                                                  PathStrokeType::curved,
                                                  PathStrokeType::rounded));

    // --- Borde exterior del componente ---
    g.setColour(Colour(80, 80, 90));
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 4.0f, 1.0f);
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
      peakFreqSlider   (*p.apvts.getParameter("Peak Frequency"),  "Hz"),
      peakGainSlider   (*p.apvts.getParameter("Peak Gain"),       "dB"),
      peakQualitySlider(*p.apvts.getParameter("Peak Quality"),    ""),
      lowCutFreqSlider (*p.apvts.getParameter("LowCut Frequency"),"Hz"),
      highCutFreqSlider(*p.apvts.getParameter("HighCut Frequency"),"Hz"),

      // Attachments: sincronizan slider ↔ APVTS automáticamente
      peakFreqAttachment   (p.apvts, "Peak Frequency",   peakFreqSlider),
      peakGainAttachment   (p.apvts, "Peak Gain",        peakGainSlider),
      peakQualityAttachment(p.apvts, "Peak Quality",     peakQualitySlider),
      lowCutFreqAttachment (p.apvts, "LowCut Frequency", lowCutFreqSlider),
      highCutFreqAttachment(p.apvts, "HighCut Frequency",highCutFreqSlider),
      lowCutSlopeAttachment(p.apvts, "LowCut Slope",     lowCutSlopeBox),
      highCutSlopeAttachment(p.apvts,"HighCut Slope",    highCutSlopeBox)
{
    // Añadir todos los hijos
    addAndMakeVisible(responseCurveComponent);

    for (auto* slider : { &peakFreqSlider, &peakGainSlider, &peakQualitySlider,
                          &lowCutFreqSlider, &highCutFreqSlider })
        addAndMakeVisible(slider);

    addAndMakeVisible(lowCutSlopeBox);
    addAndMakeVisible(highCutSlopeBox);

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

    setSize(600, 480);
}

EQAudioProcessorEditor::~EQAudioProcessorEditor() {}

//==============================================================================
void EQAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(18, 18, 22));
}

void EQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(8);

    // El visualizador ocupa la mitad superior (ajusta el ratio a tu gusto)
    auto displayArea  = area.removeFromTop(area.getHeight() / 2);
    responseCurveComponent.setBounds(displayArea);

    // Espacio para los controles
    area.removeFromTop(8);  // margen

    // Fila superior de sliders: LowCut | Peak x3 | HighCut
    auto controlsArea = area;
    int sliderW = controlsArea.getWidth() / 5;

    lowCutFreqSlider .setBounds(controlsArea.removeFromLeft(sliderW));
    peakFreqSlider   .setBounds(controlsArea.removeFromLeft(sliderW));
    peakGainSlider   .setBounds(controlsArea.removeFromLeft(sliderW));
    peakQualitySlider.setBounds(controlsArea.removeFromLeft(sliderW));
    highCutFreqSlider.setBounds(controlsArea);

    // ComboBoxes de slope — debajo de los sliders extremos
    // (los posicionamos de forma absoluta respecto al editor)
    int comboH = 24;
    int comboY = getHeight() - comboH - 8;

    lowCutSlopeBox .setBounds(8,              comboY, 120, comboH);
    highCutSlopeBox.setBounds(getWidth() - 128, comboY, 120, comboH);
}
