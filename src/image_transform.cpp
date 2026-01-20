#include "image_transform.h"

#include <cstdint>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <expected>
#include <vector>
#include <string>
#include <string_view>
#include <span>
#include <memory>
#include <sstream>

#include "status.h"

namespace sticker_bot {

static std::vector<std::string> SplitString(const std::string &str, char delim)
{
    std::stringstream ss(str);
    std::string tmp;
    std::vector<std::string> strings;

    while (getline(ss, tmp, delim)) {
        strings.push_back(tmp);
    }
    return strings;
}

static std::expected<std::string, Status> Execute(const std::string &cmd)
{
    std::string result;
    char buffer[256];

    FILE *f = popen(cmd.c_str(), "r");
    if (f == NULL) {
        return std::unexpected(Status(StatusCode::kInternalError,
                "Failed to run command"));
    }

    while (fgets(buffer, sizeof(buffer), f) != NULL) {
        result += buffer;
    }

    pclose(f);
    return result;
}

std::expected<std::string, Status>
    ImageTransform::ProcessImage(const std::string &path)
{
    const std::string kIdentifyCmd = "identify";
    const std::string kConvertCmd = "convert";
    const std::string kOutputImageName = "image.rgb";
    /* TODO: Normalize? */
    const std::string kConvertArgs =
        "-background white -flatten -resize 576x "
        "-colorspace gray -negate " + kOutputImageName;
    const std::string kRotateArgs = "-rotate 90";
    static constexpr uint8_t kDimensionsOffset = 2;

    /* Get info on the image. */
    std::string cmd = kIdentifyCmd + " " + path;
    auto output = Execute(cmd);
    if (!output.has_value()) {
        return std::unexpected(output.error());
    }

    /* Grab the dimensions */
    std::vector<std::string> strings = SplitString(*output, ' ');
    if (strings.size() <= kDimensionsOffset) {
        return std::unexpected(Status(StatusCode::kInternalError,
                "Image information output does not contain dimensions"));
    }
    std::string dimension = strings[kDimensionsOffset];

    /*
     * The format is ${X_PIXELS}x${Y_PIXELS} potentilly with '+...' appended
     * to the end. STOI will stop at the non-int character.
     */
    std::vector<std::string> dimensions = SplitString(dimension, 'x');
    if (dimensions.size() < 2) {
        return std::unexpected(Status(StatusCode::kInternalError,
                "Failed to correctly parse dimensions"));
    }
    uint32_t x_pixels = std::stoi(dimensions[0]);
    uint32_t y_pixels = std::stoi(dimensions[1]);

    /*
     * If it's larger in the X direction, rotate it so we can print at a higher
     * resolution.
     */
    if (x_pixels > y_pixels) {
        cmd = kConvertCmd + " " + path + " " + kRotateArgs + " " + path;
        output = Execute(cmd);
        if (!output.has_value()) {
            return std::unexpected(output.error());
        }
    }

    /* Append [0] to the path so we only convert the first frame of the image */
    std::string path_first_frame = path;
    path_first_frame.append("[0]");

    /*
     * Finally, convert the image to grayscale and RGB format.
     * TODO: This unconditionally resizes, should we do this instead of padding
     * with space?
     */
    cmd = kConvertCmd + " " + path_first_frame + " " + kConvertArgs;
    output = Execute(cmd);
    if (!output.has_value()) {
        return std::unexpected(output.error());
    }

    return kOutputImageName;
}

std::expected<std::vector<uint8_t>, Status>
    ImageTransform::ReadFile(const std::string &path)
{
    FILE *f = fopen(path.c_str(), "r");
    if (f == NULL) {
        return std::unexpected(Status(StatusCode::kNotFoundError,
                                      "Failed to open file"));
    }

    /* Get the size of the file. */
    if (fseek(f, 0, SEEK_END)) {
        return std::unexpected(Status(StatusCode::kInternalError,
                                      "Failed to seek file"));
    }
    size_t file_size = ftell(f);
    /* Reset the seek pointer to the start. */
    rewind(f);

    std::vector<uint8_t> data(file_size);

    size_t total_read = 0;
    do {
        size_t num_read = fread(&data[total_read], /*size=*/sizeof(data[0]),
                                 file_size - total_read, f);
        total_read += num_read;
    } while (total_read != file_size);

    fclose(f);

    return data;
}

std::vector<uint8_t> ImageTransform::RasterImageDitherFloydSteinberg()
{
    RgbImage img(data_, width_);
    return img.RasterImageDitherFloydSteinberg();
}

std::vector<uint8_t> ImageTransform::RasterImageDitherAtkinson()
{
    RgbImage img(data_, width_);
    return img.RasterImageDitherAtkinson();
}

/*
 * We cannot determine the dimensions from the file, but we can if we know the
 * width.
 */
std::expected<std::unique_ptr<ImageTransform>, Status>
    ImageTransform::ImageFromRgbFile(const std::string &path, uint32_t width)
{

    auto data = ReadFile(path);
    if (!data.has_value()) {
        return std::unexpected(data.error());
    }

    return std::make_unique<ImageTransform>(*data, width);
}

std::expected<std::unique_ptr<ImageTransform>, Status>
    ImageTransform::ImageFromFile(const std::string &path)
{
    static constexpr uint16_t kImageWidth = 576;

    auto transformed_img_path = ProcessImage(path);
    if (!transformed_img_path.has_value()) {
        return std::unexpected(transformed_img_path.error());
    }

    auto data = ReadFile(*transformed_img_path);
    if (!data.has_value()) {
        return std::unexpected(data.error());
    }

    return std::make_unique<ImageTransform>(*data, kImageWidth);
}

std::vector<uint8_t> RgbImage::RasterImageDitherAtkinson()
{
    const uint8_t threshold = 0x80;
    /* Each byte in the raster is 8 pixels (dots). */
    std::vector<uint8_t> print_img(width_ * height_ / 8, 0);

    size_t print_img_offset = 0;
    uint8_t print_img_byte_shift = 7;
    for (size_t i = 0; i < width_ * height_; i++) {
        uint32_t curr_y = i / width_;
        uint32_t curr_x = i % width_;

        int32_t old_pixel = average_pixel(&data_[i]);
        uint8_t new_pixel = round_pixel(old_pixel, threshold);
        int32_t err = old_pixel - new_pixel;

        /*
         * Write out the pixel. Note that if the color is 0, that means it's a
         * black pixel, which means that we would set the bit in the raster
         * data. However, we previously inverted the image, so a pixel value of
         * 0xff means we set the bit in the raster data.
         */
        if (new_pixel == 0xff) {
            print_img[print_img_offset] |= (1 << print_img_byte_shift);
        }
        if (print_img_byte_shift == 0) {
            /*
             * If an 8-bit bitmap happens to be a newline character, the printer
             * FW will do a newline rather than print out each pixel in the
             * bitmap.
             * To work around this HIGH QUALITY firmware, convert the bitmap
             * from 0b00001010 to 0b00010100, since it's close enough to what we
             * wanted.
             */
            if (print_img[print_img_offset] == 0x0a) {
                print_img[print_img_offset] = 0x14;
            }
            print_img_byte_shift = 7;
            print_img_offset++;
        } else {
            print_img_byte_shift--;
        }

        /*
         * Dither using the Atkinson algorithm.
         * Error propagation is done as follows
         * ... ... ... ...
         * ... cur 1/8 1/8
         * 1/8 1/8 1/8 ...
         * ... 1/8 ... ...
         */
        /* R */
        if (curr_x < width_) {
            error_propagate_atkinson(&data_[i + 1], err);
        }
        /* 2R */
        if (curr_x < width_ - 1) {
            error_propagate_atkinson(&data_[i + 2], err);
        }
        /* DL */
        if (curr_y < height_ && curr_x != 0) {
            error_propagate_atkinson(&data_[i + width_ - 1], err);
        }
        /* D */
        if (curr_y < height_) {
            error_propagate_atkinson(&data_[i + width_], err);
        }
        /* DR */
        if (curr_y < height_ && curr_x < width_) {
            error_propagate_atkinson(&data_[i + width_ + 1], err);
        }
        /* 2D */
        if (curr_y < height_ - 1) {
            error_propagate_atkinson(&data_[i + (width_ * 2)], err);
        }
    }

    return print_img;
}

std::vector<uint8_t> RgbImage::RasterImageDitherFloydSteinberg()
{
    const uint8_t threshold = 0x80;
    /* Each byte in the raster is 8 pixels (dots). */
    std::vector<uint8_t> print_img(width_ * height_ / 8);

    size_t print_img_offset = 0;
    uint8_t print_img_byte_shift = 7;
    for (size_t i = 0; i < width_ * height_; i++) {
        uint32_t curr_y = i / height_;
        uint32_t curr_x = i % width_;

        int32_t old_pixel = average_pixel(&data_[i]);
        uint8_t new_pixel = round_pixel(old_pixel, threshold);
        int32_t err = old_pixel - new_pixel;

        /*
         * Write out the pixel. Note that if the color is 0, that means it's a
         * black pixel, which means that we would set the bit in the raster
         * data. However, we previously inverted the image, so a pixel value of
         * 0xff means we set the bit in the raster data.
         */
        if (new_pixel == 0xff) {
            print_img[print_img_offset] |= (1 << print_img_byte_shift);
        }
        if (print_img_byte_shift == 0) {
            /*
             * If an 8-bit bitmap happens to be a newline character, the printer
             * FW will do a newline rather than print out each pixel in the
             * bitmap.
             * To work around this HIGH QUALITY firmware, convert the bitmap
             * from 0b00001010 to 0b00010100, since it's close enough to what we
             * wanted.
             */
            if (print_img[print_img_offset] == 0x0a) {
                print_img[print_img_offset] = 0x14;
            }
            print_img_byte_shift = 7;
            print_img_offset++;
        } else {
            print_img_byte_shift--;
        }

        /*
         * Dither using the Floyd Steinberg algorithm.
         * Error propagation is done as follows
         * .... .... ....
         * .... curr 7/16
         * 3/16 5/16 1/16
         */
        /* R */
        if (curr_x < width_) {
            error_propagate(&data_[i + 1], err, 7);
        }
        /* DL */
        if (curr_y < height_ && curr_x != 0) {
            error_propagate(&data_[i + width_ - 1], err, 3);
        }
        /* D */
        if (curr_y < height_) {
            error_propagate(&data_[i + width_], err, 5);
        }
        /* DR */
        if (curr_y < height_ && curr_x < width_) {
            error_propagate(&data_[i + width_ + 1], err, 1);
        }
    }

    return print_img;
}

};

