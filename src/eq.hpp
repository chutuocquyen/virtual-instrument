#ifndef _EQUALIZER_
#define _EQUALIZER_

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

        float samplingRate_ = 44100;

        float lowQ_ = 0.70710687f, midQ_ = 1, highQ_ = 0.70710687f;
        float lowGain_ = 10, midGain_ = 2, highGain_ = -3;
        float lowCutoff_ = 200, midCutoff_ = 500, highCutoff_ = 1200;

        float level_ = 1.2;

        Biquad lowEQ_, midEQ_, highEQ_;
};

#endif
