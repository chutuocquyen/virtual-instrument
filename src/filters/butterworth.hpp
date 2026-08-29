#ifndef _BUTTERWORTH_
#define _BUTTERWORTH_

#include <cmath>
#include <numbers>

enum class FilterType {
    Lowpass,
    Lowshelf,
};

class Butterworth {
    public:
        explicit Butterworth(const float &samplingRate = 44100, const float &cutoff = 10, const float &gain = 6, const FilterType type = FilterType::Lowpass) : samplingRate_(samplingRate), cutoff_(cutoff), gain_(gain), type_(type) {
            update();
        }

        float process(const float &sample) {
            // Direct form II - Transposed
            // y[n] = b0 * x[n] + b1 * x[n - 1] + b2 * x[n - 2] - a1 * y[n - 1] - a2 * y[n - 2]
            const float output = b0 * sample + s1_;
            s1_ = b1 * sample - a1 * output + s2_;
            s2_ = b2 * sample - a2 * output;

            return output;
        }

        void reset() {
            s1_ = 0;
            s2_ = 0;
        }

		void setCutoff(const float &cutoff) {
			cutoff_ = cutoff;
            update();
		}

    private:
        void update () {
            const float omega = 2 * std::numbers::pi_v<float> * cutoff_ / samplingRate_;

            const float cos = std::cos(omega);
            const float sin = std::sin(omega);
            
            // |G(omega)|**2 = 1 / (1 + omega**4) -> pi / 4
            // s**2 + sqrt(2) * s + 1 -> Q = 1 / sqrt(2)
            const float Q = 1 / std::numbers::sqrt2_v<float>;
            const float alpha = sin / 2 / Q;

            switch (type_) {
                case FilterType::Lowpass: {
                    a0 = 1 + alpha;
                    b0 = (1 - cos) / 2 / a0;
                    b1 = (1 - cos) / a0;
                    b2 = b0;
                    a1 = -2 * cos / a0;
                    a2 = (1 - alpha) / a0;
                    break;
                }
                
                case FilterType::Lowshelf: {
                    const float A = std::pow(10, gain_ / 40);
                    
                    a0 = (A + 1) + (A - 1) * cos + 2 * sqrt(A) * alpha;
                    a1 = -2 * ((A - 1) + (A + 1) * cos) / a0;
                    a2 = ((A + 1) + (A - 1) * cos - 2 * sqrt(A) * alpha) / a0;
                    b0 = A * ((A + 1) - (A - 1) * cos + 2 * sqrt(A) * alpha) / a0;
                    b1 = 2 * A * ((A - 1) - (A + 1) * cos) / a0;
                    b2 = A * ((A + 1) - (A - 1) * cos - 2 * sqrt(A) * alpha) / a0;
                    break;
                }
            }
        }

        float samplingRate_;
        float cutoff_;

        float a0, a1, a2, b0, b1, b2;
        float s1_ = 0, s2_ = 0;

        float gain_;

        FilterType type_;
};

#endif
