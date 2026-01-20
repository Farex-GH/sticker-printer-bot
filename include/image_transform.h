#ifndef IMAGE_TRANSFORM_H
#define IMAGE_TRANSFORM_H

#include <cstdint>
#include <expected>
#include <vector>
#include <string>
#include <string_view>
#include <span>
#include <memory>

#include "status.h"

namespace sticker_bot {

class RgbImage {
  public:
    RgbImage(std::span<uint8_t> img_data, uint32_t width) :
            data_(reinterpret_cast<Rgb *>(img_data.data())),
            size_(img_data.size()),
            width_(width),
            height_(size_ / sizeof(*data_) / width_) {}

    std::vector<uint8_t> RasterImageDitherFloydSteinberg();
    std::vector<uint8_t> RasterImageDitherAtkinson();

  private:
    typedef struct Rgb {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    } __attribute__((packed)) Rgb;

    static inline uint16_t average_pixel(const Rgb *data)
    {
        uint16_t pixel = data->r + data->g + data->b;
        return pixel / 3;
    }

    static inline uint8_t round_pixel(int32_t pixel, uint8_t threshold)
    {
        return pixel > threshold ? 0xff : 0x00;
    }

    static inline uint8_t add_and_cap(uint8_t val1, int32_t val2)
    {
        constexpr uint8_t kMax = 0xff;
        constexpr uint8_t kMin = 0x00;

        int32_t val = val1 + val2;
        if (val > kMax) {
            val = kMax;
        } else if (val < kMin) {
            val = kMin;
        }
        return val;
    }

    static inline void error_propagate(Rgb *rgb, int32_t err,
                                       int32_t numerator)
    {
        rgb->r = add_and_cap(rgb->r, err * numerator / 16);
        rgb->g = add_and_cap(rgb->g, err * numerator / 16);
        rgb->b = add_and_cap(rgb->b, err * numerator / 16);
    }

    static inline void error_propagate_atkinson(Rgb *rgb, int32_t err)
    {
        rgb->r = add_and_cap(rgb->r, err * 1 / 8);
        rgb->g = add_and_cap(rgb->g, err * 1 / 8);
        rgb->b = add_and_cap(rgb->b, err * 1 / 8);
    }

    Rgb *data_;
    size_t size_;
    uint32_t width_;
    uint32_t height_;
};

class ImageTransform {
  public:
    static std::expected<std::unique_ptr<ImageTransform>, Status>
        ImageFromFile(const std::string &path);

    ImageTransform(std::span<uint8_t> data, uint32_t width) :
        /* Subtract by 1 since the RGB data passed in ends with a newline. */
        data_(data.begin(), data.end() - 1),
        width_(width) {}

    std::vector<uint8_t> RasterImageDitherFloydSteinberg();
    std::vector<uint8_t> RasterImageDitherAtkinson();

    static std::expected<std::unique_ptr<ImageTransform>, Status>
        ImageFromRgbFile(const std::string &path, uint32_t width);

  private:
    ImageTransform();

    static std::expected<std::vector<uint8_t>, Status>
        ReadFile(const std::string &path);

    /* Converts to correct width and grayscale, and rotates if needed. */
    static std::expected<std::string, Status>
        ProcessImage(const std::string &path);

    std::vector<uint8_t> data_;
    uint32_t width_;
};

};

#endif
