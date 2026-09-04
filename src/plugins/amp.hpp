#ifndef AMP_PROCESSOR
#define AMP_PROCESSOR

#include "processor.hpp"
#include "filters/biquad.hpp"
#include "filters/chebyshev.hpp"

class preampProcessor : public Processor {
    public:
        explicit preampProcessor(const float samplingRate = 44100.f) : samplingRate_(samplingRate) {}

        float process(const float sample) override {
            if (!enabled_) return sample;
            
            const float driven = sample * preGain_;
            const float biased = driven - lowpass_.process(std::abs(driven)) * bias_;

            const float nonlinear = applyMapping(std::clamp(biased, -mappingRange_, mappingRange_));

            return (nonlinear * blend_ + (1 - blend_) * driven) * postGain_;
        }

        void enable(const bool a) override {
            enabled_ = a;
            if (!enabled_) reset();
        }

        void reset() override {
            lowpass_.reset();
        }

		void setPreGain(const float a) {
			preGain_ = a;
		}
		void setPostGain(const float a) {
			postGain_ = a;
		}
		void setBias(const float a) {
			bias_ = a;
		}
		void setBlend(const float a) {
			blend_ = a;
		}
		void setCutoff(const float a) {
			lowpass_.setCutoff(a);
		}

    private:
        float applyMapping(const float sample) {
            float output = polynomialCoeffs[NUM_COEFFS];
            for (size_t i = NUM_COEFFS; i > 0; --i) {
                output = output * sample + polynomialCoeffs[i - 1];
            }
            return output;
        }

        bool enabled_ = false;

        float samplingRate_ = 44100;

        static constexpr size_t NUM_COEFFS = 5;
        static constexpr std::array<float, NUM_COEFFS + 1> polynomialCoeffs = {
            0.f, 1.f, .12f, -.25f, -.04f, .16f
        };

        float preGain_ = 2.f;
        float bias_ = .2f;
        float mappingRange_ = 4.f;
        float blend_ = .85f;
        float postGain_ = .9f;

        Biquad lowpass_{samplingRate_, 10.f};
};

class powerampProcessor : public Processor {
    public:
        explicit powerampProcessor(const float samplingRate = 44100.f, const float cutoff = 10.f) : samplingRate_(samplingRate), lowpass_(4, 2.f, cutoff * 2.f / samplingRate) {}

        float process(const float sample) override {
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

        void enable(const bool a) override {
            enabled_ = a;
            if (!enabled_) reset();
        }

        void reset() override {
            lowpass_.reset();
            previousBiased_ = 0;
            previousAntiDerivative_ = 0;
        }

		void setPreGain(const float a) {
			preGain_ = a;
		}
		void setPostGain(const float a) {
			postGain_ = a;
		}
		void setBias(const float a) {
			bias_ = a;
		}
		void setBlend(const float a) {
			blend_ = a;
		}
		void setCutoff(const float a) {
			lowpass_.setCutoff(a);
		}

    private:
        float applyMapping(const float sample) {
            if (sample > kp_) {
                return ap_ * std::tanh(gp_ * (sample - kp_)) + bp_;
            }
            if (sample < -kn_) {
                return an_ * std::tanh(gn_ * (sample + kn_)) + bn_;
            }

            return std::tanh(sample);
        }

        float antiDerivative(const float sample) {
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

        float samplingRate_ = 44100.f;

        float previousBiased_ = 0.f;
        float previousAntiDerivative_ = 0.f;

        float preGain_ = 2.f;
        float bias_ = .2f;
        float mappingRange_ = 4.f;
        float blend_ = .85f;
        float postGain_ = .9f;

        float kp_ = .1f, kn_ = .1f;
        float Gp_ = 20.f, Gn_ = 10.f;
        float gp_ = std::pow(10.f, Gp_ / 20.f);
        float gn_ = std::pow(10.f, Gn_ / 20.f);

        const float ap_ = (1 - std::tanh(kp_) * std::tanh(kp_)) / gp_;
        const float bp_ = std::tanh(kp_);
        const float an_ = (1 - std::tanh(kn_) * std::tanh(kn_)) / gn_;
        const float bn_ = -std::tanh(kn_);

        const float cp_ = std::log(std::cosh(kp_)) - kp_ * bp_;
        const float cn_ = std::log(std::cosh(kn_)) + kn_ * bn_;

        ChebyshevI lowpass_;
};

#endif
