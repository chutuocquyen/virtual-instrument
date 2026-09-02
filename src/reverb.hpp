#ifndef _REVERB_PROCESSOR_
#define _REVERB_PROCESSOR_

#include "processor.hpp"
#include "filters/biquad.hpp"
#include <numeric>

struct Feedback {
    std::vector<float> buffer{};
    size_t idx = 0;
    float damped = 0;

    void resize(const size_t &size) {
        buffer.assign(size, 0);
        idx = 0;
        damped = 0;
    }

    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0);
        idx = 0;
        damped = 0;
    }
};

class reverbProcessor : public Processor {
    public:
        explicit reverbProcessor(const float &samplingRate = 44100) : samplingRate_(samplingRate) {
            update();
            updateDecay();
        };

        float process(const float &sample) override {
            if (!enabled_) return sample;

            // Tap delay
            float a = sample;
            for (size_t i = 0; i < TAP_DELAY_SIZE; ++i) {
                a += tapDelayBuffer_[tapDelayIdx_[i]++] * TAP_GAIN[i];
                if (tapDelayIdx_[i] == tapDelayBuffer_.size()) tapDelayIdx_[i] = 0;
            }
            
            tapDelayBuffer_[tapDelayBufferIdx_++] = sample;
            if (tapDelayBufferIdx_ == tapDelayBuffer_.size()) tapDelayBufferIdx_ = 0;

            // Comb filters
            float b = 0;
            for (size_t i = 0; i < NUM_COMB_FILTERS; ++i) {
                auto &comb = combFilters_[i];
                const float delayed = comb.buffer[comb.idx];
                comb.damped = delayed * (1 - damping_) + comb.damped * damping_;
                comb.buffer[comb.idx++] = a + comb.damped * feedbackGains_[i];

                if (comb.idx == comb.buffer.size()) comb.idx = 0;
                b += delayed;
            }
            b /= NUM_COMB_FILTERS;

            // AP filter
            // y[n] = g * y[n - m] - g * x[n] + x[n - m]
            const float delayed = allpass_.buffer[allpass_.idx];
            float c = delayed - allpassGain_ * b;
            allpass_.buffer[allpass_.idx++] = allpassGain_ * c + b;
            if (allpass_.idx == allpass_.buffer.size()) allpass_.idx = 0;
            
            float d = delayBuffer_[delayBufferIdx_];
            delayBuffer_[delayBufferIdx_++] = c;
            if (delayBufferIdx_ == delayBuffer_.size()) delayBufferIdx_ = 0;

            d = spectralTilt.process(d);
            return sample + d * wet_;
        }

        void enable(const bool &a) override {
            enabled_ = a;
            if (!enabled_) reset();
        }

        void reset() override {
            for (size_t i = 0; i < TAP_DELAY_SIZE; ++i) {
                tapDelayIdx_[i] = (tapDelayBuffer_.size() - (size_t) (TAP_DELAY[i] * samplingRate_)) % tapDelayBuffer_.size();
            }
            std::fill(tapDelayBuffer_.begin(), tapDelayBuffer_.end(), 0);
            tapDelayBufferIdx_ = 0;

            for (auto &a: combFilters_) a.reset();

            allpass_.reset();
            std::fill(delayBuffer_.begin(), delayBuffer_.end(), 0);
            delayBufferIdx_ = 0;
            spectralTilt.reset();
        }

    private:
        static constexpr int TAP_DELAY_SIZE = 6;
        static constexpr int NUM_COMB_FILTERS = 6;
        static constexpr std::array<float, TAP_DELAY_SIZE> TAP_DELAY = {.0199f, .0354f, .0389f, .0414f, .0699f, .0796f};
        static constexpr std::array<float, TAP_DELAY_SIZE> TAP_GAIN = {.841f, .504f, .491f, .379f, .380f, .346f};

        static constexpr std::array<float, NUM_COMB_FILTERS> COMB_FILTER_DELAY = {.05f, .056f, .061f, .068f, .072f, .078f};

        void update() {
            // Tap delay
            for (size_t i = 0; i < TAP_DELAY_SIZE; ++i) {
                tapDelayIdx_[i] = (size_t) (TAP_DELAY[i] * samplingRate_);
            }

            tapDelayBuffer_.assign(tapDelayIdx_[TAP_DELAY_SIZE - 1], 0);
            tapDelayBufferIdx_ = 0;

            for (size_t i = 0; i < TAP_DELAY_SIZE; ++i) {
                tapDelayIdx_[i] = (tapDelayBuffer_.size() - tapDelayIdx_[i]) % tapDelayBuffer_.size();
            }

            // Comb filters
            for (size_t i = 0; i < NUM_COMB_FILTERS; ++i) {
                combFilters_[i].resize((size_t) (COMB_FILTER_DELAY[i] * samplingRate_));
            }

            // Allpass & Delay
            allpass_.resize(allpassDelay_);
            delayBuffer_.resize((size_t) (delay_ * samplingRate_));
        }

        void updateDecay() {
            for (size_t i = 0; i < NUM_COMB_FILTERS; ++i) {
                // -60dB
                feedbackGains_[i] = (float) std::pow(0.001, COMB_FILTER_DELAY[i] / decay_);
            }
        }

        bool enabled_ = false;
        float samplingRate_;

        std::array<size_t, TAP_DELAY_SIZE> tapDelayIdx_{0};
        std::vector<float> tapDelayBuffer_{0};
        size_t tapDelayBufferIdx_ = 0;

        std::array<Feedback, NUM_COMB_FILTERS> combFilters_{};
        std::array<float, NUM_COMB_FILTERS> feedbackGains_{0};

        Feedback allpass_;
        const float allpassGain_ = .7f;
        size_t allpassDelay_ = (size_t) (.006f * samplingRate_);

        std::vector<float> delayBuffer_{0};
        size_t delayBufferIdx_ = 0;
        float delay_ = .03f;

        float decay_ = 1.7f;
        float damping_ = .35f;
        float wet_ = .15f;
        float tone_ = 10.3f;

        Biquad spectralTilt{samplingRate_, 474.f, FilterType::Lowshelf, tone_};
};

#endif
