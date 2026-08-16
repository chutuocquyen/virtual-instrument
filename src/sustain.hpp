#ifndef _SUSTAIN_PROCESSOR_
#define _SUSTAIN_PROCESSOR_

#include <vector>
#include "midi.hpp"
#include "processor.hpp"

struct sustainState : public State {
	bool pedalDown = false;
	std::array<bool, ANZAHL_NOTES> keysDown{false};
	std::array<bool, ANZAHL_NOTES> sustainedNotes{false};

	void reset() override {
		pedalDown = false;
		keysDown.fill(false);
		sustainedNotes.fill(false);
	};
};

class sustainProcessor : public Processor {
    public:
        std::vector<MidiEvent> process(const MidiEvent& event) override {
			switch (event.type) {
				case MidiEventType::NoteOn:
					return handleNoteOn(event);
				case MidiEventType::NoteOff:
					return handleNoteOff(event);
				case MidiEventType::ControlChange:
					return handleControlChange(event);
			}
		};

        void reset() override {
			state_.reset();
		};

        // const sustainState& state() const {
		// 	return state_;
		// }

    private:
        static constexpr uint8_t SUSTAIN_CONTROLLER = 64;
        static constexpr uint8_t SUSTAIN_THRESHOLD = 64;

        sustainState state_;

		std::vector<MidiEvent> handleNoteOn(const MidiEvent& event) {
			state_.keysDown[event.data1] = true;
			state_.sustainedNotes[event.data1] = false;
			return {event};
		};
		std::vector<MidiEvent> handleNoteOff(const MidiEvent& event) {
			state_.keysDown[event.data1] = false;
			if (state_.pedalDown) {
				state_.sustainedNotes[event.data1] = true;
				return {};
			}
			state_.sustainedNotes[event.data1] = false;
			return {event};
		};
		std::vector<MidiEvent> handleControlChange(const MidiEvent& event) {
			if (event.data1 != SUSTAIN_CONTROLLER) {
				return {event};
			}

			const bool a = state_.pedalDown;
			state_.pedalDown = event.data2 >= SUSTAIN_THRESHOLD;

			std::vector<MidiEvent> output{event};
			if (a && !state_.pedalDown) {
				for (size_t note = 0; note < state_.sustainedNotes.size(); note++) {
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
