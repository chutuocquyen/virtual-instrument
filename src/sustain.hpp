#ifndef _SUSTAIN_PROCESSOR_
#define _SUSTAIN_PROCESSOR_

#include "processor.hpp"

struct sustainState : public State {
	bool pedalDown = false;
	std::array<bool, ANZAHL_NOTES> keysDown{false};
	std::array<bool, ANZAHL_NOTES> sustainedNotes{false};

	void reset() override {
		pedalDown = false;
		keysDown.fill(false);
		sustainedNotes.fill(false);
	}
};

class sustainProcessor : public Processor {
    public:
        MidiEventBuffer process(const MidiEvent &event) override {
			if (!enabled_) return MidiEventBuffer(event);
			switch (event.type) {
				case MidiEventType::NoteOn:
					return handleNoteOn(event);
				case MidiEventType::NoteOff:
					return handleNoteOff(event);
				case MidiEventType::ControlChange:
					return handleControlChange(event);
			}

			return MidiEventBuffer(event);
		}

        void reset() override {
			state_.reset();
		}

		void enable(const bool &a) override {
			enabled_ = a;
			if (!enabled_) reset();
		}

		bool enabled() const {
			return enabled_;
		}

    private:
        static constexpr uint8_t SUSTAIN_CONTROLLER = 64;
        static constexpr uint8_t SUSTAIN_THRESHOLD = 64;

        bool enabled_ = false;
        sustainState state_;

		MidiEventBuffer handleNoteOn(const MidiEvent &event) {
			state_.keysDown[event.data1] = true;
			state_.sustainedNotes[event.data1] = false;
			return MidiEventBuffer(event);
		}
		MidiEventBuffer handleNoteOff(const MidiEvent &event) {
			state_.keysDown[event.data1] = false;
			if (state_.pedalDown) {
				state_.sustainedNotes[event.data1] = true;
				return MidiEventBuffer{};
			}
			state_.sustainedNotes[event.data1] = false;
			return MidiEventBuffer(event);
		}
		MidiEventBuffer handleControlChange(const MidiEvent &event) {
			if (event.data1 != SUSTAIN_CONTROLLER) return MidiEventBuffer(event);

			const bool a = state_.pedalDown;
			state_.pedalDown = !(event.data2 < SUSTAIN_THRESHOLD);

			MidiEventBuffer output{event};
			if (a && !state_.pedalDown) {
				for (size_t note = 0; note < state_.sustainedNotes.size(); ++note) {
					if (state_.sustainedNotes[note] && !state_.keysDown[note]) {
						output.push_back(MidiEvent::NoteOff((uint8_t) note, 0, event.channel));
						state_.sustainedNotes[note] = false;
					}
				}
			}
			
			return output;
		}
};

#endif
