#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace risha::gfx {

class CoverageBuffer {
public:
    CoverageBuffer(int width, int height) : width_(std::max(1, width)), height_(std::max(1, height)), stride_(width_ + 2), data_(static_cast<size_t>(stride_) * static_cast<size_t>(height_), 0.0f) {}

    void line(float x0, float y0, float x1, float y1) {
        if (y0 == y1) return;
        float direction = 1.0f;
        if (y0 > y1) {
            std::swap(x0, x1);
            std::swap(y0, y1);
            direction = -1.0f;
        }
        const float dxdy = (x1 - x0) / (y1 - y0);
        float x = x0;
        if (y0 < 0.0f) x -= y0 * dxdy;
        const int first = std::max(0, static_cast<int>(std::floor(y0)));
        const int last = std::min(height_, static_cast<int>(std::ceil(y1)));
        for (int y = first; y < last; ++y) {
            float* row = data_.data() + static_cast<size_t>(y) * static_cast<size_t>(stride_);
            const float dy = std::min(static_cast<float>(y + 1), y1) - std::max(static_cast<float>(y), y0);
            const float next = x + dxdy * dy;
            const float d = dy * direction;
            const float left = std::min(x, next);
            const float right = std::max(x, next);
            const float left_floor = std::floor(left);
            const int left_index = clamp_x(static_cast<int>(left_floor));
            const float right_ceil = std::ceil(right);
            const int right_index = clamp_x(static_cast<int>(right_ceil));
            if (right_index <= left_index + 1) {
                const float mid = 0.5f * (x + next) - left_floor;
                row[left_index] += d - d * mid;
                row[left_index + 1] += d * mid;
            } else {
                const float s = 1.0f / (right - left);
                const float left_fraction = left - left_floor;
                const float a0 = 0.5f * s * (1.0f - left_fraction) * (1.0f - left_fraction);
                const float right_fraction = right - right_ceil + 1.0f;
                const float am = 0.5f * s * right_fraction * right_fraction;
                row[left_index] += d * a0;
                if (right_index == left_index + 2) {
                    row[left_index + 1] += d * (1.0f - a0 - am);
                } else {
                    const float a1 = s * (1.5f - left_fraction);
                    row[left_index + 1] += d * (a1 - a0);
                    for (int xi = left_index + 2; xi < right_index - 1; ++xi) row[xi] += d * s;
                    const float a2 = a1 + static_cast<float>(right_index - left_index - 3) * s;
                    row[right_index - 1] += d * (1.0f - a2 - am);
                }
                row[right_index] += d * am;
            }
            x = next;
        }
    }

    template <typename Sink>
    void resolve(Sink&& sink) const {
        for (int y = 0; y < height_; ++y) {
            const float* row = data_.data() + static_cast<size_t>(y) * static_cast<size_t>(stride_);
            float accumulated = 0.0f;
            for (int x = 0; x < width_; ++x) {
                accumulated += row[x];
                const float coverage = std::min(1.0f, std::fabs(accumulated));
                if (coverage > 0.003f) sink(x, y, coverage);
            }
        }
    }

    int width() const { return width_; }
    int height() const { return height_; }

private:
    int clamp_x(int x) const { return std::clamp(x, 0, width_); }
    int width_;
    int height_;
    int stride_;
    std::vector<float> data_;
};

}
