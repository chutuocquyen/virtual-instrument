#ifndef _NOTE_SYNTHESIS_
#define _NOTE_SYNTHESIS_

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

constexpr double pi = std::numbers::pi;

constexpr std::array<double, 7> harmonicRatios{
    1.000, 2.005, 3.012, 4.021, 5.035, 6.058, 7.090
};
constexpr std::array<double, 7> harmonicAmplitudes{
    1.00, 0.55, 0.32, 0.20, 0.13, 0.08, 0.05
};
constexpr std::array<double, 7> harmonicDecays{
    1.8, 1.5, 1.2, 1.0, 0.7, 0.5, 0.3
};

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
            velocityGain_ = (double) velocity / 127;
        };
        void noteOff() {
            if (!active_ || released_) return;
            released_ = true;
            releaseTime_ = 0;
        };

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

            for (size_t i = 0; i < harmonicRatios.size(); ++i) {
                sample += harmonicAmplitudes[i] * std::exp(-lastTime_ / harmonicDecays[i]) * std::sin(2 * pi * frequency_ * harmonicRatios[i] * lastTime_);
            }

            const double hammer = 0.12 * std::exp(-lastTime_ / 0.018) * std::sin(2 * pi * frequency_ * 8.7 * lastTime_);

            sample = 0.34 * velocityGain_ * attack * release * (sample + hammer);
            
            lastTime_ += samplingTime_;
            if (released_) {
                releaseTime_ += samplingTime_;
                if (release < 0.0001) active_ = false;
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
};

#endif