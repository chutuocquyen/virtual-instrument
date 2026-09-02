#ifndef EQUALIZER
#define EQUALIZER

#include "processor.hpp"
#include "filters/biquad.hpp"

class Equalizer : public Processor {
    public:
        explicit Equalizer(const float &samplingRate = 44100) : samplingRate_(samplingRate), lowEQ_(samplingRate_, lowCutoff_, FilterType::Lowshelf, lowGain_, lowQ_), midEQ_(samplingRate_, midCutoff_, FilterType::Bell, midGain_, midQ_), highEQ_(samplingRate_, highCutoff_, FilterType::Highshelf, highGain_, highQ_) {};

        float process(const float &sample) override {
			if (!enabled_) return sample;

			const float low = lowEQ_.process(sample);
			const float mid = midEQ_.process(low);
			const float high = highEQ_.process(mid);

			return high * level_;
        }

        void enable(const bool &a) override {
            enabled_ = a;
            if (!enabled_) reset();
        }

        void reset() override {
			lowEQ_.reset();
			midEQ_.reset();
			highEQ_.reset();
        }

    private:
        bool enabled_ = false;

        float samplingRate_ = 44100.f;

        float lowQ_ = .70710687f, midQ_ = 1.f, highQ_ = .70710687f;
        float lowGain_ = 10.f, midGain_ = 2.f, highGain_ = -3.f;
        float lowCutoff_ = 200.f, midCutoff_ = 500.f, highCutoff_ = 1200.f;

        float level_ = 1.2f;

        Biquad lowEQ_, midEQ_, highEQ_;
};

#endif
