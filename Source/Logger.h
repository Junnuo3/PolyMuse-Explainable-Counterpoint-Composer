#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

class JsonlLogger {
public:
    explicit JsonlLogger(const juce::File& f): file(f){ file.create(); }
    void log(const juce::var& obj){
        juce::FileOutputStream os(file, 1024);
        if (os.openedOk()){
            os.setPosition(file.getSize());
            os.writeText(juce::JSON::toString(obj) + "\n", false, false, nullptr);
        }
    }
private:
    juce::File file;
};
