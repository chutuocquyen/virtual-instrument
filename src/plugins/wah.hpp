#ifndef WAH_PROCESSOR
#define WAH_PROCESSOR

#include "processor.hpp"

class wahProcessor : public Processor {
    public:
        explicit wahProcessor(const float &samplingRate = 44100) : samplingRate_(samplingRate) {
            reset();
            update();
        }

        float process(const float &sample) override {
            if (!enabled_) return sample;

            const float i3 = sample + i0 - 2 * i2;
            const float v1 = i1;

            i1 += g1 * i3 - g2 * v1;     // band
            i2 += g3 * i3 + g4 * v1;     // low
            i0 = sample;

            return (1 - mix_) * sample + mix_ * std::tanh(i1 * outputGain_);
        }

        void enable(const bool &a) override {
            enabled_ = a;
            if (!enabled_) reset();
        }

        void reset() override {
            i0 = i1 = i2 = 0;
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

        float samplingRate_ = 44100;
        
        float i0, i1, i2;

        float cutoff = 1000;
        float bandwidth = 550;

        // float pedal_;
        float mix_ = 1;
        float outputGain_ = 2;

        float g1, g2, g3, g4;

};

#endif