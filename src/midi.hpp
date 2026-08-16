#ifndef _MIDI_
#define _MIDI_

#include <cstdint>

enum class MidiEventType : uint8_t {
    NoteOn,
    NoteOff,
    ControlChange,
};

struct MidiEvent {
    MidiEventType type;
    uint8_t data1;
    uint8_t data2;
    uint8_t channel;

    static MidiEvent NoteOn(uint8_t note,
                            uint8_t velocity,
                            uint8_t channel = 0) {
		return {MidiEventType::NoteOn, note, velocity, channel};
	};
	static MidiEvent NoteOff(uint8_t note,
							 uint8_t velocity = 0,
							 uint8_t channel = 0) {
		return {MidiEventType::NoteOff, note, velocity, channel};
	};
	static MidiEvent ControlChange(uint8_t controller,
							 	   uint8_t value,
							 	   uint8_t channel = 0) {
		return {MidiEventType::ControlChange, controller, value, channel};
	}

    bool operator==(const MidiEvent& event) const {
        return type == event.type &&
               data1 == event.data1 &&
               data2 == event.data2 &&
               channel == event.channel;
    };
};

#endif
