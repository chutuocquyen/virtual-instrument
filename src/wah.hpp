#ifndef _WAH_PROCESSOR_
#define _WAH_PROCESSOR_

#include <numbers>
#include "processor.hpp"

struct wahState : public State {
    float i0 = 0;
    float i1 = 0;
    float i2 = 0;

    void reset() override {
        i0 = 0;
        i1 = 0;
        i2 = 0;
    }
};

class wahProcessor : public Processor {
    public:
        explicit wahProcessor(const double &samplingRate = 44100) : samplingRate_(samplingRate) {
            update();
        }

        float process(const float &sample) override {
            if (!enabled_) return sample;

            const float i3 = sample + state_.i0 - 2 * state_.i2;
            const float v1 = state_.i1;

            state_.i1 += g1 * i3 - g2 * v1;     // band
            state_.i2 += g3 * i3 + g4 * v1;     // low
            state_.i0 = sample;

            return (1 - mix_) * sample + mix_ * std::tanh(state_.i1 * outputGain_);
        }

        void enable(const bool &a) override {
            enabled_ = a;
            if (!enabled_) reset();
        }

        void reset() override {
            state_.reset();
        }

        // void setPedal(const float &position) {
        //     pedal_ = std::clamp(position, 0, 1);
        //     update();
        // }

    private:
        void update() {
            const float g = std::tan(pi * cutoff / samplingRate_);
            const float k = bandwidth / cutoff;
            
            const float ginv = g / (1 + g * (g + k));
            g1 = ginv;
            g2 = 2 * (g + k) * ginv;
            g3 = g * ginv;
            g4 = 2 * ginv;
        }

        bool enabled_ = false;
        wahState state_;

        double samplingRate_ = 44100;
        // float nyquistRate_ = (float) samplingRate_ * 0.45;
        // float minFreq_ = 350;
        // float maxFreq_ = 2200;

        float cutoff = 1000;
        float bandwidth = 550;

        // float pedal_;
        float mix_ = 1;
        float outputGain_ = 2;

        float g1, g2, g3, g4;

};

#endif