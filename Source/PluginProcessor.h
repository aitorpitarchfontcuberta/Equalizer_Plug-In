/*
  ==============================================================================
    PluginProcessor.h
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// ============================================================
//  Slope enum  (12 / 24 / 36 / 48 dB/oct)
// ============================================================
enum Slope { Slope_12, Slope_24, Slope_36, Slope_48 };

// ============================================================
//  ChainSettings  — snapshot de todos los parámetros
// ============================================================
struct ChainSettings
{
    float peakFreq          { 0 };
    float peakGainInDecibels{ 0 };
    float peakQuality       { 1.0f };
    float peak2Freq         { 0 };
    float peak2GainInDecibels{ 0 };
    float peak2Quality      { 1.0f };
    float peak3Freq         { 0 };
    float peak3GainInDecibels{ 0 };
    float peak3Quality      { 1.0f };
    float lowCutFreq        { 0 };
    float highCutFreq       { 0 };
    Slope lowCutSlope       { Slope::Slope_12 };
    Slope highCutSlope      { Slope::Slope_12 };
    bool  lowCutBypass      { false };
    bool  peakBypass        { false };
    bool  peak2Bypass       { false };
    bool  peak3Bypass       { false };
    bool  highCutBypass     { false };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

// ============================================================
//  Canal (usado como tag en el FIFO)
// ============================================================
enum Channel { Left, Right };

// ============================================================
//  SingleChannelSampleFifo
//  Permite empujar bloques de audio desde el audio-thread y
//  leerlos desde la GUI de forma lock-free mediante AbstractFifo.
// ============================================================
template<typename BlockType>
struct SingleChannelSampleFifo
{
    SingleChannelSampleFifo(Channel ch) : channelToUse(ch)
    {
        prepared.set(false);
    }

    // Llamado desde prepareToPlay (audio-thread, antes de procesar)
    void prepare(int bufferSize)
    {
        prepared.set(false);
        size.set(bufferSize);

        bufferToFill.setSize(1,            // 1 canal (mono)
                             bufferSize,
                             false,        // keepExistingContent
                             true,         // clearExtraSpace
                             true);        // avoidReallocating

        abstractFifo.setTotalSize(bufferSize);
        prepared.set(true);
    }

    // Llamado desde processBlock — copia el canal indicado al FIFO
    void update(const BlockType& buffer)
    {
        jassert(prepared.get());
        jassert(buffer.getNumChannels() > channelToUse);

        auto* channelPtr = buffer.getReadPointer(channelToUse);

        int start1, size1, start2, size2;
        abstractFifo.prepareToWrite(buffer.getNumSamples(),
                                    start1, size1, start2, size2);

        if (size1 > 0)
            bufferToFill.copyFrom(0, start1, channelPtr,        size1);
        if (size2 > 0)
            bufferToFill.copyFrom(0, start2, channelPtr + size1, size2);

        abstractFifo.finishedWrite(size1 + size2);
        numSamplesAvailable.set(abstractFifo.getNumReady());
    }

    // Llamado desde la GUI — devuelve true si había suficientes muestras
    bool getNextAudioBlock(BlockType& destination)
    {
        if (!prepared.get() ||
            abstractFifo.getNumReady() < destination.getNumSamples())
            return false;

        int start1, size1, start2, size2;
        abstractFifo.prepareToRead(destination.getNumSamples(),
                                   start1, size1, start2, size2);

        if (size1 > 0)
            destination.copyFrom(0, 0,     bufferToFill, 0, start1, size1);
        if (size2 > 0)
            destination.copyFrom(0, size1, bufferToFill, 0, start2, size2);

        abstractFifo.finishedRead(size1 + size2);
        numSamplesAvailable.set(abstractFifo.getNumReady());
        return true;
    }

    int  getNumSamplesAvailable() const { return numSamplesAvailable.get(); }
    bool isPrepared()             const { return prepared.get(); }
    int  getSize()                const { return size.get(); }

private:
    Channel              channelToUse;
    juce::AbstractFifo   abstractFifo { 1 };
    BlockType            bufferToFill;
    juce::Atomic<bool>   prepared     { false };
    juce::Atomic<int>    size         { 0 };
    juce::Atomic<int>    numSamplesAvailable { 0 };
};

// ============================================================
//  EQAudioProcessor
// ============================================================
class EQAudioProcessor : public juce::AudioProcessor
{
public:
    EQAudioProcessor();
    ~EQAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
#endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool  acceptsMidi()  const override;
    bool  producesMidi() const override;
    bool  isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ----------------------------------------------------------
    //  APVTS público (el editor lo necesita para los attachments
    //  y para recalcular la curva de filtros en la GUI)
    // ----------------------------------------------------------
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr,
                                               "Parameters",
                                               createParameterLayout() };

    // ----------------------------------------------------------
    //  FIFOs públicos: la GUI los lee para el análisis FFT
    // ----------------------------------------------------------
    SingleChannelSampleFifo<juce::AudioBuffer<float>> leftChannelFifo  { Channel::Left  };
    SingleChannelSampleFifo<juce::AudioBuffer<float>> rightChannelFifo { Channel::Right };

    
    // --- Tipos del procesador de filtros ---
    using Filter = juce::dsp::IIR::Filter<float>;
    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, Filter, Filter, CutFilter>;
    MonoChain leftChain, rightChain;

private:

    enum ChainPositions { LowCut, Peak, Peak2, Peak3, HighCut };

    // --- Actualización de filtros ---
    void updatePeakFilter     (const ChainSettings& s);
    void updatePeak2Filter    (const ChainSettings& s);
    void updatePeak3Filter    (const ChainSettings& s);
    void updateLowCutFilters  (const ChainSettings& s);
    void updateHighCutFilters (const ChainSettings& s);
    void updateFilters();

    using Coefficients = Filter::CoefficientsPtr;
    static void updateCoefficients(Coefficients& old, const Coefficients& replacements);

    template<int Index, typename ChainType, typename CoefficientType>
    void update(ChainType& chain, const CoefficientType& coefficients)
    {
        updateCoefficients(chain.template get<Index>().coefficients,
                           coefficients[Index]);
        chain.template setBypassed<Index>(false);
    }

    template<typename ChainType, typename CoefficientType>
    void updateCutFilter(ChainType& chain,
                         const CoefficientType& coefficients,
                         const Slope& slope)
    {
        chain.template setBypassed<0>(true);
        chain.template setBypassed<1>(true);
        chain.template setBypassed<2>(true);
        chain.template setBypassed<3>(true);

        switch (slope)
        {
            case Slope_48: update<3>(chain, coefficients); [[fallthrough]];
            case Slope_36: update<2>(chain, coefficients); [[fallthrough]];
            case Slope_24: update<1>(chain, coefficients); [[fallthrough]];
            case Slope_12: update<0>(chain, coefficients); break;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQAudioProcessor)
};
