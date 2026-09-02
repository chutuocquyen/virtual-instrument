#ifndef _NOTE_SYNTHESIS_
#define _NOTE_SYNTHESIS_

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>

constexpr float pi = std::numbers::pi_v<float>;

constexpr float pickPosition = .12f;
constexpr float decayTime = 10.f;

inline float inharmonicityCoeff(uint8_t note) {
    const float m = (float) note;
    return std::exp(-10.2f - .014f * m) + std::exp(.1021f * m - 14.6221f);
}

class Piano {
    public:
        Piano() = default;
        Piano(const float &samplingRate) : samplingRate_(samplingRate), samplingDuration_(1.f / samplingRate) {}
        Piano(const float &samplingRate, const uint8_t &note) : samplingRate_(samplingRate), samplingDuration_(1.f / samplingRate), note_(note), frequency_(440.f * pow(2.f, (float) (note - 69) / 12)) {}


        void setSamplingRate(const float &samplingRate) {
            samplingRate_ = samplingRate;
            samplingDuration_ = 1.f / samplingRate;
        };

        void noteOn(const uint8_t &note, const uint8_t &velocity) {
			if (note != note_) return;

            lastTime_ = 0;
            releaseTime_ = 0;

            released_ = false;
            active_ = velocity > 0;
            velocityGain_ = std::pow((float) velocity / 127.f, 2.f);

            harmonics_.generate(note_);

            for (auto &harmonic: harmonics_.h) {
                harmonic.envelopeValue = 1.0;
                harmonic.envelopeDecay = std::exp(-samplingDuration_ / harmonic.decay);
                harmonic.phase = 0;
                harmonic.phaseShift = 2 * pi * frequency_ * harmonic.ratio * samplingDuration_;
            }
        };
        void noteOff() {
            if (!active_ || released_) return;
            released_ = true;
            releaseTime_ = 0;
        };

        void reset() {
            velocityGain_ = 0;
            lastTime_ = 0;
            releaseTime_ = 0;
            active_ = false;
            released_ = false;
        }

        bool active() const {
            return active_;
        };
        uint8_t note() const {
            return note_;
        };

        float render() {
            if (!active_) return 0.f;
            const float attack = 1.f - std::exp(-lastTime_ / .004f);
            const float release = released_ ? std::exp(-releaseTime_ / .32f) : 1.f;

            float sample = 0.f;

            for (auto &harmonic : harmonics_.h) {
                sample += harmonic.amplitude * harmonic.envelopeValue * std::sin(harmonic.phase);
                harmonic.envelopeValue *= harmonic.envelopeDecay;
                harmonic.phase += harmonic.phaseShift;
                if (harmonic.phase > 2.f * pi) harmonic.phase -= 2.f * pi;
            }

            const float hammer = .12f * std::exp(-lastTime_ / .018f) * (float) std::sin(2 * pi * frequency_ * 8.7 * lastTime_);

            sample = .34f * velocityGain_ * attack * release * (sample + hammer);
            
            lastTime_ += samplingDuration_;
            if (released_) {
                releaseTime_ += samplingDuration_;
                if (release < .01f) active_ = false;
            }

            return sample;
        };

    private:
        float samplingRate_ = 44100.f;
        float samplingDuration_ = 1.f / samplingRate_;

        uint8_t note_;
        float frequency_;

        float velocityGain_ = 0.f;
        float lastTime_ = 0.f;
        float releaseTime_ = 0.f;

        bool active_ = false;
        bool released_ = false;

        struct Harmonic {
            Harmonic() = default;
            Harmonic(const uint8_t &n, const float &a, const float &d) : order(n), amplitude(a), decay(d) {};
            uint8_t order;
            float amplitude;
            float decay;
            float ratio;

            float envelopeValue;
            float envelopeDecay;
            float phase;
            float phaseShift;
        };

        struct Harmonics {
            std::array<Harmonic, 7> h;

