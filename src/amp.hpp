#ifndef _AMP_PROCESSOR_
#define _AMP_PROCESSOR_

#include "processor.hpp"
#include "filters/butterworth.hpp"

class preampProcessor : public Processor {
    public:
        explicit preampProcessor(const float &samplingRate = 44100) : samplingRate_(samplingRate) {};

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

		void setPreGain(const float &a) {
			preGain_ = a;
		}
		void setPostGain(const float &a) {
			postGain_ = a;
		}
		void setBias(const float &a) {
			bias_ = a;
		}
		void setBlend(const float &a) {
			blend_ = a;
		}
		void setCutoff(const float &a) {
			lowpass_.setCutoff(a);
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

        float samplingRate_ = 44100;

        static constexpr size_t ANZAHL_COEFFS = 5;
        static constexpr std::array<float, ANZAHL_COEFFS + 1> polynomialCoeffs = {
            0, 1.0, 0.12, -0.25, -0.04, 0.16
        };

        float preGain_ = 2;
        float bias_ = 0.2;
        float mappingRange_ = 4;
        float blend_ = 0.85;
        float postGain_ = 0.9;

        Butterworth lowpass_{samplingRate_, 10.f};
};

class powerampProcessor : public Processor {
    public:
        explicit powerampProcessor(const float &samplingRate = 44100, const float &cutoff = 10) : samplingRate_(samplingRate), lowpass_(samplingRate, cutoff) {
        };

        float process(const float &sample) override {
            if (!enabled_) return sample;
            
            const float driven = sample * preGain_;
            const float biased = std::clamp(driven - lowpass_.process(std::abs(driven)) * bias_, -mappingRange_, mappingRange_);

            const float t = antiDerivative(biased);
            const float d = biased - previousBiased_;

            float nonlinear;
            if (abs(d) > 1e-5) nonlinear = (t - previousAntiDerivative_) / d;
            else nonlinear = applyMapping(biased);
            previousBiased_ = biased;
            previousAntiDerivative_ = t;

            return (nonlinear * blend_ + (1 - blend_) * driven) * postGain_;
        }

        void enable(const bool &a) override {
            enabled_ = a;
            if (!enabled_) reset();
        }

        void reset() override {
            lowpass_.reset();
            previousBiased_ = 0;
            previousAntiDerivative_ = 0;
        }

		void setPreGain(const float &a) {
			preGain_ = a;
		}
		void setPostGain(const float &a) {
			postGain_ = a;
		}
		void setBias(const float &a) {
			bias_ = a;
		}
		void setBlend(const float &a) {
			blend_ = a;
		}
		void setCutoff(const float &a) {
			lowpass_.setCutoff(a);
		}

    private:
        float applyMapping(const float &sample) {
            if (sample > kp_) {
                return ap_ * std::tanh(gp_ * (sample - kp_)) + bp_;
            }
            if (sample < -kn_) {
                return an_ * std::tanh(gn_ * (sample + kn_)) + bn_;
            }

            return std::tanh(sample);
        }

        float antiDerivative(const float &sample) {
            if (sample > kp_) {
                return bp_ * sample + ap_ / gp_ * std::log(std::cosh(gp_ * (sample - kp_))) + cp_;
            }
            if (sample < -kn_) {
                return bn_ * sample + an_ / gn_ * std::log(std::cosh(gn_ * (sample + kn_))) + cn_;
            }

            return std::log(std::cosh(sample));
            // const float t = abs(sample);
            // return t + std::log1p(std::exp(-2 * t)) - 0.6931471825f;
        }

        bool enabled_ = false;

        float samplingRate_ = 44100;

        float previousBiased_ = 0;
        float previousAntiDerivative_ = 0;

        float preGain_ = 2;
        float bias_ = 0.2;
        float mappingRange_ = 4;
        float blend_ = 0.85;
        float postGain_ = 0.9;

        float kp_ = 0.1, kn_ = 0.1;
        float Gp_ = 20, Gn_ = 10;
        float gp_ = std::pow(10.f, Gp_ / 20);
        float gn_ = std::pow(10.f, Gn_ / 20);

        const float ap_ = (1 - std::tanh(kp_) * std::tanh(kp_)) / gp_;
        const float bp_ = std::tanh(kp_);
        const float an_ = (1 - std::tanh(kn_) * std::tanh(kn_)) / gn_;
        const float bn_ = -std::tanh(kn_);

        const float cp_ = std::log(std::cosh(kp_)) - kp_ * bp_;
        const float cn_ = std::log(std::cosh(kn_)) + kn_ * bn_;

        Butterworth lowpass_;
};

#endif
