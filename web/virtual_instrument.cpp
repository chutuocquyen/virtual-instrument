#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/webaudio.h>

#include <span>

#include "midi.hpp"
#include "instrument.hpp"
#include "plugins/sustain.hpp"
#include "plugins/wah.hpp"
#include "plugins/amp.hpp"
#include "plugins/reverb.hpp"
#include "plugins/eq.hpp"

enum class InstrumentType { Piano = 1, Guitar = 2 };

enum class EffectType {
    Sustain     = 0,
    Wah         = 1,
    Preamp      = 2,
    EQ          = 3,
    Poweramp    = 4,
    Reverb      = 5,
};

// Audio context
EMSCRIPTEN_WEBAUDIO_T audioContext = 0;
EMSCRIPTEN_WEBAUDIO_T audioNode = 0;
alignas(16) uint8_t audioWorklet[64 * 1024];

// Instruments
VirtualPiano piano;
VirtualGuitar guitar;
InstrumentType active;

constexpr int MinNote = 35;         // B1
constexpr int BaseNote = 48;        // C3
constexpr int MaxNote = 60;         // C4
int transpose = 0;

std::array<int, 128> notes{};
std::array<bool, 6> effects{};

// Effects
Sustain sustain;
Wah wah;
Preamp preamp;
ToneStacks<VoxAC30> eq;
Reverb reverb;
Poweramp poweramp;

// DOM
EM_JS(void, statusInstrument, (int active), {
    const instrument = document.getElementById("instrument");
    instrument.textContent = active === 1 ? "Piano" : "Guitar";
});

EM_JS(void, statusKey, (int a), {
    const key = document.getElementById("key");
    const octave = document.getElementById("octave");
    const notes = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
    key.textContent = notes[a % 12];
    octave.textContent = Math.floor(a / 12) - 1;
});

EM_JS(bool, isChecked, (int effect), {
    const ids = ["sustain", "wah", "preamp", "eq", "poweramp", "reverb"];
    return document.getElementById(ids[effect]).checked ? 1 : 0;
});

EM_JS(void, checkEffect, (int effect, int a), {
    const ids = ["sustain", "wah", "preamp", "eq", "poweramp", "reverb"];
    const b = document.getElementById(ids[effect]);
    if (b) b.checked = Boolean(a);
});

EM_JS(void, effectControls, (), {
    document.querySelectorAll(".effect-controls").forEach(box => {
        const effect = Number(box.dataset.effect);

        box.querySelectorAll('input[type="range"]').forEach(control => {
            const output = document.getElementById(control.id + "-value");
            if (!output) return;

            const parameter = Number(control.dataset.parameter);
            const digits = Number(control.dataset.digits);
            const suffix = control.dataset.suffix || "";

            const updateDisplay = () => {
                output.value = Number(control.value).toFixed(digits) + suffix;
            };

            control.addEventListener("input", () => {
                updateDisplay();
                Module["_updateEffectParameter"](
                    effect, parameter, Number(control.value));
            });
            updateDisplay();
        });
    });
});

// AUDIO QUEUE
void resume() {
    if (audioContext != 0 && emscripten_audio_context_state(audioContext) != AUDIO_CONTEXT_STATE_RUNNING) emscripten_resume_audio_context_sync(audioContext);
}

template<typename Q>
void toWorklet(Q &&fn) {
    resume();
    if (audioNode) fn();
}

// INSTRUMENT
void setInstrument(int instrument) {
    const InstrumentType a = instrument == 1 ? InstrumentType::Piano : InstrumentType::Guitar;
    if (a == active) return;

    switch (active) {
        case InstrumentType::Piano: piano.reset(); break;
        case InstrumentType::Guitar: guitar.reset(); break;
    }
    active = a;
}

float render() {
    switch (active) {
        case InstrumentType::Piano: return piano.render();
        case InstrumentType::Guitar: return guitar.render();
    }
}

// SUSTAIN & NOTE
void throughSustain(const MidiEvent &event) {
    for (const auto &a: sustain.process(event))
        switch (active) {
            case InstrumentType::Piano: piano.process(a); break;
            case InstrumentType::Guitar: guitar.process(a); break;
        }
}

void noteOn(const int note, const int velocity) {
    throughSustain(MidiEvent::NoteOn((uint8_t) note, (uint8_t) velocity));
}

void noteOff(const int note) {
    throughSustain(MidiEvent::NoteOff((uint8_t) note));
}

// TRANSPOSE
void onTranspose(const int a) {
	piano.transpose(a);
	guitar.transpose(a);
}