            void generate(uint8_t note) {
                const float B = inharmonicityCoeff(note);

                for (auto &harmonic: h) {
                    const uint8_t order = harmonic.order;
                    harmonic.ratio = order * sqrt((1 + B * order * order) / (1 + B));
                }
            };
        };

        static Harmonics generateHarmonics() {
            Harmonics tmp{};

            tmp.h[0] = Harmonic{1, 1.00f, 1.8f};
            tmp.h[1] = Harmonic{2, 0.55f, 1.5f};
            tmp.h[2] = Harmonic{3, 0.32f, 1.2f};
            tmp.h[3] = Harmonic{4, 0.20f, 1.0f};
            tmp.h[4] = Harmonic{5, 0.13f, 0.7f};
            tmp.h[5] = Harmonic{6, 0.08f, 0.5f};
            tmp.h[6] = Harmonic{7, 0.05f, 0.3f};

            return tmp;
        };

        Harmonics harmonics_ = generateHarmonics();
};

class Guitar {
    public:
        Guitar() = default;
        Guitar(const float &samplingRate) : samplingRate_(samplingRate) {
            samplingDuration_ = 1.f / samplingRate_;
        }
        Guitar(const float &samplingRate, const uint8_t &note) : samplingRate_(samplingRate), note_(note), frequency_(440.f * pow(2.f, (float) (note - 69) / 12)) {
            samplingDuration_ = 1.f / samplingRate_;
            // const float B = pow(pi, 3.f) * 2.0e11f * pow(0.014f * 0.0254f, 4.f) / (64.f * 0.648f * 0.648f * 80.f);
            const float B = pow(pi, 3.f) * 2.0e11f * pow(0.012f * 0.0254f, 4.f) / (64.f * 0.648f * 0.648f * 72.f);

            for (auto &a: Hc) a.coeffs(B, 4, note);
        }

        void setSamplingRate(const float &samplingRate) {
            samplingRate_ = samplingRate;
            samplingDuration_ = 1.f / samplingRate_;

            const float omega = 2.f * pi * frequency_ / samplingRate_;
            const float dispersionDelay = Hc[0].phaseDelay(omega) * 4.f;

            const float delay = samplingRate_ / frequency_ - .5f - dispersionDelay;
            toDelay_ = (size_t) std::floor(delay);

            float d = delay - (float) toDelay_;
            if (d < .2f) {
                --toDelay_;
                d += 1.f;
            }

            delayBuffer_.assign(toDelay_, 0.f);
            delayBufferIdx_ = 0;

            Hg.a = (1.f - d) / (1.f + d);
            Hg.reset();
            for (auto &a: Hc) a.reset();
            prevY_ = 0.f;

            float T60 = decayTime - .55f * std::log2(frequency_ / 82.406889f);  // Low E
            T60 = std::max(T60, .5f);
            feedbackGain_ = std::exp(std::log(.001f) / (T60 * frequency_));
            releaseMultiplier_ = std::exp(std::log(.001f) / decayTime / samplingRate_);
        }

        void noteOn(const uint8_t &note, const uint8_t &velocity) {
            if (!velocity || (note != note_)) return;

            active_ = true;
            released_ = false;

            velocityGain_ = (float) velocity / 127;
            releaseGain_ = 1.f;

            string(randomizer_);

            Hg.reset();
            for (auto &a: Hc) a.reset();
            prevY_ = delayBuffer_.empty() ? 0.f : delayBuffer_[(delayBufferIdx_ + delayBuffer_.size() - 1) % delayBuffer_.size()];
        }

        void noteOff() {
            if (released_) return;
            released_ = true;
        }

        void reset() {
            active_ = false;
            init_ = false;
            released_ = false;

            Hg.reset();
            for (auto &a: Hc) a.reset();
            prevY_ = 0.f;
        }

        bool active() {
            return active_;
        }

        float fromDelay() {
            if (delayBuffer_.empty()) return 0.f;

            const float Y = delayBuffer_[delayBufferIdx_];

            // Loss filter
            float output = (Y + prevY_) * .5f;
            prevY_ = Y;
            // Dispersion filter
            for (size_t i = 0; i < 4; ++i) {
                output = Hc[i].process(output);
            }
            // Tuning filter
            output = Hg.process(output);

            return output;
        }

