#ifndef _PREAMP_PROCESSOR_
#define _PREAMP_PROCESSOR_

#include "processor.hpp"
#include "filters/butterworth.hpp"

class preampProcessor : public Processor {
    public:
        explicit preampProcessor(const double &samplingRate = 44100) : samplingRate_(samplingRate) {};

        float process(const float &sample) override {
            if (!enabled_) return sample;
            
            const float driven = sample * preGain_;
            const float biased = driven - lowpass_.process(std::abs(driven)) * bias_;

            const float nonlinear = applyMapping(std::clamp(biased, -mappingRange_, mappingRange_));

            return (nonlinear * blend_ + (1 - blend_) * driven) * postGain_;
        }

        void enable(const bool &a) override {
            enabled_ = a;
            if (!enabled_) reset();
        }

        void reset() override {
            lowpass_.reset();
        }

    private:
        float applyMapping(const float &sample) {
            float output = polynomialCoeffs[ANZAHL_COEFFS];
            for (size_t i = ANZAHL_COEFFS; i > 0; --i) {
                output = output * sample + polynomialCoeffs[i - 1];
            }
            return output;
        }

        bool enabled_ = false;

        double samplingRate_ = 44100;

        static constexpr size_t ANZAHL_COEFFS = 5;
        static constexpr std::array<float, ANZAHL_COEFFS + 1> polynomialCoeffs = {
            0, 1.0, 0.12, -0.25, -0.04, 0.16
        };

        float preGain_ = 2;
        float bias_ = 0.2;
        float mappingRange_ = 1.5;
        float blend_ = 0.85;
        float postGain_ = 0.9;

        Butterworth lowpass_{samplingRate_, 10.f};
};

#endif
