#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>
#include "Engine.h"
#include "ParamTable.h"
#include "Params.h"

class BrasaProcessor : public juce::AudioProcessor
{
public:
    BrasaProcessor();
    ~BrasaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Brasa"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // editor helpers
    float getHeatDb() const { return engine.heatDb(); }
    float getAutoGainDb() const { return engine.autoGainDb(); }

private:
    brasa::Params readParams() const;

    brasa::Engine engine;
    std::unordered_map<std::string, std::atomic<float>*> rawById;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrasaProcessor)
};
