#ifndef _INSTRUMENT_
#define _INSTRUMENT_

#include <array>
#include "midi.hpp"
#include "synthesis.hpp"

class Instrument {
    public:
        explicit Instrument(const float &samplingRate = 44100.f) : samplingRate_(samplingRate) {}
        virtual ~Instrument() = default;
        virtual void setSamplingRate(const float &samplingRate) = 0;
        virtual void process(const MidiEvent &event) = 0;
        virtual float render() = 0;
        virtual void reset() = 0;

    protected:
        float samplingRate_ = 44100;
        std::array<uint8_t, 128> active_{};
		std::array<bool, 128> indices_{};
        std::size_t activeCounter_ = 0;
};

template<typename Voice>
class VirtualInstrument: public Instrument {
    public:
        explicit VirtualInstrument(const float &samplingRate = 44100.f) : notes_(init(samplingRate, std::make_index_sequence<128>())) {
            samplingRate_ = samplingRate;
        }

        void setSamplingRate(const float &samplingRate) override {
            samplingRate_ = samplingRate;
            for (auto &note: notes_) {
                note.setSamplingRate(samplingRate_);
            }
        }

        void process(const MidiEvent &event) override {
            switch (event.type) {
                case MidiEventType::NoteOn:
                    if (event.data1 < 128) {
                        if (event.data2 == 0) {
                            notes_[event.data1].noteOff();
                        } else {
                            if (!indices_[event.data1]) {
                                active_[activeCounter_++] = event.data1;
								indices_[event.data1] = true;
                            }
                            notes_[event.data1].noteOn(event.data1, event.data2);
                        }
                    }
                    break;

                case MidiEventType::NoteOff:
                    if (event.data1 < 128) {
                        notes_[event.data1].noteOff();
                    }
                    break;

                case MidiEventType::ControlChange:
                    break;
            }
        }

        float render() override {
            float output = 0;

            size_t i = 0;
            while (i < activeCounter_) {
                const uint8_t note = active_[i];

                if (!notes_[note].active()) {
					indices_[note] = false;
                    --activeCounter_;
                    active_[i] = active_[activeCounter_];
                } else {
                    output += notes_[note].render();
                    ++i;
                }
            }

            return output;
        }

        void reset() override {
            for (size_t i = 0; i < activeCounter_; ++i) {
                notes_[active_[i]].reset();
            }
			indices_.fill(false);
            activeCounter_ = 0;
        };

    private:
        template<size_t... I>
        static std::array<Voice, 128> init(const float &samplingRate, std::index_sequence<I...>) {
            return { Voice(samplingRate, (uint8_t) I)... };
        }
        std::array<Voice, 128> notes_;
};

using VirtualPiano = VirtualInstrument<Piano>;
using VirtualGuitar = VirtualInstrument<Guitar>;

#endif