void Transpose(const int a) {
    const int prev = transpose;
    transpose += a;
    if (transpose < MinNote - BaseNote) transpose = MinNote - BaseNote;
    if (transpose > MaxNote - BaseNote) transpose = MaxNote - BaseNote;

    if (transpose == prev) return;

    if (audioNode) {
        emscripten_audio_worklet_post_function_vi(audioContext, onTranspose, transpose);
    }

    statusKey(BaseNote + transpose);
}

// EFFECT
void enable(const int effect, const int a) {
    const bool b = a != 0;
    switch ((EffectType) effect) {
        case EffectType::Sustain:
            sustain.enable(b);
            throughSustain(MidiEvent::ControlChange(64, b ? 127 : 0));
            break;
        case EffectType::Wah: wah.enable(b); break;
        case EffectType::Preamp: preamp.enable(b); break;
        case EffectType::EQ: eq.enable(b); break;
        case EffectType::Poweramp: poweramp.enable(b); break;
        case EffectType::Reverb: reverb.enable(b); break;
    }
}

void trigger(const EffectType &effect, const bool a) {
    toWorklet([&] {
        emscripten_audio_worklet_post_function_vii(audioContext, enable, (int) effect, a ? 1 : 0);
    });
}

EM_BOOL onClick(int, const EmscriptenMouseEvent*, void *p) {
    const auto effect = (EffectType) ((intptr_t) p);
    const bool a = isChecked((int) effect) != 0;

    effects[(size_t) effect] = a;
    trigger(effect, a);

    return EM_FALSE;
}

// EFFECT PARAMETERS
void setWahCutoff(double a) { wah.setCutoff((float) a); }
void setWahBandwidth(double a) { wah.setBandwidth((float) a); }
void setWahMix(double a) { wah.setMix((float) a); }
void setWahGain(double a) { wah.setGain((float) a); }

void setPreampPreGain(double a) { preamp.setPreGain((float) a); }
void setPreampPostGain(double a) { preamp.setPostGain((float) a); }
void setPreampBias(double a) { preamp.setBias((float) a); }
void setPreampBlend(double a) { preamp.setBlend((float) a); }
void setPreampCutoff(double a) { preamp.setCutoff((float) a); }

void setEQBass(double a) { eq.setBass((float) a); }
void setEQMiddle(double a) { eq.setMiddle((float) a); }
void setEQTreble(double a) { eq.setTreble((float) a); }
void setEQLevel(double a) { eq.setLevel((float) a); }

void setPowerampPreGain(double a) { poweramp.setPreGain((float) a); }
void setPowerampPostGain(double a) { poweramp.setPostGain((float) a); }
void setPowerampBias(double a) { poweramp.setBias((float) a); }
void setPowerampBlend(double a) { poweramp.setBlend((float) a); }
void setPowerampCutoff(double a) { poweramp.setCutoff((float) a); }

void setReverbWet(double a) { reverb.setWet((float) a); }
void setReverbDecay(double a) { reverb.setDecay((float) a); }
void setReverbDamping(double a) { reverb.setDamping((float) a); }
void setReverbTone(double a) { reverb.setTone((float) a); }

struct EffectParameter {
    void (*apply) (double);
    float a;
    float min;
    float max;
};

std::array<EffectParameter, 4> wahParameters{{
    {setWahCutoff, 1000.f, 200.f, 3000.f},
    {setWahBandwidth, 550.f, 100.f, 2000.f},
    {setWahMix, 1.f, 0.f, 1.f},
    {setWahGain, 2.f, 0.f, 5.f},
}};

std::array<EffectParameter, 5> preampParameters{{
    {setPreampPreGain, 2.f, 0.f, 5.f},
    {setPreampPostGain, 0.9f, 0.f, 5.f},
    {setPreampBias, 0.2f, 0.f, 2.f},
    {setPreampBlend, 0.85f, 0.f, 1.f},
    {setPreampCutoff, 10.f, 0.1f, 100.f},
}};

std::array<EffectParameter, 4> eqParameters{{
    {setEQBass, 0.5f, 0.f, 1.f},
    {setEQMiddle, 1.f, 0.f, 1.f},
    {setEQTreble, 1.f, 0.f, 1.f},
    {setEQLevel, 2.f, 0.f, 5.f},
}};

std::array<EffectParameter, 5> powerampParameters{{
    {setPowerampPreGain, 2.f, 0.f, 5.f},
    {setPowerampPostGain, 0.9f, 0.f, 5.f},
    {setPowerampBias, 0.2f, 0.f, 2.f},
    {setPowerampBlend, 0.85f, 0.f, 1.f},
    {setPowerampCutoff, 10.f, 1.f, 100.f},
}};

