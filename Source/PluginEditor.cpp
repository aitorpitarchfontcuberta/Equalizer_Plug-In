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
        float y = juce::jmap(dB, -24.0f, 24.0f,
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
    g.fillAll(Colours::black);

    auto w = getWidth();
    auto h = getHeight();

    // =====================================================================
    // NUEVO: Reservamos 20 píxeles abajo para las etiquetas del eje X
    // =====================================================================
    int bottomMargin = 20;
    float graphHeight = (float)(h - bottomMargin);

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

    // --- Eje Y Dual (FFT Izquierda / Filtros Derecha) ---
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
    juce::Path responseCurve;
    std::vector<double> mags(w);

    for (int i = 0; i < w; ++i)
    {
        double mag = 1.0;
        auto freq = mapXToFreq((float)i, 0.0f, (float)w);
        auto& chain = audioProcessor.leftChain;

        if (!chain.isBypassed<1>()) mag *= chain.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

        if (!chain.isBypassed<0>()) {
            auto& lowcut = chain.get<0>();
            if (!lowcut.isBypassed<0>()) mag *= lowcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowcut.isBypassed<1>()) mag *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowcut.isBypassed<2>()) mag *= lowcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowcut.isBypassed<3>()) mag *= lowcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        if (!chain.isBypassed<2>()) {
            auto& highcut = chain.get<2>();
            if (!highcut.isBypassed<0>()) mag *= highcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highcut.isBypassed<1>()) mag *= highcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highcut.isBypassed<2>()) mag *= highcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highcut.isBypassed<3>()) mag *= highcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        if (std::isnan(mag) || std::isinf(mag))
            mag = (i > 0) ? mags[i - 1] : 1.0;

        mags[i] = mag;
    }

    bool firstPoint = true;
    for (int i = 0; i < w; ++i)
    {
        auto magDB = juce::Decibels::gainToDecibels(mags[i]);

        // Mapeamos de -24dB a +24dB
        auto y = juce::jmap((float)magDB, -24.0f, 24.0f, graphHeight, 0.0f);

        
        y = juce::jlimit(1.0f, graphHeight - 1.0f, y);

        if (firstPoint) {
            responseCurve.startNewSubPath((float)i, y);
            firstPoint = false;
        }
        else {
            responseCurve.lineTo((float)i, y);
        }
    }

    // Como ahora la matemática es perfecta, podemos quitar el saveState() y el reduceClipRegion()
    g.setColour(juce::Colours::white);
    g.strokePath(responseCurve, juce::PathStrokeType(2.0f));

    g.saveState();
    g.reduceClipRegion(0, 0, w, (int)graphHeight); // Nada se dibujará por debajo del eje X (o por arriba)

    g.setColour(juce::Colours::white);
    g.strokePath(responseCurve, juce::PathStrokeType(2.0f));

    g.restoreState(); // Restauramos el estado normal de dibujo

    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.0f));
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
    peakFreqSlider(*p.apvts.getParameter("Peak Frequency"), "Hz"),
    peakGainSlider(*p.apvts.getParameter("Peak Gain"), "dB"),
    peakQualitySlider(*p.apvts.getParameter("Peak Quality"), ""),
    lowCutFreqSlider(*p.apvts.getParameter("LowCut Frequency"), "Hz"),
    highCutFreqSlider(*p.apvts.getParameter("HighCut Frequency"), "Hz"),

    // Attachments: sincronizan slider ↔ APVTS automáticamente
    peakFreqAttachment(p.apvts, "Peak Frequency", peakFreqSlider),
    peakGainAttachment(p.apvts, "Peak Gain", peakGainSlider),
    peakQualityAttachment(p.apvts, "Peak Quality", peakQualitySlider),
    lowCutFreqAttachment(p.apvts, "LowCut Frequency", lowCutFreqSlider),
    highCutFreqAttachment(p.apvts, "HighCut Frequency", highCutFreqSlider),
    lowCutSlopeAttachment(p.apvts, "LowCut Slope", lowCutSlopeBox),
    highCutSlopeAttachment(p.apvts, "HighCut Slope", highCutSlopeBox)
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

    setSize(900, 600);
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

    // 1. El visualizador ocupa el 65% superior
    auto displayArea = area.removeFromTop(juce::roundToInt(area.getHeight() * 0.65f));
    responseCurveComponent.setBounds(displayArea);

    // 2. Separación extra entre la gráfica y los controles
    area.removeFromTop(15);

    // 3. Forzamos una altura máxima para las ruedas para que no se vean gigantes
    auto controlsArea = area;
    int maxSliderHeight = 110; // Prueba con 110, 100 o 90 px según tu gusto
    if (controlsArea.getHeight() > maxSliderHeight) {
        // Centra el rectángulo de controles si nos sobra espacio vertical
        controlsArea = controlsArea.withSizeKeepingCentre(controlsArea.getWidth(), maxSliderHeight);
    }

    int sliderW = controlsArea.getWidth() / 5;

    // Distribuimos los sliders
    lowCutFreqSlider.setBounds(controlsArea.removeFromLeft(sliderW));
    peakFreqSlider.setBounds(controlsArea.removeFromLeft(sliderW));
    peakGainSlider.setBounds(controlsArea.removeFromLeft(sliderW));
    peakQualitySlider.setBounds(controlsArea.removeFromLeft(sliderW));
    highCutFreqSlider.setBounds(controlsArea);

    // 4. Ubicación de los comboboxes de Slope
    // Los colocamos justo debajo de sus respectivos controles de LowCut y HighCut
    int comboH = 20;

    // LowCut ComboBox debajo del LowCut Freq
    auto lowCutBounds = lowCutFreqSlider.getBounds();
    lowCutSlopeBox.setBounds(lowCutBounds.getX() + 10, lowCutBounds.getBottom() + 5, lowCutBounds.getWidth() - 20, comboH);

    // HighCut ComboBox debajo del HighCut Freq
    auto highCutBounds = highCutFreqSlider.getBounds();
    highCutSlopeBox.setBounds(highCutBounds.getX() + 10, highCutBounds.getBottom() + 5, highCutBounds.getWidth() - 20, comboH);
}