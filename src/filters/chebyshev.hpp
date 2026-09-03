#ifndef CHEBYSHEV
#define CHEBYSHEV

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

class ChebyshevI {
    public:
		enum class FilterType {
			Lowpass,
			Highpass,
		};

		explicit ChebyshevI(const uint8_t n, const float Rp, const float Wp, const FilterType &type = FilterType::Lowpass) : n(n), Rp(Rp), Wp(Wp), type_(type) {
			real_.assign((n + 1) / 2, 0.f);
			imag_.assign((n + 1) / 2, 0.f);
			prevW.assign(n, 0.f);
			d = n % 2 ? 1 : pow(10.f, -Rp / 20.f);
			coeffs();
		}

		explicit ChebyshevI(const float Rp, const float Wp, const float Rs, const float Ws, const FilterType &type = FilterType::Lowpass) : Rp(Rp), Wp(Wp), type_(type) {
			const float wp = tan(Wp * pi / 2.f);
			const float ws = tan(Ws * pi / 2.f);

			float tmp;
			if (type_ == FilterType::Lowpass) tmp = acosh(ws / wp);
			else if (type_ == FilterType::Highpass) tmp = acosh(wp / ws);

			const float epsilon = sqrt(pow(10.f, Rp / 10.f) - 1);
			const float A = sqrt(pow(10.f, Rs / 10.f) - 1);

			n = (uint8_t) ceil(acosh(A / epsilon) / tmp);
			real_.assign((n + 1) / 2, 0.f);
			imag_.assign((n + 1) / 2, 0.f);
			prevW.assign(n, 0.f);
			d = n % 2 ? 1 : pow(10.f, -Rp / 20.f);
			coeffs();
		}

		void coeffs() {
			const float a = tan(Wp * pi / 2.f);

			const float epsilon = sqrt(pow(10.f, Rp / 10.f) - 1);
			const float b = pow((1.f / epsilon + sqrt(1.f / epsilon / epsilon + 1.f)), 1.f / n);
			
			const float s = (b * b - 1) / 2 / b;
			const float c = (b * b + 1) / 2 / b;

			for (size_t i = 0; i < n / 2; ++i) {
				const float real = -sin((2.f * ((float) i + 1.f) - 1.f) * pi / 2.f / n) * s;
				const float imag = cos((2.f * ((float) i + 1.f) - 1.f) * pi / 2.f / n) * c;
				const std::complex<float> tmp(real, imag);

				std::complex<float> z;
				if (type_ == FilterType::Lowpass) z = (1.f + tmp * a) / (1.f - tmp * a);
				else if (type_ == FilterType::Highpass) z = (tmp + a) / (tmp - a);

				real_[i] = z.real();
				imag_[i] = z.imag();
			}

			if (n % 2) {
				if (type_ == FilterType::Lowpass) real_[n / 2] = (1.f - s * a) / (1.f + s * a);
				else if (type_ == FilterType::Highpass) real_[n / 2] = (s - a) / (s + a);
				imag_[n / 2] = 0.f;
			}
		}

		float process(const float sample) {
			float output = sample;

			for (size_t i = 0; i < n / 2; ++i) {
				const float sigma = real_[i];
				const float omega = imag_[i];

				const float a1 = -2.f * sigma;
				const float a2 = sigma * sigma + omega * omega;
				float g;
				float b0, b1, b2;

				if (type_ == FilterType::Lowpass) {
					g = (1.f + a1 + a2) / 4.f;
					b0 = 1.f, b1 = 2.f, b2 = 1.f;
				} else if (type_ == FilterType::Highpass) {
					g = (1.f - a1 + a2) / 4.f;
					b0 = 1.f, b1 = -2.f, b2 = 1.f;
				}
				
				// Direct form II
				const size_t j = i * 2;
				const float w0 = output - a1 * prevW[j] - a2 * prevW[j + 1];
				output = (b0 * w0 + b1 * prevW[j] + b2 * prevW[j + 1]) * g;

				prevW[j + 1] = prevW[j];
				prevW[j] = w0;
			}

			if (n % 2) {
				const size_t j = n - 1;
				const float a1 = -real_[n / 2];
				float g;

				if (type_ == FilterType::Lowpass) {
					g = (1.f + a1) / 2.f;
				} else if (type_ == FilterType::Highpass) {
					g = (1.f - a1) / 2.f;
				}

				const float w0 = output - a1  * prevW[j];
				if (type_ == FilterType::Lowpass) {
					output = (w0 + prevW[j]) * g;
				} else if (type_ == FilterType::Highpass) {
					output = (w0 - prevW[j]) * g;
				}

				prevW[j] = w0;
			} else output *= d;

			return output;
		}

		void setCutoff(const float a) {
			Wp = a;
			coeffs();
		}

		void reset() {
			std::fill(prevW.begin(), prevW.end(), 0.f);
		}

	private:
		static constexpr float pi = std::numbers::pi_v<float>;

		uint8_t n;
		float Rp, Wp;

		float d = 1;
		std::vector<float> real_;
		std::vector<float> imag_;
		std::vector<float> prevW;

		FilterType type_ = FilterType::Lowpass;
};

#endif
