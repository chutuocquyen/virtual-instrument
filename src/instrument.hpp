#ifndef _INSTRUMENT_
#define _INSTRUMENT_

#include <array>
#include <vector>
#include "midi.hpp"
#include "synthesis.hpp"

class Instrument {
    public:
        virtual ~Instrument() = default;
        virtual std::vector<MidiEvent> process(const MidiEvent& event) = 0;
        virtual float render() = 0;
        virtual void reset() = 0;
};

class VirtualPiano : public Instrument {
    public:
        explicit VirtualPiano(double samplingRate = 44100) : samplingRate_(samplingRate) {
            for (auto& note: notes_) {
                note.setSamplingRate(samplingRate_);
            }
        };

        std::vector<MidiEvent> process(const MidiEvent& event) override {
            switch (event.type) {
                case MidiEventType::NoteOn:
                    if (event.data1 < notes_.size()) {
                        if (event.data2 == 0) {
                            notes_[event.data1].noteOff();
                        } else {
                            notes_[event.data1].noteOn(event.data1, event.data2);
                        }
                    }
                    return {};

                case MidiEventType::NoteOff:
                    if (event.data1 < notes_.size()) {
                        notes_[event.data1].noteOff();
                    }
                    return {};

                case MidiEventType::ControlChange:
                    return {};
            }
        }

        float render() override {
            float output = 0;

            for (auto& note: notes_) {
                output += note.render();
            }

            return output;
        };

        void reset() override {
            for (auto& note: notes_) {
                note = Piano(samplingRate_);
            }
        };

    private:
        double samplingRate_ = 44100;
        std::array<Piano, 128> notes_{};
};

#endif