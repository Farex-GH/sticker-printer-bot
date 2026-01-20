#include "m02_pro.h"

#include <cstdint>
#include <expected>
#include <mutex>
#include <thread>
#include <memory>
#include <span>
#include <vector>
#include <endian.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

#include "status.h"
#include "utils.h"

namespace sticker_bot {

Status M02Pro::PrintImage(std::span<const uint8_t> data, uint16_t width)
{
    /*
     * The width is in terms of pixels. The raster format encodes 8 pixels in 1
     * byte, so divide the width by 8 to get the raster image width.
     */
    uint16_t bytes_x = width / 8;

    std::lock_guard<std::mutex> lock(mu_printer_);
    RETURN_IF_ERROR(PrintRasterImage(data, bytes_x, data.size() / bytes_x));

    RETURN_IF_ERROR(SendLineFeed(3));

    /* Read the message the printer says when it finishes printing. */
    std::vector<uint8_t> buf(256);
    Status ret = ReadData(buf, 0);

    /* TODO: Log all statuses until we know what they mean. */
    DB_PRINT("Print and status read done, Status OK=%d\n", ret.Ok());
    if (ret.Ok()) {
        printf("Printer status: %.2x %.2x\n", buf[0], buf[1]);
    }
    return ret;
}

Status M02Pro::PrinterStatus()
{
    const std::vector<uint8_t> kReadBattery = {0x1f, 0x11, 0x08};
    constexpr uint8_t kBatteryStatus = 0x04;
    std::vector<uint8_t> data(8);

    std::lock_guard<std::mutex> lock(mu_printer_);
    RETURN_IF_ERROR(SendCmd(kReadBattery));
    RETURN_IF_ERROR(ReadData(data, 0));

    /*
     * TODO: Understand more statuses and determine if they're bad, especially
     * the "out of paper" status.
     */
    if (data[0] == kBatteryStatus) {
        printf("Battery life is %d%%\n", data[1]);
    } else {
        printf("Unknown status %.2x %.2x\n", data[0], data[1]);
    }
    return Status(StatusCode::kStatusOk);
}

std::expected<std::unique_ptr<M02Pro>, Status>
        M02Pro::Create(const std::string &path)
{
    int fd = open(path.c_str(), O_RDWR);
    if (fd < 0) {
        return std::unexpected(Status(StatusCode::kInvalidArgument,
                      "Failed to open M02 Pro file descriptor"));
    }

    return std::move(std::make_unique<M02Pro>(fd, path));
}

Status M02Pro::SendCmd(std::span<const uint8_t> data)
{
    size_t total_written = 0;
    while (total_written < data.size()) {
        size_t num_to_write = (data.size() - total_written) >
            kMaxBufferSize ? kMaxBufferSize :
            (data.size() - total_written);

        ssize_t bytes_written = write(fd_, &data[total_written],
                num_to_write);
        if (bytes_written < 0) {
            return Status(StatusCode::kInternalError, "Failed to send data");
        }

        DB_PRINT("%s: wrote %zd bytes\n", __func__, bytes_written);
        DB_PRINT_ARRAY(&data[total_written], bytes_written);

        total_written += bytes_written;
    }

    return Status(StatusCode::kStatusOk);
}

std::expected<ssize_t, Status> M02Pro::DoRead(std::span<uint8_t> data)
{
    ssize_t bytes_read = read(fd_, data.data(), data.size());
    if (bytes_read < 0) {
        return std::unexpected(Status(StatusCode::kInternalError,
                      "Failed to read data from printer"));
    }

    DB_PRINT("%s: read %zd bytes\n", __func__, bytes_read);
    DB_PRINT_ARRAY(data, bytes_read);

    return bytes_read;
}

/* Passing 0 for min_to_read is valid, and will only issue 1 read() call. */
Status M02Pro::ReadData(std::span<uint8_t> data, size_t min_to_read)
{
    size_t total_read = 0;
    fd_set set;
    ssize_t bytes_read;

    do {
        /* set and timeout must be reset on each iteration. */
        FD_ZERO(&set);
        FD_SET(fd_, &set);
        struct timeval timeout;
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = kReadTimeoutSec;

        int ret = select(fd_ + 1, /*readfds=*/&set, /*writefds=*/NULL,
                         /*exceptfds=*/NULL, &timeout);
        if (ret < 0) {
            /* Error. */
            return Status(StatusCode::kInternalError,
                          "Failed to select on read");
        } else if (ret == 0) {
            /* Timeout. */
            return Status(StatusCode::kTimeout, "Timed out on read");
        } else {
            /* OK. */
            auto bytes_read_or = DoRead(data.subspan(total_read,
                        data.size() - total_read));
            if (!bytes_read_or.has_value()) {
                return bytes_read_or.error();
            }
            bytes_read = *bytes_read_or;
        }

        total_read += bytes_read;
    } while (total_read < min_to_read);

    return Status(StatusCode::kStatusOk);
}

Status M02Pro::SendLineFeed(uint8_t rows)
{
    std::vector<uint8_t> cmd {0x1b, 0x64, rows};

    Status status = SendCmd(cmd);
    if (!status.Ok()) {
        status.prepend_message("Failed to send line feed");
    }
    return status;
}

Status M02Pro::InitPrinter()
{
    const std::vector<uint8_t> kCmd = {0x1b, 0x40};

    Status status = SendCmd(kCmd);
    if (!status.Ok()) {
        status.prepend_message("Failed to initialize printer: ");
    }

    return status;
}

Status M02Pro::PrintRasterImage(std::span<const uint8_t> data,
                                  uint16_t bytes_x, uint16_t bytes_y)
{
    /* For simplicity, just always do normal mode. */
    std::vector<uint8_t> cmd = {0x1d, 0x76, 0x30,
                      static_cast<uint8_t>(M02ProPrintRasterImageMode::kNormal),
                      static_cast<uint8_t>(htole16(bytes_x) & 0xff),
                      static_cast<uint8_t>(htole16(bytes_x) >> 8),
                      static_cast<uint8_t>(htole16(bytes_y) & 0xff),
                      static_cast<uint8_t>(htole16(bytes_y) >> 8)};

    RETURN_IF_ERROR(InitPrinter());

    Status status = SendCmd(cmd);
    if (!status.Ok()) {
        status.prepend_message("Failed to send raster image header");
        return status;
    }

    status = SendCmd(data);
    if (!status.Ok()) {
        status.prepend_message("Failed to send raster image");
        return status;
    }

    DB_PRINT("%s: Sent %zu-byte raster image\n", __func__, data.size());

    return Status(StatusCode::kStatusOk);
}

};
