/*
  ==============================================================================
    PluginEditor.h
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// ============================================================
//  Constantes FFT compartidas entre el analyzer y el editor
// ============================================================
static constexpr int FFT_ORDER = 11;           // 2^11 = 2048 puntos
static constexpr int FFT_SIZE = 1 << FFT_ORDER;

// ============================================================
//  SpectrumAnalyzer
//  Lee muestras del FIFO, aplica ventana Hann, ejecuta la FFT
//  y expone el array de bins normalizado (0..1) para el paint().
//  Su método process() es llamado por el Timer del ResponseCurveComponent.
// ============================================================
struct SpectrumAnalyzer
{
    SpectrumAnalyzer(SingleChannelSampleFifo<juce::AudioBuffer<float>>& fifo)
        : fifo(fifo),
        forwardFFT(FFT_ORDER),
        window(FFT_SIZE, juce::dsp::WindowingFunction<float>::hann)
    {
        fftData.assign(2 * FFT_SIZE, 0.0f);
        monoBuffer.resize(FFT_SIZE, 0.0f);
        drawingData.fill(0.0f);
    }

    static constexpr float NOISE_FLOOR = -72.0f;
    static constexpr float MAX_DB = 0.0f;
    static constexpr float ATTACK_COEFF = 0.30f;
    static constexpr float DECAY_COEFF = 0.08f;

    void process(double sampleRate)
    {
        currentSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;

        juce::AudioBuffer<float> tempBuffer(1, FFT_SIZE);
        bool hasNewData = false;

        while (fifo.getNextAudioBlock(tempBuffer))
        {
            auto* ptr = tempBuffer.getReadPointer(0);
            for (int i = 0; i < FFT_SIZE; ++i)
                monoBuffer[i] = ptr[i];

            hasNewData = true;
        }

        if (!hasNewData) return;

        std::fill(fftData.begin(), fftData.end(), 0.0f);
        std::copy(monoBuffer.begin(), monoBuffer.end(), fftData.begin());

        window.multiplyWithWindowingTable(fftData.data(), FFT_SIZE);
        forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());

        // Normalización de amplitud y conversión a decibelios
        for (int i = 0; i < FFT_SIZE / 2; ++i)
        {
            float mag = fftData[i] / (float)FFT_SIZE;
            float dB = juce::Decibels::gainToDecibels(mag);
            float val = juce::jlimit(0.0f, 1.0f, juce::jmap(dB, NOISE_FLOOR, MAX_DB, 0.0f, 1.0f));

            float coeff = (val > drawingData[i]) ? ATTACK_COEFF : DECAY_COEFF;
            drawingData[i] = val * coeff + drawingData[i] * (1.0f - coeff);
        }
    }

    std::array<float, FFT_SIZE / 2> getDrawingData() const { return drawingData; }

    float binToFreq(int bin) const
    {
        return (float)bin * (float)currentSampleRate / (float)FFT_SIZE;
    }

private:
    SingleChannelSampleFifo<juce::AudioBuffer<float>>& fifo;
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    std::vector<float> fftData;
    std::vector<float> monoBuffer;
    std::array<float, FFT_SIZE / 2> drawingData;
    double currentSampleRate{ 44100.0 };
};

// ============================================================
//  ResponseCurveComponent
//  Componente visual que:
//   1. Dibuja el espectro FFT (relleno verde semitransparente)
//   2. Dibuja la curva de magnitud de los filtros (línea blanca)
//  Es a la vez el Timer owner (a través del SpectrumAnalyzer).
// ============================================================
class ResponseCurveComponent : public juce::Component,
    public juce::AudioProcessorParameter::Listener,
    public juce::Timer
{
public:
    ResponseCurveComponent(EQAudioProcessor& p);
    ~ResponseCurveComponent() override;

    // AudioProcessorParameter::Listener
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int, bool) override {}

    // juce::Timer  (refresco de la curva de filtros cuando cambia un parámetro)
    void timerCallback() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    static constexpr int BOTTOM_MARGIN = 20;  // Reservado para etiquetas del eje X

    EQAudioProcessor& audioProcessor;

    // Flag atómico: un parámetro cambió → necesitamos recalcular la curva
    juce::Atomic<bool> parametersChanged{ false };

    // Curva de magnitud de los filtros — recalculada en el GUI thread
    // cuando cambia cualquier parámetro (sin tocar el audio thread)
    juce::Path filterCurvePath;

    // Posiciones de los puntos de los peaks (píxel X, píxel Y)
    float peakPointX{ 0.0f }, peakPointY{ 0.0f };
    float peak2PointX{ 0.0f }, peak2PointY{ 0.0f };
    float peak3PointX{ 0.0f }, peak3PointY{ 0.0f };

    // Analizador FFT
    SpectrumAnalyzer leftSpectrumAnalyzer;

    // Recalcula filterCurvePath a partir de los valores actuales del APVTS
    void updateFilterCurve();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResponseCurveComponent)
};

// ============================================================
//  Componente auxiliar: RotarySliderWithLabels
//  Un Slider rotatorio con etiqueta de nombre y unidad debajo.
// ============================================================
struct RotarySliderWithLabels : juce::Slider
{
    enum SliderType { Default, Peak1, Peak2, Peak3 };

    RotarySliderWithLabels(juce::RangedAudioParameter& param, const juce::String& unitSuffix, SliderType type = Default)
        : juce::Slider(juce::Slider::RotaryHorizontalVerticalDrag,
            juce::Slider::NoTextBox),
        param(&param),
        suffix(unitSuffix),
        sliderType(type),
        isBypassed(false)
    {
        setLookAndFeel(&lnf);
    }

    ~RotarySliderWithLabels()
    {
        setLookAndFeel(nullptr);
    }

    // LookAndFeel personalizado integrado en la clase
    struct LookAndFeel : juce::LookAndFeel_V4
    {
        void drawRotarySlider(juce::Graphics& g,
            int x, int y, int width, int height,
            float sliderPosProportional,
            float rotaryStartAngle,
            float rotaryEndAngle,
            juce::Slider&) override;
    };

    juce::String getDisplayString() const;

    void setBypassState(bool bypassed) 
    { 
        isBypassed = bypassed;
        repaint();
    }

    SliderType sliderType{ Default };
    bool isBypassed{ false };

private:
    LookAndFeel lnf;
    juce::RangedAudioParameter* param;
    juce::String suffix;
};

// ============================================================
//  EQAudioProcessorEditor
// ============================================================
class EQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    EQAudioProcessorEditor(EQAudioProcessor&);
    ~EQAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    EQAudioProcessor& audioProcessor;

    // --- Visualizador FFT + curva de filtros ---
    ResponseCurveComponent responseCurveComponent;

    // --- Botón Default ---
    juce::TextButton defaultButton { "Default" };

    // --- Botones On/Off para cada filtro ---
    juce::ToggleButton lowCutToggle { "LowCut" };
    juce::ToggleButton peakToggle { "Peak" };
    juce::ToggleButton peak2Toggle { "Peak2" };
    juce::ToggleButton peak3Toggle { "Peak3" };
    juce::ToggleButton highCutToggle { "HighCut" };

    // --- Sliders rotatorios ---
    RotarySliderWithLabels peakFreqSlider,
        peakGainSlider,
        peakQualitySlider,
        peak2FreqSlider,
        peak2GainSlider,
        peak2QualitySlider,
        peak3FreqSlider,
        peak3GainSlider,
        peak3QualitySlider,
        lowCutFreqSlider,
        highCutFreqSlider;

    // --- ComboBoxes para los slopes ---
    juce::ComboBox lowCutSlopeBox, highCutSlopeBox;

    // --- Attachments (conectan sliders/combos con el APVTS) ---
    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAt = APVTS::SliderAttachment;
    using ComboAt = APVTS::ComboBoxAttachment;

    SliderAt peakFreqAttachment,
        peakGainAttachment,
        peakQualityAttachment,
        peak2FreqAttachment,
        peak2GainAttachment,
        peak2QualityAttachment,
        peak3FreqAttachment,
        peak3GainAttachment,
        peak3QualityAttachment,
        lowCutFreqAttachment,
        highCutFreqAttachment;

    ComboAt lowCutSlopeAttachment,
        highCutSlopeAttachment;

    // --- Callback del botón Default ---
    void resetToDefaults();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQAudioProcessorEditor)
};