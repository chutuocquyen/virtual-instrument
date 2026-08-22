#ifndef _NOTE_SYNTHESIS_
#define _NOTE_SYNTHESIS_

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>

constexpr double pi = std::numbers::pi;

constexpr double pickPosition = 0.12;
constexpr double decayTime = 4.2;

double inharmonicityCoeff(uint8_t note) {
    const double m = (double) note;
    return std::exp(-10.2 - 0.014 * m) + std::exp(0.1021 * m - 14.6221);
}

class Piano {
    public:
        Piano() = default;
        Piano(double samplingRate) : samplingRate_(samplingRate) {};

        void setSamplingRate(double samplingRate) {
            samplingRate_ = samplingRate;
            samplingTime_ = 1.0 / samplingRate;
        };

        void noteOn(uint8_t note, uint8_t velocity) {
            note_ = note;
            frequency_ = 440.0 * std::pow(2.0, ((double) note - 69) / 12);

            lastTime_ = 0;
            releaseTime_ = 0;

            released_ = false;
            active_ = velocity > 0;
            velocityGain_ = std::pow((double) velocity / 127, 2);

            harmonics_.generate(note_);

            for (auto &harmonic: harmonics_.h) {
                harmonic.envelopeValue = 1.0;
                harmonic.envelopeDecay = std::exp(-samplingTime_ / harmonic.decay);
                harmonic.phase = 0;
                harmonic.phaseShift = 2 * pi * frequency_ * harmonic.ratio * samplingTime_;
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
            const double attack = 1.0 - std::exp(-lastTime_ / 0.004);
            const double release = released_ ? std::exp(-releaseTime_ / 0.32) : 1;

            double sample = 0;

            for (auto &harmonic : harmonics_.h) {
                sample += harmonic.amplitude * harmonic.envelopeValue * std::sin(harmonic.phase);
                harmonic.envelopeValue *= harmonic.envelopeDecay;
                harmonic.phase += harmonic.phaseShift;
                if (harmonic.phase > 2 * pi) harmonic.phase -= 2 * pi;
            }

            const double hammer = 0.12 * std::exp(-lastTime_ / 0.018) * std::sin(2 * pi * frequency_ * 8.7 * lastTime_);

            sample = 0.34 * velocityGain_ * attack * release * (sample + hammer);
            
            lastTime_ += samplingTime_;
            if (released_) {
                releaseTime_ += samplingTime_;
                if (release < 0.001) active_ = false;
            }

            return (float) sample;
        };

    private:
        double samplingRate_ = 44100;
        double samplingTime_ = 1.0 / samplingRate_;

        uint8_t note_ = 60;    // C4
        double frequency_ = 440.0 * std::pow(2.0, -9.0 / 12);

        double velocityGain_ = 0;
        double lastTime_ = 0;
        double releaseTime_ = 0;

        bool active_ = false;
        bool released_ = false;

        struct Harmonic {
            Harmonic() = default;
            Harmonic(const size_t n, const double a, const double d) : order(n), amplitude(a), decay(d) {};
            size_t order;
            double amplitude;
            double decay;
            double ratio;

            double envelopeValue;
            double envelopeDecay;
            double phase;
            double phaseShift;
        };

        struct Harmonics {
            std::array<Harmonic, 7> h;

            void generate(uint8_t note) {
                const double B = inharmonicityCoeff(note);

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
            // tmp.h[4] = Harmonic{5, 0.13, 0.7};
            // tmp.h[5] = Harmonic{6, 0.08, 0.5};
            // tmp.h[6] = Harmonic{7, 0.05, 0.3};

            return tmp;
        };

        Harmonics harmonics_ = generateHarmonics();
};

class Guitar {
    public:
        Guitar() = default;
        Guitar(double samplingRate) : samplingRate_(samplingRate) {};

        void setSamplingRate(double samplingRate) {
            samplingRate_ = samplingRate;
            samplingTime_ = 1.0 / samplingRate;
        };
        
        void noteOn(uint8_t note, uint8_t velocity) {
            note_ = note;
            frequency_ = 440.0 * std::pow(2.0, ((double) note - 69) / 12);

            velocityGain_ = (double) velocity / 127;
            active_ = velocity > 0;
            released_ = false;
            releaseGain_ = 1;
            
            previous_ = 0;
            idx_ = 0;

            if (!active_) {
                delayBuffer_.clear();
                return;
            }

            toDelay_ = samplingRate_ / frequency_;
            const size_t bufferSize = (size_t) std::ceil(toDelay_) + 2;
            delayBuffer_.assign(bufferSize, 0);

            double T60 = decayTime - 0.55 * std::log2(frequency_ / 82.406889);  // Low E
            // T60 = std::clamp(T60, 0.5, 4.2);
            T60 = std::max(T60, 0.5);
            feedbackGain_ = std::exp(std::log(0.001) / (T60 * frequency_));
            releaseMultipler_ = std::exp(std::log(0.001) / (decayTime * samplingRate_));

            ++noteCounter_;

            // std::seed_seq seed;
            std::mt19937 randomizer;
            string(randomizer);
        }
        void noteOff() {
            if (!active_ || released_) return;
            released_ = true;
        }

        bool active() const {
            return active_;
        };
        uint8_t note() const {
            return note_;
        };

        double fromDelay() const {
            if (delayBuffer_.empty()) return 0;

            double position = idx_ - toDelay_;
            const double bufferSize = (double) delayBuffer_.size();
            while (position < 0) {
                position += bufferSize;
            }
            while (position >= bufferSize) {
                position -= bufferSize;
            }

            const size_t i0 = std::floor(position);
            const size_t i1 = (i0 + 1) % delayBuffer_.size();
            const double tmp = position - i0;
            
            return delayBuffer_[i0] + tmp * (delayBuffer_[i1] - delayBuffer_[i0]);
        }

        float render() {
            if (!active_ || delayBuffer_.empty()) return 0;
            
            const double current = fromDelay();
            const double averaged = 0.5 * (current + previous_);

            // delayBuffer_[idx_] = averaged;
            delayBuffer_[idx_] = feedbackGain_ * averaged;
            previous_ = current;
            idx_ = (idx_ + 1) % delayBuffer_.size();

            if (released_) {
                releaseGain_ *= releaseMultipler_;

                if (releaseGain_ < 0.01) {
                    active_ = false;
                    return 0;
                }
            }

            // return (float) current * 0.55 * releaseGain_;
            return (float) std::tanh(1.35 * current) * 0.55 * releaseGain_;
        }

    private:
        void string(std::mt19937 &randomizer) {
            if (delayBuffer_.empty()) return;
            
            std::uniform_real_distribution<double> noise(-1, 1);
            // At least 2 fixed ends
            const size_t stringSamples = std::max<size_t>(2, std::floor(toDelay_));
            double mean = 0;

            for (size_t i = 0; i < delayBuffer_.size(); ++i) {
                // y(x, 0) = hx / pL
                const double x = (double) (i % stringSamples) / (stringSamples - 1);
                const double h = x < pickPosition ? x / pickPosition : (1 - x) / (1 - pickPosition);
                
                const double tmp = 0.92 * h + 0.08 * noise(randomizer);
                delayBuffer_[i] = tmp;
                mean += tmp;
            }

            mean /= delayBuffer_.size();

            for (double &sample: delayBuffer_) {
                sample = (sample - mean) * velocityGain_;
            }
        };

        double samplingRate_ = 44100;
        double samplingTime_ = 1.0 / samplingRate_;

        uint8_t note_ = 60;    // C4
        double frequency_ = 440.0 * std::pow(2.0, -9.0 / 12);

        double velocityGain_;
        double releaseGain_;
        double releaseMultipler_;
        double feedbackGain_;

        std::vector<double> delayBuffer_{};
        size_t idx_ = 0;
        double toDelay_ = 0;

        double previous_;
        
        bool active_ = false;
        bool released_ = false;
        uint32_t noteCounter_ = 0;
};

#endif