#ifndef EQUALIZER
#define EQUALIZER

#include "processor.hpp"
#include "filters/biquad.hpp"

class Equalizer : public Processor {
    public:
        explicit Equalizer(const float samplingRate = 44100.f) : samplingRate_(samplingRate), lowEQ_(samplingRate_, lowCutoff_, Biquad::FilterType::Lowshelf, lowGain_, lowQ_), midEQ_(samplingRate_, midCutoff_, Biquad::FilterType::Bell, midGain_, midQ_), highEQ_(samplingRate_, highCutoff_, Biquad::FilterType::Highshelf, highGain_, highQ_) {}

        float process(const float sample) override {
			if (!enabled_) return sample;

			const float low = lowEQ_.process(sample);
			const float mid = midEQ_.process(low);
			const float high = highEQ_.process(mid);

			return high * level_;
        }

        void enable(const bool a) override {
            enabled_ = a;
            if (!enabled_) reset();
        }

        void reset() override {
			lowEQ_.reset();
			midEQ_.reset();
			highEQ_.reset();
        }

    private:
        bool enabled_ = false;

        float samplingRate_ = 44100.f;

        float lowQ_ = .70710687f, midQ_ = 1.f, highQ_ = .70710687f;
        float lowGain_ = 10.f, midGain_ = 2.f, highGain_ = -3.f;
        float lowCutoff_ = 200.f, midCutoff_ = 500.f, highCutoff_ = 1200.f;

        float level_ = 1.2f;

        Biquad lowEQ_, midEQ_, highEQ_;
};

// https://github.com/grame-cncm/faustlibraries/blob/master/tonestacks.lib
template<typename TS>
class ToneStacks : public Processor {
    public:
        explicit ToneStacks(const float samplingRate = 44100.f) : samplingRate_(samplingRate) {
            update();
        }

        float process(const float sample) override {
            if (!enabled_) return sample;

            const float output = b0_ * sample + s1;

            s1 = b1_ * sample - a1_ * output + s2;
            s2 = b2_ * sample - a2_ * output + s3;
            s3 = b3_ * sample - a3_ * output;

            return output * level_;
        }

        void enable(const bool a) override {
            enabled_ = a;
            if (!enabled_) reset();
        }

        void reset() override {
            s1 = 0.f;
            s2 = 0.f;
            s3 = 0.f;
        }

    private:
        void update() {
            constexpr double R1 = TS::R1;
            constexpr double R2 = TS::R2;
            constexpr double R3 = TS::R3;
            constexpr double R4 = TS::R4;

            constexpr double C1 = TS::C1;
            constexpr double C2 = TS::C2;
            constexpr double C3 = TS::C3;

            const double t = treble_, m = middle_;
            const double l = exp((bass_ - 1) * 3.4f);

            const double b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
            const double b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                           + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                           + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
                           + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                           + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
            const double b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                           - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                           + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                           + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
                           + t * l * C1 * C2 * C3 * R1 * R2 * R4;

            const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4)
                           + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
            const double a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3
                           + C2 * C3 * R3 * R3) + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                           - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + l * (C1 * C2 * R2 * R4
                           + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
                           + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4
                           + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
            const double a3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                           - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                           + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3
                           - C1 * C2 * C3 * R1 * R3 * R4) + l * C1 * C2 * C3 * R1 * R2 * R4
                           + C1 * C2 * C3 * R1 * R3 * R4;

            const double c = 2.f * samplingRate_;
            const double c2 = c * c;
            const double c3 = c2 * c;

            const double B0 = -b1 * c - b2 * c2 - b3 * c3;
            const double B1 = -b1 * c + b2 * c2 + 3.f * b3 * c3;
            const double B2 = b1 * c + b2 * c2 - 3.f * b3 * c3;
            const double B3 = b1 * c - b2 * c2 + b3 * c3;

            const double A0 = -1.f - a1 * c - a2 * c2 - a3 * c3;
            const double A1 = -3.f - a1 * c + a2 * c2 + 3.f * a3 * c3;
            const double A2 = -3.f + a1 * c + a2 * c2 - 3.f * a3 * c3;
            const double A3 = -1.f + a1 * c - a2 * c2 + a3 * c3;

            a1_ = (float) (A1 / A0);
            a2_ = (float) (A2 / A0);
            a3_ = (float) (A3 / A0);

            b0_ = (float) (B0 / A0);
            b1_ = (float) (B1 / A0);
            b2_ = (float) (B2 / A0);
            b3_ = (float) (B3 / A0);
        }

        bool enabled_ = false;

        float samplingRate_;

        float level_ = 2.f;
        float bass_ = .5f, middle_ = 1.f, treble_ = 1.f;

        float a1_, a2_, a3_, b0_, b1_, b2_, b3_;
        float s1 = 0.f, s2 = 0.f, s3 = 0.f;
};

struct FenderBassman {
    static constexpr double R1 = 250e3;
    static constexpr double R2 = 1e6;
    static constexpr double R3 = 25e3;
    static constexpr double R4 = 56e3;

    static constexpr double C1 = .25e-9;
    static constexpr double C2 = 20e-9;
    static constexpr double C3 = 20e-9;
};

struct VoxAC30 {
    static constexpr double R1 = 1e6;
    static constexpr double R2 = 1e6;
    static constexpr double R3 = 10e3;
    static constexpr double R4 = 100e3;

    static constexpr double C1 = .05e-9;
    static constexpr double C2 = 22e-9;
    static constexpr double C3 = 22e-9;
};

#endif
