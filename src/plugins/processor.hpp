#ifndef PROCESSOR
#define PROCESSOR

#include <array>
#include "midi.hpp"

class Processor {
    public:        
        virtual ~Processor() = default;

        virtual MidiEventBuffer process(const MidiEvent &event) {
            return MidiEventBuffer(event);
        }
        virtual float process(const float sample) {
            return sample;
        }
        virtual void enable(const bool a) = 0;
        virtual void reset() = 0;
};

#endif