std::array<EffectParameter, 4> reverbParameters{{
    {setReverbWet, 0.15f, 0.f, 1.f},
    {setReverbDecay, 1.7f, 0.1f, 10.f},
    {setReverbDamping, 0.35f, 0.f, 0.95f},
    {setReverbTone, 10.3f, -12.f, 12.f},
}};

std::span<EffectParameter> effectParameters(const EffectType &a) {
    switch (a) {
        case EffectType::Wah: return wahParameters;
        case EffectType::Preamp: return preampParameters;
        case EffectType::EQ: return eqParameters;
        case EffectType::Poweramp: return powerampParameters;
        case EffectType::Reverb: return reverbParameters;
        default: return {};
    }
}

void update(const EffectType &a, const size_t b) {
    auto parameters = effectParameters(a);

    const auto apply = parameters[b].apply;
    const double value = parameters[b].a;
    toWorklet([apply, value] {
        emscripten_audio_worklet_post_function_vd(audioContext, apply, value);
    });
}

extern "C" EMSCRIPTEN_KEEPALIVE void updateEffectParameter(int a, int b, float value) {
    const auto type = (EffectType) a;
    auto parameters = effectParameters(type);

    auto &p = parameters[(size_t) b];
    p.a = std::clamp(value, p.min, p.max);
    update(type, (size_t) b);
}

// MIDI
int asMidi(const char *a) {
    static constexpr struct { const char *a; int note; } mapping[] = {
        {"KeyA", 48}, {"KeyW", 49}, {"KeyS", 50}, {"KeyE", 51},
        {"KeyD", 52}, {"KeyF", 53}, {"KeyT", 54}, {"KeyG", 55},
        {"KeyY", 56}, {"KeyH", 57}, {"KeyU", 58}, {"KeyJ", 59},
        {"KeyK", 60}, {"KeyO", 61}, {"KeyL", 62}, {"KeyP", 63},
        {"Semicolon", 64}, {"Quote", 65}, {"BracketLeft", 66},
        {"Backslash", 67}, {"BracketRight", 68},
    };

    for (const auto &word: mapping) {
        if (strcmp(a, word.a) == 0) return word.note;
    }

    return -1;
}

EM_BOOL onKeyDown(int, const EmscriptenKeyboardEvent *event, void*) {
    if (strcmp(event->code, "Digit1") == 0 || strcmp(event->code, "Digit2") == 0) {
        if (!event->repeat && audioNode) {
            const int a = (strcmp(event->code, "Digit1") == 0) ? 1 : 2;
            emscripten_audio_worklet_post_function_vi(audioContext, setInstrument, a);
            statusInstrument(a);

            notes.fill(0);
            // for (size_t i = 0; i < effects.size(); ++i) {
            //     effects[i] = false;
            //     checkEffect((int) i, 0);
            //     trigger((EffectType) i, false);
            // }
        }
        return EM_TRUE;
    }

    if (strcmp(event->code, "Tab") == 0) {
        if (!event->repeat && audioNode) {
            resume();
            const bool a = !effects[(size_t) EffectType::Sustain];
            effects[(size_t) EffectType::Sustain] = a;
            checkEffect((int) EffectType::Sustain, a ? 1 : 0);
            trigger(EffectType::Sustain, a);
        }
        return EM_TRUE;
    }

    if (strcmp(event->code, "ArrowLeft") == 0) {
        if (!event->repeat) {
            Transpose(-1);
        }
        return EM_TRUE;
    } else if (strcmp(event->code, "ArrowRight") == 0) {
        if (!event->repeat) {
            Transpose(1);
        }
        return EM_TRUE;
    }

    if (strcmp(event->code, "ArrowUp") == 0) {
        if (!event->repeat) {
            Transpose(12);
        }
        return EM_TRUE;
    } else if (strcmp(event->code, "ArrowDown") == 0) {
        if (!event->repeat) {
            Transpose(-12);
        }
        return EM_TRUE;
    }

    const int note = asMidi(event->code);
    if (note < 0) return EM_FALSE;
    if (!event->repeat && audioNode && !notes[(size_t) note]) {
        resume();
        notes[(size_t) note] = note;
        emscripten_audio_worklet_post_function_vii(audioContext, noteOn, note, 100);
    }
    return EM_TRUE;
}

