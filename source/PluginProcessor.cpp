#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace brasa;

BrasaProcessor::BrasaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BRASA", createLayout())
{
    for (auto& d : paramTable())
    {
        auto* p = apvts.getRawParameterValue (d.id);
        jassert (p != nullptr);
        rawById[d.id] = p;
    }
}

bool BrasaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

Params BrasaProcessor::readParams() const
{
    return engineParamsFrom ([this] (const std::string& id)
    {
        auto it = rawById.find (id);
        return it != rawById.end() ? it->second->load() : 0.0f;
    });
}

void BrasaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate, std::max (64, samplesPerBlock));
    setLatencySamples (engine.latencySamples());
}

void BrasaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, n);
    const int nch = std::min (2, std::min (numIn, buffer.getNumChannels()));
    if (nch <= 0) return;

    engine.setParams (readParams());
    engine.process (buffer.getArrayOfWritePointers(), nch, n);
}

int BrasaProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

const juce::String BrasaProcessor::getProgramName (int index)
{
    const auto& list = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) list.size()) ? juce::String (list[(size_t) index].name) : juce::String();
}

void BrasaProcessor::setCurrentProgram (int index)
{
    const auto& list = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    currentProgram = index;
    const auto& pr = list[(size_t) index];
    for (auto& d : paramTable())
    {
        if (std::string (d.id) == pid::hq) continue;   // quality is a user choice, not part of a sound
        auto* param = apvts.getParameter (d.id);
        if (param == nullptr) continue;
        auto it = pr.values.find (d.id);
        const float v = it != pr.values.end() ? it->second : d.def;
        param->setValueNotifyingHost (param->convertTo0to1 (v));
    }
}

void BrasaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BrasaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* BrasaProcessor::createEditor() { return new BrasaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BrasaProcessor(); }
