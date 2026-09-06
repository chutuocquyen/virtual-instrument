#ifndef NOTE_SYNTHESIS
#define NOTE_SYNTHESIS

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>
#include "filters/thiran.hpp"

constexpr float pi = std::numbers::pi_v<float>;

inline float inharmonicityCoeff(uint8_t note) {
    const float m = (float) note;
    return std::exp(-10.2f - .014f * m) + std::exp(.1021f * m - 14.6221f);
}

class Piano {
    public:
        Piano() = default;
        Piano(const float &samplingRate) : samplingRate_(samplingRate), samplingDuration_(1.f / samplingRate), nyquistRate_(samplingRate * .5f) {}
        Piano(const float &samplingRate, const uint8_t &note) : samplingRate_(samplingRate), samplingDuration_(1.f / samplingRate), nyquistRate_(samplingRate * .5f), note_(note), frequency_(440.f * pow(2.f, (float) (note - 69) / 12)) {}


        void setSamplingRate(const float &samplingRate) {
            samplingRate_ = samplingRate;
            samplingDuration_ = 1.f / samplingRate;
        }

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
        }

        void noteOff() {
            if (!active_ || released_) return;
            released_ = true;
            releaseTime_ = 0;
        }

        void reset() {
            velocityGain_ = 0;
            lastTime_ = 0;
            releaseTime_ = 0;
            active_ = false;
            released_ = false;
        }

        bool active() const {
            return active_;
        }

        uint8_t note() const {
            return note_;
        }

        float render() {
            if (!active_) return 0.f;
            const float attack = 1.f - std::exp(-lastTime_ / .004f);
            const float releaseDecay = releaseDecay_ * pow(440.f / frequency_, alpha_);
            const float release = released_ ? std::exp(-releaseTime_ / releaseDecay) : 1.f;

            float sample = 0.f;

            for (auto &harmonic : harmonics_.h) {
                if (frequency_ * harmonic.ratio < nyquistRate_) sample += harmonic.amplitude * harmonic.envelopeValue * std::sin(harmonic.phase);
                harmonic.envelopeValue *= harmonic.envelopeDecay;
                harmonic.phase += harmonic.phaseShift;
                while (harmonic.phase > 2.f * pi) harmonic.phase -= 2.f * pi;
            }

            const float hammer = (frequency_ * 8.7f) < nyquistRate_ ? .12f * std::exp(-lastTime_ / .018f) * (float) std::sin(2 * pi * frequency_ * 8.7f * lastTime_) : 0.f;

            sample = .34f * velocityGain_ * attack * release * (sample + hammer);
            
            lastTime_ += samplingDuration_;
            if (released_) {
                releaseTime_ += samplingDuration_;
                if (release < .01f) active_ = false;
            }

            return sample;
        };

        void setFrequency() {
            frequency_ = 440.f * pow(2.f, (float) (note_ + transpose_ - 69) / 12);
            for (auto &a: harmonics_.h) {
                a.phaseShift = 2.f * pi * frequency_ * a.ratio * samplingDuration_;
            }
        }

        void transpose(const int a) {
            transpose_ = a;
            setFrequency();
        }

    private:
        float samplingRate_ = 44100.f;
        float samplingDuration_ = 1.f / samplingRate_;
        float nyquistRate_ = samplingRate_ * .5f;

        uint8_t note_;
        float frequency_;
        int transpose_ = 0;

        float velocityGain_ = 0.f;
        float lastTime_ = 0.f;

        float releaseTime_ = 0.f;
        // decayTime(f) = releaseDecay_ * (440 / f)**alpha_
        const float releaseDecay_ = .5f;
        const float alpha_ = .42f;

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
        Guitar(const float samplingRate, const uint8_t note) : note_(note) {
            init(note);
            setFrequency();
            setSamplingRate(samplingRate);
        }

        void setSamplingRate(const float samplingRate) {
            samplingRate_ = samplingRate;
            samplingDuration_ = 1.f / samplingRate_;

            const size_t delaySize = (size_t) std::floor(samplingRate_ / FREQ_B1);

            delayBuffer_.assign(delaySize, 0.f);
            delayBufferIdx_ = 0;
            transposeBuffer_.assign(delaySize, 0.f);

            updateTuning();
            reset();
        }

        void noteOn(const uint8_t note, const uint8_t velocity) {
            if (!velocity || note != note_ || delayBuffer_.empty()) return;

            active_ = true;
            released_ = false;

            velocityGain_ = (float) velocity / 127.f;
            releaseTime_ = 0.f;
            releasePeak_ = 0.f;
            releaseCounter_ = toDelay_;

            delayBufferIdx_ = 0;
            string();

            Hg.reset();
            for (auto &a: Hc) a.reset();
            prevY_ = delayBuffer_[(delayBufferIdx_ + delayBuffer_.size() - toDelay_ - 1) % delayBuffer_.size()];
        }

        void noteOff() {
            if (!active_ || released_) return;

            released_ = true;
            releaseTime_ = 0.f;
            releasePeak_ = 0.f;
            releaseCounter_ = toDelay_;
        }

        void reset() {
            active_ = false;
            released_ = false;
            releaseTime_ = 0.f;
            releasePeak_ = 0.f;
            releaseCounter_ = 0;

            Hg.reset();
            for (auto &a: Hc) a.reset();
            prevY_ = 0.f;
        }

        bool active() const {
            return active_;
        }

        float render() {
            if (!active_) return 0.f;

            const float feedback = fromDelay();
            const float gain = released_ ? releaseGain_ : feedbackGain_;
            const float sample = gain * feedback;

            delayBuffer_[delayBufferIdx_] = sample;
            delayBufferIdx_ = (delayBufferIdx_ + 1) % delayBuffer_.size();

            if (released_) {
                releaseTime_ += samplingDuration_;
                releasePeak_ = std::max(releasePeak_, abs(sample));

                --releaseCounter_;
                if (releaseCounter_ == 0) {
                    if (releasePeak_ < .01f) active_ = false;
                    releasePeak_ = 0.f;
                    releaseCounter_ = toDelay_;
                }
                if (releaseTime_ > releaseDuration_) active_ = false;
            }

            return feedback * .67f;
        }

        void setFrequency() {
            frequency_ = 440.f * pow(2.f, (float) (note_ + transpose_ - 69) / 12);
        }

        void transpose(const int a) {
            if (a == transpose_) return;

            const size_t prevDelay = toDelay_;
            transpose_ = a;
            updateTuning();

            if (!active_) return;

            const size_t a0 = (delayBufferIdx_ + delayBuffer_.size() - prevDelay) % delayBuffer_.size();
            for (size_t i = 0; i < prevDelay; ++i) {
                transposeBuffer_[i] = delayBuffer_[(a0 + i) % delayBuffer_.size()];
            }

            std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0.f);
            delayBufferIdx_ = 0;

            const float step = (float) prevDelay / (float) toDelay_;
            const size_t a1 = delayBuffer_.size() - toDelay_;

            for (size_t i = 0; i < toDelay_; ++i) {
                const float p = (float) i * step;
                const size_t l = (size_t) p;
                const float tmp = p - (float) l;
                delayBuffer_[a1 + i] = transposeBuffer_[l] + tmp * (transposeBuffer_[(l + 1) % prevDelay] - transposeBuffer_[l]);
            }

            prevY_ = delayBuffer_[delayBuffer_.size() - 1];
            delayBuffer_[a1 - 1] = prevY_;

            Hg.reset();
            for (auto &a: Hc) a.reset();
        }

    private:
        struct String {
            uint8_t note;
            // B = pi**3 * Q * d**4 / 64 / l**2 / T
            float Q, d, l, T;
            float decayDuration;
            float releaseDuration;
        };

        inline static constexpr std::array<String, 7> strings_{{
            {35, 2.e11f, 0.022f * 0.0254f, 0.648f, 75.f, 12.f, 5.2f},   // B1
            {40, 2.e11f, 0.018f * 0.0254f, 0.648f, 75.f, 11.f, 4.7f},   // E2
            {45, 2.e11f, 0.016f * 0.0254f, 0.648f, 80.f, 10.f, 4.2f},   // A2
            {50, 2.e11f, 0.014f * 0.0254f, 0.648f, 80.f, 9.0f, 3.7f},   // D3
            {55, 2.e11f, 0.012f * 0.0254f, 0.648f, 75.f, 8.0f, 3.2f},   // G3
            {59, 2.e11f, 0.016f * 0.0254f, 0.648f, 70.f, 7.0f, 2.7f},   // B3
            {64, 2.e11f, 0.012f * 0.0254f, 0.648f, 72.f, 6.0f, 2.2f},   // E4
        }};

        void init(const uint8_t note) {
            const float b = note < 35 ? 35.f : note > 85 ? 85.f : (float) note;
            const String *string;

            if (b > 63) string = &strings_[6];
            else if (b > 58) string = &strings_[5];
            else if (b > 54) string = &strings_[4];
            else if (b > 49) string = &strings_[3];
            else if (b > 44) string = &strings_[2];
            else if (b > 39) string = &strings_[1];
            else string = &strings_[0];

            const float fret = b - (float) string->note;
            const float length = string->l / std::pow(2.f, fret / 12.f);

            // TODO: pre-calc
            const float B = pow(pi, 3.f) * string->Q * pow(string->d, 4.f) / 64.f / length / length / string->T;
            for (auto &a: Hc) a.coeffs(B, NUM_DISPERSION_FILTERS, note);

            decayDuration_ = string->decayDuration;
            releaseDuration_ = string->releaseDuration;
        }

        void updateTuning() {
            setFrequency();

            const float omega = 2.f * pi * frequency_ / samplingRate_;
            const float dispersionDelay = Hc[0].phaseDelay(omega) * (float) NUM_DISPERSION_FILTERS;

            const float delay = samplingRate_ / frequency_ - .5f - dispersionDelay;
            toDelay_ = (size_t) std::floor(delay);

            float d = delay - (float) toDelay_;
            if (d < .2f) {
                --toDelay_;
                d += 1.f;
            }

            Hg.coeffs(d);

			// E2
            float T60 = decayDuration_ - .55f * float(note_ + transpose_ - 40) / 12.f;
			T60 = std::max(T60, .5f);
            feedbackGain_ = std::exp(std::log(.001f) / T60 / frequency_);
            releaseGain_ = std::exp(std::log(.001f) / releaseDuration_ / frequency_);
        }

        void string() {
            if (delayBuffer_.empty()) return;

            std::uniform_real_distribution<float> noise(-1, 1);
            // At least 2 fixed ends
            const size_t numStringSamples = std::max<size_t>(2, toDelay_);
            float mean = 0.f;

            for (size_t i = 0; i < numStringSamples; ++i) {
                // y(x, 0) = hx / pL
                const float x = (float) i / ((float) numStringSamples - 1.f);
                const float h = x < pickPosition_ ? x / pickPosition_ : (1 - x) / (1 - pickPosition_);

                const float tmp = .92f * h + .08f * noise(randomizer_);
                // const float tmp = noise(randomizer_);
                delayBuffer_[(delayBufferIdx_ + delayBuffer_.size() - numStringSamples + i) % delayBuffer_.size()] = tmp;
                mean += tmp;
            }

            mean /= (float) numStringSamples;
            for (size_t i = 0; i < numStringSamples; ++i) {
                delayBuffer_[(delayBufferIdx_ + delayBuffer_.size() - numStringSamples + i) % delayBuffer_.size()] -= mean;
                delayBuffer_[(delayBufferIdx_ + delayBuffer_.size() - numStringSamples + i) % delayBuffer_.size()] *= velocityGain_;
            }
        }

        float fromDelay() {
            const float Y = delayBuffer_[(delayBufferIdx_ + delayBuffer_.size() - toDelay_) % delayBuffer_.size()];

            // Loss filter
            float output = (Y + prevY_) * .5f;
            prevY_ = Y;
            // Dispersion filter
            for (auto &a: Hc) {
                output = a.process(output);
            }
            // Tuning filter
            output = Hg.process(output);

            return output;
        }

        static constexpr size_t NUM_DISPERSION_FILTERS = 4;
        static constexpr float pickPosition_ = .12f;
        float decayDuration_ = 10.f;
        float releaseDuration_ = 4.2f;

		static constexpr float FREQ_B1 = 61.735413f;

        bool active_ = false;

        uint8_t note_;
        float frequency_;
        int transpose_ = 0;

        float samplingRate_;
        float samplingDuration_;

		std::mt19937 randomizer_{};

        std::vector<float> delayBuffer_{};
        std::vector<float> transposeBuffer_{};
        size_t delayBufferIdx_ = 0;
        size_t toDelay_;
        float prevY_ = 0;

        float velocityGain_;
        float feedbackGain_;    // Natural envelope decay

        bool released_;
        float releaseGain_;     // Release envelope decay
        float releaseTime_;
        float releasePeak_;

        Thiran Hg;
        std::array<Thiran, NUM_DISPERSION_FILTERS> Hc;

        size_t releaseCounter_ = 0;
};

#endif
