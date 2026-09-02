#ifndef THIRAN
#define THIRAN

#include <cmath>

class Thiran {
    public:
        Thiran() = default;

        void coeffs(const float B, const size_t M, const uint8_t note) {
            const float lnB = log(B), lnM = log((float) M), I = (float) note - 20.f;
            const float Cd = exp((m1 * lnM + m2) * lnB + m3 * lnM + m4);
            const float kd = exp(k1 * lnB * lnB + k2 * lnB + k3);
            d = exp(Cd - I * kd);
            a = (1.f - d) / (d + 1.f);
        }

        void coeffs(const float delay) {
            d = delay;
            a = (1.f - d) / (d + 1.f);
        }
        
        float phaseDelay(const float omega) const {
            if (omega == 0.f) return d;
            
            return 2.f / omega * atan(d * tan(omega * .5f));
        }

        float process(const float X) {
            // H(z) = (a + z**(-1)) / (1 + a * z**(-1))
            // a = (1 - d) / (d + 1)
            // y[n] = a * x[n] + x[n - 1] - a * y[n - 1]
            const float Y = a * X + prevX - a * prevY;
            prevX = X;
            prevY = Y;
            return Y;
        }

        void reset() {
            prevX = 0;
            prevY = 0;
        }

    private:
        static constexpr float k1 = -0.00179f;
        static constexpr float k2 = -0.0233f;
        static constexpr float k3 = -2.93f;
        static constexpr float m1 = 0.0126f;
        static constexpr float m2 = 0.0606f;
        static constexpr float m3 = -0.00825f;
        static constexpr float m4 = 1.97f;

        float a, d;
        float prevX = 0, prevY = 0;
};

#endif