        float render() {
            if (!active_) return 0.f;

            const float feedback = fromDelay();
            delayBuffer_[delayBufferIdx_] = feedbackGain_ * feedback;
            delayBufferIdx_ = (delayBufferIdx_ + 1) % delayBuffer_.size();

            if (released_) {
                releaseGain_ *= releaseMultiplier_;
                if (releaseGain_ < 0.01f) active_ = false;
            }

            return feedback * releaseGain_;
        }

    private:
        struct TuningAllpass {
            // H(z) = (a + z**(-1)) / (1 + a * z**(-1))
            // a = (1 - d) / (d + 1)
            // y[n] = a * x[n] + x[n - 1] - a * y[n - 1]
            float a;
            float prevX = 0, prevY = 0;

            float process(const float &X) {
                const float Y = a * X + prevX - a * prevY;
                prevX = X;
                prevY = Y;
                return Y;
            }

            void reset() {
                prevX = 0;
                prevY = 0;
            }
        };

        struct DispersionAllpass {
            DispersionAllpass() = default;

            static constexpr float k1 = -0.00179f;
            static constexpr float k2 = -0.0233f;
            static constexpr float k3 = -2.93f;
            static constexpr float m1 = 0.0126f;
            static constexpr float m2 = 0.0606f;
            static constexpr float m3 = -0.00825f;
            static constexpr float m4 = 1.97f;

            float a, d;
            float prevX = 0, prevY = 0;
            
            void coeffs(const float B, const uint8_t M, const uint8_t note) {
                const float lnB = log(B), lnM = log((float) M), I = (float) note - 20.f;
                const float Cd = exp((m1 * lnM + m2) * lnB + m3 * lnM + m4);
                const float kd = exp(k1 * lnB * lnB + k2 * lnB + k3);
                d = exp(Cd - I * kd);
                a = (1.f - d) / (d + 1.f);
            }

            float phaseDelay(const float omega) const {
                if (omega == 0.f) return d;
                
                return 2.f / omega * atan(d * tan(omega * .5f));
            }

            float process(const float &X) {
                const float Y = a * X + prevX - a * prevY;
                prevX = X;
                prevY = Y;
                return Y;
            }

            void reset() {
                prevX = 0;
                prevY = 0;
            }
        };

        void string(std::mt19937 &randomizer) {
            if (delayBuffer_.empty()) return;

            std::uniform_real_distribution<float> noise(-1, 1);
            // At least 2 fixed ends
            const size_t numStringSamples = std::max<size_t>(2, (size_t) std::floor(toDelay_));
            float mean = 0;

            for (size_t i = 0; i < delayBuffer_.size(); ++i) {
                // y(x, 0) = hx / pL
                const float x = (float) (i % numStringSamples) / ((float) numStringSamples - 1.f);
                const float h = x < pickPosition ? x / pickPosition : (1 - x) / (1 - pickPosition);

                const float tmp = .92f * h + .08f * noise(randomizer);
                delayBuffer_[i] = tmp;
                mean += tmp;
            }

            mean /= (float) delayBuffer_.size();
            for (float &sample: delayBuffer_) {
                sample = (sample - mean) * velocityGain_;
            }
        }

        bool active_ = false;
        bool init_ = false;

        uint8_t note_;
        float frequency_;

        float samplingRate_;
        float samplingDuration_;

		std::mt19937 randomizer_{};

        std::vector<float> delayBuffer_{};
        size_t delayBufferIdx_ = 0;
        size_t toDelay_;
        float prevY_ = 0;

        float velocityGain_;
        float feedbackGain_;    // Natural envelope decay

        bool released_;
        float releaseGain_;     // Release envelope decay
        float releaseMultiplier_;

        TuningAllpass Hg;
        std::array<DispersionAllpass, 4> Hc;
};

#endif
