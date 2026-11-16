// Renders the plugin editor to a PNG: BrasaShot <out.png> [program index]
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    BrasaProcessor proc;
    if (argc >= 3) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc >= 2 ? argv[1] : "shot.png"));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat png;
    return png.writeImageToStream (img, os) ? 0 : 1;
}
