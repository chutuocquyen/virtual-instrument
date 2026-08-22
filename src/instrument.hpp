#ifndef _INSTRUMENT_
#define _INSTRUMENT_

#include <array>
#include <vector>
#include "midi.hpp"
#include "synthesis.hpp"

class Instrument {
    public:
        virtual ~Instrument() = default;
        virtual void setSamplingRate(const double &samplingRate) = 0;
        virtual void process(const MidiEvent &event) = 0;
        virtual float render() = 0;
        virtual void reset() = 0;

    protected:
        double samplingRate_ = 44100;
        std::array<uint8_t, 128> active_{};
        std::size_t activeCounter_ = 0;
};

class VirtualPiano : public Instrument {
    public:
        explicit VirtualPiano(const double samplingRate = 44100) {
            setSamplingRate(samplingRate);
        };

        void setSamplingRate(const double &samplingRate) override {
            samplingRate_ = samplingRate;
            for (auto &note: notes_) {
                note.setSamplingRate(samplingRate_);
            }
        }

        void process(const MidiEvent &event) override {
            switch (event.type) {
                case MidiEventType::NoteOn:
                    if (event.data1 < notes_.size()) {
                        if (event.data2 == 0) {
                            notes_[event.data1].noteOff();
                        } else {
                            if (!notes_[event.data1].active()) {
                                active_[activeCounter_++] = event.data1;
                            }
                            notes_[event.data1].noteOn(event.data1, event.data2);
                        }
                    }
                    break;

                case MidiEventType::NoteOff:
                    if (event.data1 < notes_.size()) {
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
            activeCounter_ = 0;
        };

    private:
        std::array<Piano, 128> notes_{};
};

class VirtualGuitar : public Instrument {
    public:
        explicit VirtualGuitar(const double samplingRate = 44100) {
            setSamplingRate(samplingRate);
        };

        void setSamplingRate(const double &samplingRate) override {
            samplingRate_ = samplingRate;
            for (auto &note: notes_) {
                note.setSamplingRate(samplingRate_);
            }
        }

        void process(const MidiEvent &event) override {
            switch (event.type) {
                case MidiEventType::NoteOn:
                    if (event.data1 < notes_.size()) {
                        if (event.data2 == 0) {
                            notes_[event.data1].noteOff();
                        } else {
                            if (!notes_[event.data1].active()) {
                                active_[activeCounter_++] = event.data1;
                            }
                            notes_[event.data1].noteOn(event.data1, event.data2);
                        }
                    }
                    break;

                case MidiEventType::NoteOff:
                    if (event.data1 < notes_.size()) {
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
            for (auto &note: notes_) {
                note = Guitar(samplingRate_);
            }
        }

    private:
        std::array<Guitar, 128> notes_{};
};

#endif
