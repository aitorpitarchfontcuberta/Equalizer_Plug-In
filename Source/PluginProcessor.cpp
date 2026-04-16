/*
  ==============================================================================
    PluginProcessor.cpp
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
EQAudioProcessor::EQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

EQAudioProcessor::~EQAudioProcessor() {}

//==============================================================================
const juce::String EQAudioProcessor::getName() const { return JucePlugin_Name; }

bool EQAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool EQAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool EQAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double EQAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int    EQAudioProcessor::getNumPrograms()             { return 1; }
int    EQAudioProcessor::getCurrentProgram()          { return 0; }
void   EQAudioProcessor::setCurrentProgram (int)     {}
const  juce::String EQAudioProcessor::getProgramName (int) { return {}; }
void   EQAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void EQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels      = 1;
    spec.sampleRate       = sampleRate;

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    updateFilters();

    // -----------------------------------------------------------------
    //  Prepara los FIFOs con el tamaño de bloque actual.
    //  Usamos fftSize (2048) para que siempre haya suficientes muestras
    //  para un análisis FFT completo.
    // -----------------------------------------------------------------
        leftChannelFifo.prepare(8192);
        rightChannelFifo.prepare(8192);
}

void EQAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif
    return true;
#endif
}
#endif

void EQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    updateFilters();

    juce::dsp::AudioBlock<float> block(buffer);

    auto leftBlock  = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    leftChain.process(juce::dsp::ProcessContextReplacing<float>(leftBlock));
    rightChain.process(juce::dsp::ProcessContextReplacing<float>(rightBlock));

    // -----------------------------------------------------------------
    //  Empuja las muestras YA PROCESADAS al FIFO para el análisis FFT.
    //  Lo hacemos DESPUÉS de procesar los filtros para que el espectro
    //  refleje la señal de SALIDA (post-EQ).
    // -----------------------------------------------------------------
    leftChannelFifo.update(buffer);
    rightChannelFifo.update(buffer);
}

//==============================================================================
bool EQAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* EQAudioProcessor::createEditor()
{
    return new EQAudioProcessorEditor(*this);
    // Descomenta la línea anterior cuando tengas el editor implementado.
    // return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void EQAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {}
void EQAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {}

//==============================================================================
//  getChainSettings — lee los valores raw del APVTS
//==============================================================================
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;

    settings.peakFreq           = apvts.getRawParameterValue("Peak Frequency")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality        = apvts.getRawParameterValue("Peak Quality")->load();
    settings.peak2Freq          = apvts.getRawParameterValue("Peak2 Frequency")->load();
    settings.peak2GainInDecibels= apvts.getRawParameterValue("Peak2 Gain")->load();
    settings.peak2Quality       = apvts.getRawParameterValue("Peak2 Quality")->load();
    settings.peak3Freq          = apvts.getRawParameterValue("Peak3 Frequency")->load();
    settings.peak3GainInDecibels= apvts.getRawParameterValue("Peak3 Gain")->load();
    settings.peak3Quality       = apvts.getRawParameterValue("Peak3 Quality")->load();
    settings.lowCutFreq         = apvts.getRawParameterValue("LowCut Frequency")->load();
    settings.highCutFreq        = apvts.getRawParameterValue("HighCut Frequency")->load();
    settings.lowCutSlope  = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());
    settings.lowCutBypass  = apvts.getRawParameterValue("LowCut Bypass")->load() > 0.5f;
    settings.peakBypass    = apvts.getRawParameterValue("Peak Bypass")->load() > 0.5f;
    settings.peak2Bypass   = apvts.getRawParameterValue("Peak2 Bypass")->load() > 0.5f;
    settings.peak3Bypass   = apvts.getRawParameterValue("Peak3 Bypass")->load() > 0.5f;
    settings.highCutBypass = apvts.getRawParameterValue("HighCut Bypass")->load() > 0.5f;

    return settings;
}

//==============================================================================
//  Actualización de filtros
//==============================================================================
void EQAudioProcessor::updatePeakFilter(const ChainSettings& s)
{
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        getSampleRate(), s.peakFreq, s.peakQuality,
        juce::Decibels::decibelsToGain(s.peakGainInDecibels));

    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients,  peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void EQAudioProcessor::updatePeak2Filter(const ChainSettings& s)
{
    auto peak2Coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        getSampleRate(), s.peak2Freq, s.peak2Quality,
        juce::Decibels::decibelsToGain(s.peak2GainInDecibels));

    updateCoefficients(leftChain.get<ChainPositions::Peak2>().coefficients,  peak2Coefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak2>().coefficients, peak2Coefficients);
}

void EQAudioProcessor::updatePeak3Filter(const ChainSettings& s)
{
    auto peak3Coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        getSampleRate(), s.peak3Freq, s.peak3Quality,
        juce::Decibels::decibelsToGain(s.peak3GainInDecibels));

    updateCoefficients(leftChain.get<ChainPositions::Peak3>().coefficients,  peak3Coefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak3>().coefficients, peak3Coefficients);
}

void EQAudioProcessor::updateCoefficients(Coefficients& old, const Coefficients& replacements)
{
    *old = *replacements;
}

void EQAudioProcessor::updateLowCutFilters(const ChainSettings& s)
{
    auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
        s.lowCutFreq, getSampleRate(), 2 * (s.lowCutSlope + 1));

    auto& leftLowCut  = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    updateCutFilter(leftLowCut,  cutCoefficients, s.lowCutSlope);
    updateCutFilter(rightLowCut, cutCoefficients, s.lowCutSlope);
}

void EQAudioProcessor::updateHighCutFilters(const ChainSettings& s)
{
    auto highCutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
        s.highCutFreq, getSampleRate(), 2 * (s.highCutSlope + 1));

    auto& leftHighCut  = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    updateCutFilter(leftHighCut,  highCutCoefficients, s.highCutSlope);
    updateCutFilter(rightHighCut, highCutCoefficients, s.highCutSlope);
}

void EQAudioProcessor::updateFilters()
{
    auto chainSettings = getChainSettings(apvts);

    // Aplicar bypass a los filtros
    leftChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypass);
    rightChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypass);

    leftChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypass);
    rightChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypass);

    leftChain.setBypassed<ChainPositions::Peak2>(chainSettings.peak2Bypass);
    rightChain.setBypassed<ChainPositions::Peak2>(chainSettings.peak2Bypass);

    leftChain.setBypassed<ChainPositions::Peak3>(chainSettings.peak3Bypass);
    rightChain.setBypassed<ChainPositions::Peak3>(chainSettings.peak3Bypass);

    leftChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypass);
    rightChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypass);

    updatePeakFilter(chainSettings);
    updatePeak2Filter(chainSettings);
    updatePeak3Filter(chainSettings);
    updateLowCutFilters(chainSettings);
    updateHighCutFilters(chainSettings);
}

//==============================================================================
//  createParameterLayout
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout EQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "LowCut Frequency", "Low Cut Frequency",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 20.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "HighCut Frequency", "HighCut Frequency",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 20000.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Frequency", "Peak Frequency",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 100.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak2 Frequency", "Peak2 Frequency",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 750.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak2 Gain", "Peak2 Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.5f, 1.0f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak2 Quality", "Peak2 Quality",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.05f, 1.0f), 1.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak3 Frequency", "Peak3 Frequency",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 5000.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak3 Gain", "Peak3 Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.5f, 1.0f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak3 Quality", "Peak3 Quality",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.05f, 1.0f), 1.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Gain", "Peak Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.5f, 1.0f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Quality", "Peak Quality",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.05f, 1.0f), 1.0f));

    juce::StringArray stringArray;
    for (int i = 0; i < 4; ++i)
    {
        juce::String str;
        str << (12 + i * 12) << " dB/Oct";
        stringArray.add(str);
    }

    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope",  "LowCut Slope",  stringArray, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0));

    // --- Parámetros de bypass ---
    layout.add(std::make_unique<juce::AudioParameterBool>("LowCut Bypass",  "LowCut Bypass",  false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Peak Bypass",    "Peak Bypass",    false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Peak2 Bypass",   "Peak2 Bypass",   false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Peak3 Bypass",   "Peak3 Bypass",   false));
    layout.add(std::make_unique<juce::AudioParameterBool>("HighCut Bypass", "HighCut Bypass", false));

    return layout;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQAudioProcessor();
}
