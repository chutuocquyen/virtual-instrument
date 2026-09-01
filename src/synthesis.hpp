#ifndef _NOTE_SYNTHESIS_
#define _NOTE_SYNTHESIS_

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>

constexpr float pi = std::numbers::pi_v<float>;

constexpr float pickPosition = 0.12f;
constexpr float decayTime = 10.f;

float inharmonicityCoeff(uint8_t note) {
    const float m = (float) note;
    return std::exp(-10.2 - 0.014 * m) + std::exp(0.1021 * m - 14.6221);
}

class Piano {
    public:
        Piano() = default;
        Piano(const float &samplingRate) : samplingRate_(samplingRate), samplingDuration_(1.f / samplingRate) {}
        Piano(const float &samplingRate, const uint8_t &note) : samplingRate_(samplingRate), samplingDuration_(1.f / samplingRate), note_(note), frequency_(440.f * pow(2.f, (float) (note - 69) / 12)) {}


        void setSamplingRate(const float &samplingRate) {
            samplingRate_ = samplingRate;
            samplingDuration_ = 1.0 / samplingRate;
        };

        void noteOn(const uint8_t &note, const uint8_t &velocity) {
			if (note != note_) return;

            lastTime_ = 0;
            releaseTime_ = 0;

            released_ = false;
            active_ = velocity > 0;
            velocityGain_ = std::pow((float) velocity / 127, 2);

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
            if (!active_) return 0;
            const float attack = 1.0 - std::exp(-lastTime_ / 0.004);
            const float release = released_ ? std::exp(-releaseTime_ / 0.32) : 1;

            float sample = 0;

            for (auto &harmonic : harmonics_.h) {
                sample += harmonic.amplitude * harmonic.envelopeValue * std::sin(harmonic.phase);
                harmonic.envelopeValue *= harmonic.envelopeDecay;
                harmonic.phase += harmonic.phaseShift;
                if (harmonic.phase > 2 * pi) harmonic.phase -= 2 * pi;
            }

            const float hammer = 0.12 * std::exp(-lastTime_ / 0.018) * std::sin(2 * pi * frequency_ * 8.7 * lastTime_);

            sample = 0.34 * velocityGain_ * attack * release * (sample + hammer);
            
            lastTime_ += samplingDuration_;
            if (released_) {
                releaseTime_ += samplingDuration_;
                if (release < 0.01) active_ = false;
            }

            return sample;
        };

    private:
        float samplingRate_ = 44100;
        float samplingDuration_ = 1.0 / samplingRate_;

        uint8_t note_;
        float frequency_;

        float velocityGain_ = 0;
        float lastTime_ = 0;
        float releaseTime_ = 0;

        bool active_ = false;
        bool released_ = false;

        struct Harmonic {
            Harmonic() = default;
            Harmonic(const size_t &n, const float &a, const float &d) : order(n), amplitude(a), decay(d) {};
            size_t order;
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

            tmp.h[0] = Harmonic{1, 1.00, 1.8};
            tmp.h[1] = Harmonic{2, 0.55, 1.5};
            tmp.h[2] = Harmonic{3, 0.32, 1.2};
            tmp.h[3] = Harmonic{4, 0.20, 1.0};
            tmp.h[4] = Harmonic{5, 0.13, 0.7};
            tmp.h[5] = Harmonic{6, 0.08, 0.5};
            tmp.h[6] = Harmonic{7, 0.05, 0.3};

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
        }

        void setSamplingRate(const float &samplingRate) {
            samplingRate_ = samplingRate;
            samplingDuration_ = 1.f / samplingRate_;

            const float delay = samplingRate_ / frequency_ - .5f;
            toDelay_ = (size_t) (std::floor(delay));

            float d = delay - toDelay_;
            if (d < .2f) {
                --toDelay_;
                d += 1.f;
            }

            delayBuffer_.assign(toDelay_, 0.f);
            delayBufferIdx_ = 0;

            tuningAllpass.a = (1.f - d) / (1.f + d);
            tuningAllpass.reset();
            prevY_ = 0.f;

            float T60 = decayTime - 0.55 * std::log2(frequency_ / 82.406889);  // Low E
            T60 = std::max(T60, 0.5f);
            feedbackGain_ = std::exp(std::log(0.001) / (T60 * frequency_));
            releaseMultiplier_ = std::exp(std::log(0.001) / decayTime / samplingRate_);
        }

        void noteOn(const uint8_t &note, const uint8_t &velocity) {
            if (!velocity || (note != note_)) return;

            active_ = true;

            velocityGain_ = (float) velocity / 127;
            released_ = false;
            releaseGain_ = 1.f;

            string(randomizer);

            tuningAllpass.reset();
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

            tuningAllpass.reset();
            prevY_ = 0.f;
        }

        bool active() {
            return active_;
        }

        float fromDelay() {
            if (delayBuffer_.empty()) return 0.f;

            const float Y = delayBuffer_[delayBufferIdx_];
            const float Ha = (Y + prevY_) * .5f;
            prevY_ = Y;

            const float Hg = tuningAllpass.process(Ha);
            return Hg;
        }

        float render() {
            const float feedback = fromDelay();
            delayBuffer_[delayBufferIdx_] = feedbackGain_ * feedback;
            delayBufferIdx_ = (delayBufferIdx_ + 1) % delayBuffer_.size();

            if (released_) {
                releaseGain_ *= releaseMultiplier_;
                if (releaseGain_ < 0.01) active_ = false;
            }

            return feedback * releaseGain_;
        }

    private:
        struct Allpass {
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

        void string(std::mt19937 &randomizer) {
            if (delayBuffer_.empty()) return;

            std::uniform_real_distribution<float> noise(-1, 1);
            // At least 2 fixed ends
            const size_t numStringSamples = std::max<size_t>(2, std::floor(toDelay_));
            float mean = 0;

            for (size_t i = 0; i < delayBuffer_.size(); ++i) {
                // y(x, 0) = hx / pL
                const float x = (float) (i % numStringSamples) / (numStringSamples - 1);
                const float h = x < pickPosition ? x / pickPosition : (1 - x) / (1 - pickPosition);

                const float tmp = 0.92 * h + 0.08 * noise(randomizer);
                delayBuffer_[i] = tmp;
                mean += tmp;
            }

            mean /= delayBuffer_.size();
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

		std::mt19937 randomizer{};

        std::vector<float> delayBuffer_{};
        size_t delayBufferIdx_ = 0;
        size_t toDelay_;
        float prevY_ = 0;

        float velocityGain_;
        float feedbackGain_;    // Natural envelope decay

        bool released_;
        float releaseGain_;     // Release envelope decay
        float releaseMultiplier_;

        Allpass tuningAllpass;
};

#endif
