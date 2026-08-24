#ifndef _MIDI_
#define _MIDI_

#include <array>
#include <cstddef>
#include <cstdint>

constexpr size_t ANZAHL_NOTES = 128;
constexpr size_t MAX_EVENTS = ANZAHL_NOTES + 1;

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

    static MidiEvent NoteOn(const uint8_t &note, const uint8_t &velocity, const uint8_t &channel = 0) {
		return {MidiEventType::NoteOn, note, velocity, channel};
	};
	static MidiEvent NoteOff(const uint8_t &note, const uint8_t &velocity = 0, const uint8_t &channel = 0) {
		return {MidiEventType::NoteOff, note, velocity, channel};
	};
	static MidiEvent ControlChange(const uint8_t &controller, const uint8_t &value, const uint8_t &channel = 0) {
		return {MidiEventType::ControlChange, controller, value, channel};
	}
};

class MidiEventBuffer {
    public:
        MidiEventBuffer() = default;

        explicit MidiEventBuffer(const MidiEvent &event) {
            push_back(event);
        }

        bool push_back(const MidiEvent &event) {
            if (size_ < events_.size()) {
                events_[size_++] = event;
                return true;
            }
            return false;
        }

        const MidiEvent* begin() const {
            return events_.data();
        }
        const MidiEvent* end() const {
            return events_.data() + size_;
        }

    private:
        std::array<MidiEvent, MAX_EVENTS> events_{};
        size_t size_ = 0;
};

#endif
