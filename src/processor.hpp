#ifndef _PROCESSOR_
#define _PROCESSOR_

#include <array>
#include <vector>
#include "midi.hpp"

constexpr uint8_t ANZAHL_NOTES = 128;

struct State {
    virtual ~State() = default;
	virtual void reset() = 0;
};

class Processor {
    public:
        virtual ~Processor() = default;

        virtual std::vector<MidiEvent> process(const MidiEvent& event) = 0;
        virtual void reset() = 0;
};

#endif