EM_BOOL onKeyUp(int, const EmscriptenKeyboardEvent *event, void*) {
    static const char *a[] = {
        "Tab", "Digit1", "Digit2", "ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown",
    };
    for (const char *i: a) {
        if (strcmp(event->code, i) == 0) return EM_TRUE;
    }

    const int note = asMidi(event->code);
    if (note < 0) return EM_FALSE;
    if (audioNode && notes[(size_t) note]) {
        notes[(size_t) note] = 0;
        emscripten_audio_worklet_post_function_vi(audioContext, noteOff, note);
    }

    return EM_TRUE;
}

// AUDIO WORKLET
bool process(int, const AudioSampleFrame*, int numOutputs, AudioSampleFrame *outputs, int, const AudioParamFrame*, void*) {
    for (int i = 0; i < numOutputs; ++i) {
        const int numChannels = outputs[i].numberOfChannels;
        const int numSamples = outputs[i].samplesPerChannel;

        for (int j = 0; j < numSamples; ++j) {
            float sample = render();
            sample = wah.process(sample);
            sample = preamp.process(sample);
            sample = eq.process(sample);
            sample = poweramp.process(sample);
            sample = reverb.process(sample);

            for (int k = 0; k < numChannels; ++k) {
                outputs[i].data[k * numSamples + j] = sample;
            }
        }
    }
    return true;
}

void onProcessor(EMSCRIPTEN_WEBAUDIO_T context, bool a, void*) {
    if (!a) return;

    int numOutputChannels[1] = {2};

    EmscriptenAudioWorkletNodeCreateOptions opts{};
    opts.numberOfInputs         = 0;
    opts.numberOfOutputs        = 1;
    opts.outputChannelCounts    = numOutputChannels;
    opts.channelCount           = 2;
    opts.channelCountMode       = WEBAUDIO_CHANNEL_COUNT_MODE_EXPLICIT;
    opts.channelInterpretation  = WEBAUDIO_CHANNEL_INTERPRETATION_SPEAKERS;

    audioNode = emscripten_create_wasm_audio_worklet_node(context, "virtual-instrument", &opts, process, nullptr);

	if (!audioNode) return;
	emscripten_audio_worklet_post_function_vi(context, onTranspose, transpose);
    emscripten_audio_node_connect(audioNode, context, 0, 0);

    for (size_t i = 0; i < effects.size(); ++i) {
        if (effects[i]) trigger((EffectType) i, true);
    }
    for (size_t i = 1; i < effects.size(); ++i) {
        const auto effect = (EffectType) i;
        const auto parameters = effectParameters(effect);
        for (size_t parameter = 0; parameter < parameters.size(); ++parameter) {
            update(effect, parameter);
        }
    }
}

void onThread(EMSCRIPTEN_WEBAUDIO_T context, bool a, void*) {
    if (!a) return;

    WebAudioWorkletProcessorCreateOptions opts{};
    opts.name = "virtual-instrument";

    emscripten_create_wasm_audio_worklet_processor_async(context, &opts, onProcessor, nullptr);
}

// thread - processor - node
int main() {
    EmscriptenWebAudioCreateAttributes attr{};
    attr.latencyHint = "interactive";
    attr.sampleRate = 0;  // 44100
    attr.renderSizeHint = AUDIO_CONTEXT_RENDER_SIZE_DEFAULT;

    audioContext = emscripten_create_audio_context(&attr);
    const float samplingRate = (float) emscripten_audio_context_sample_rate(audioContext);
    piano.setSamplingRate(samplingRate);
    guitar.setSamplingRate(samplingRate);
    active = InstrumentType::Piano;

    sustain.enable(false);
    wah.enable(false);
    preamp.enable(false);
    eq.enable(false);
    poweramp.enable(false);
    reverb.enable(false);

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onKeyDown);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE, onKeyUp);

    auto bindEffect = [](const char *id, EffectType a) {
        emscripten_set_click_callback(id, (void*)((intptr_t) a), EM_FALSE, onClick);
    };
    bindEffect("#sustain", EffectType::Sustain);
    bindEffect("#wah", EffectType::Wah);
    bindEffect("#preamp", EffectType::Preamp);
    bindEffect("#eq", EffectType::EQ);
    bindEffect("#poweramp", EffectType::Poweramp);
    bindEffect("#reverb", EffectType::Reverb);

    effectControls();

    emscripten_start_wasm_audio_worklet_thread_async(audioContext, audioWorklet, sizeof(audioWorklet), onThread, nullptr);
    statusInstrument(1);
    statusKey(BaseNote);

    return 0;
}
