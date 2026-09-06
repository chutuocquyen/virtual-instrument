#ifndef BIQUAD
#define BIQUAD

#include <cmath>
#include <numbers>

class Biquad {
    public:
        enum class FilterType {
            Lowpass,
            Lowshelf,
            Highshelf,
            Bell,
        };
        
        explicit Biquad(const float samplingRate = 44100, const float cutoff = 10, const FilterType type = FilterType::Lowpass, const float gain = 0, const float Q = 1 / std::numbers::sqrt2_v<float>) : samplingRate_(samplingRate), cutoff_(cutoff), type_(type), gain_(gain), Q_(Q) {
            update();
        }

        float process(const float sample) {
            // Direct form II - Transposed
            // y[n] = b0 * x[n] + b1 * x[n - 1] + b2 * x[n - 2] - a1 * y[n - 1] - a2 * y[n - 2]
            const double output = b0 * sample + s1_;
            s1_ = b1 * sample - a1 * output + s2_;
            s2_ = b2 * sample - a2 * output;

            return (float) output;
        }

        void reset() {
            s1_ = 0;
            s2_ = 0;
        }

		void setCutoff(const float cutoff) {
			cutoff_ = cutoff;
            update();
		}

		void setGain(const float a) {
			gain_ = a;
			update();
		}

    private:
        // https://www.w3.org/TR/audio-eq-cookbook/
        void update () {
            const double omega = 2.0 * std::numbers::pi_v<double> * cutoff_ / samplingRate_;

            const double cos = std::cos(omega);
            const double sin = std::sin(omega);
            
            // |G(omega)|**2 = 1 / (1 + omega**4) -> pi / 4
            // s**2 + sqrt(2) * s + 1 -> Q = 1 / sqrt(2)
            const double alpha = sin / 2.0 / Q_;

            switch (type_) {
                case FilterType::Lowpass: {
					const double tmp = std::sin(omega * 0.5);

                    a0 = 1 + alpha;
                    b0 = tmp * tmp / a0;
                    b1 = 2 * b0;
                    b2 = b0;
                    a1 = -2 * cos / a0;
                    a2 = (1 - alpha) / a0;
                    break;
                }
                
                case FilterType::Lowshelf: {
                    const double A = std::pow(10.0, gain_ / 40.f);
                    
                    a0 = (A + 1) + (A - 1) * cos + 2 * sqrt(A) * alpha;
                    a1 = -2 * ((A - 1) + (A + 1) * cos) / a0;
                    a2 = ((A + 1) + (A - 1) * cos - 2 * sqrt(A) * alpha) / a0;
                    b0 = A * ((A + 1) - (A - 1) * cos + 2 * sqrt(A) * alpha) / a0;
                    b1 = 2 * A * ((A - 1) - (A + 1) * cos) / a0;
                    b2 = A * ((A + 1) - (A - 1) * cos - 2 * sqrt(A) * alpha) / a0;
                    break;
                }

                case FilterType::Highshelf: {
                    const double A = std::pow(10.0, gain_ / 40.f);
                    
                    a0 = (A + 1) - (A - 1) * cos + 2 * sqrt(A) * alpha;
                    a1 = 2 * ((A - 1) - (A + 1) * cos) / a0;
                    a2 = ((A + 1) - (A - 1) * cos - 2 * sqrt(A) * alpha) / a0;
                    b0 = A * ((A + 1) + (A - 1) * cos + 2 * sqrt(A) * alpha) / a0;
                    b1 = -2 * A * ((A - 1) + (A + 1) * cos) / a0;
                    b2 = A * ((A + 1) + (A - 1) * cos - 2 * sqrt(A) * alpha) / a0;
                    break;
                }

                case FilterType::Bell: {
                    const double A = std::pow(10.0, gain_ / 40.f);

                    a0 = 1 + alpha / A;
                    a1 = -2 * cos / a0;
                    a2 = (1 - alpha / A) / a0;
                    b0 = (1 + alpha * A) / a0;
                    b1 = a1;
                    b2 = (1 - alpha * A) / a0;
                    break;
                }
            }
        }

        float samplingRate_;
        float cutoff_;

        float Q_;
        float gain_;

        double a0, a1, a2, b0, b1, b2;
        double s1_ = 0., s2_ = 0.;

        FilterType type_;
};

#endif
