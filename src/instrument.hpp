#ifndef INSTRUMENT
#define INSTRUMENT

#include <array>
#include "midi.hpp"
#include "synthesis.hpp"

class Instrument {
    public:
        explicit Instrument(const float samplingRate = 44100.f) : samplingRate_(samplingRate) {}
        virtual ~Instrument() = default;
        virtual void setSamplingRate(const float samplingRate) = 0;
        virtual void process(const MidiEvent &event) = 0;
        virtual float render() = 0;
        virtual void transpose(const int a) = 0;
        virtual void reset() = 0;

    protected:
		static constexpr int C3 = 48;
		static constexpr size_t VOICE_RANGE = 21;

        float samplingRate_ = 44100;
        std::array<uint8_t, VOICE_RANGE> active_{};
		std::array<bool, VOICE_RANGE> indices_{};
        std::size_t activeCounter_ = 0;
};

template<typename Voice>
class VirtualInstrument: public Instrument {
    public:
        explicit VirtualInstrument(const float samplingRate = 44100.f) : notes_(init(samplingRate, std::make_index_sequence<VOICE_RANGE>())) {
            samplingRate_ = samplingRate;
        }

        void setSamplingRate(const float samplingRate) override {
            samplingRate_ = samplingRate;
            for (auto &note: notes_) {
                note.setSamplingRate(samplingRate_);
            }
        }

        void process(const MidiEvent &event) override {
			const size_t idx = (size_t) event.data1 - C3;

            switch (event.type) {
                case MidiEventType::NoteOn:
					if (event.data2 == 0) {
						notes_[idx].noteOff();
					} else {
						if (!indices_[idx]) {
							active_[activeCounter_++] = (uint8_t) idx;
							indices_[idx] = true;
						}
						notes_[idx].transpose(transpose_);
						notes_[idx].noteOn(event.data1, event.data2);
					}
                    break;

                case MidiEventType::NoteOff:
					notes_[idx].noteOff();
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

        void transpose(const int a) override {
            transpose_ = a;

            for (size_t i = 0; i < activeCounter_; ++i) {
                notes_[active_[i]].transpose(a);
            }
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
        static std::array<Voice, VOICE_RANGE> init(const float &samplingRate, std::index_sequence<I...>) {
            return {
				Voice(samplingRate, (uint8_t) (C3 + I))...
			};
        }
        std::array<Voice, VOICE_RANGE> notes_;
        int transpose_ = 0;
};

using VirtualPiano = VirtualInstrument<Piano>;
using VirtualGuitar = VirtualInstrument<Guitar>;

#endif
