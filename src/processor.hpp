#ifndef _PROCESSOR_
#define _PROCESSOR_

#include <array>
#include "midi.hpp"

struct State {
    virtual ~State() = default;
	virtual void reset() = 0;
};

class Processor {
    public:
        virtual ~Processor() = default;

        virtual MidiEventBuffer process(const MidiEvent &event) {
            return MidiEventBuffer(event);
        }
        virtual float process(const float &sample) {
            return sample;
        }
        virtual void enable(const bool &a) = 0;
        virtual void reset() = 0;
};

#endif
