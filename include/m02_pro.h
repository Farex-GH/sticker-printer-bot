#ifndef M02_PRO_H
#define M02_PRO_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <unistd.h>

#include "printer_interface.h"
#include "status.h"

namespace sticker_bot {

class M02Pro : public PrinterInterface {
  public:
    static std::expected<std::unique_ptr<M02Pro>, Status>
       Create(const std::string &path);

    M02Pro(int fd, std::string_view path) : fd_(fd), path_(path) {}
    ~M02Pro() { close(fd_); }

    Status PrintImage(std::span<const uint8_t> data, uint16_t width) override;
    Status PrinterStatus() override;

  private:
    static constexpr uint32_t kMaxBufferSize = 0x10000;
    /* Long prints can take awhile. */
    static constexpr uint32_t kReadTimeoutSec = 30;

    enum class M02ProPrintRasterImageMode : uint8_t {
        /* The printer FW does 0x00, but 0x30 matches ESC/POS spec. */
        kNormal = 0x30,
        kDoubleWidth = 0x31,
        kDoubleHeight = 0x32,
        kQuadruple = 0x33,
    };

    Status SendCmd(std::span<const uint8_t> data);
    std::expected<ssize_t, Status> DoRead(std::span<uint8_t> data);
    Status ReadData(std::span<uint8_t> data, size_t min_to_read);
    Status SendLineFeed(uint8_t rows);
    Status InitPrinter();
    Status PrintRasterImage(std::span<const uint8_t> data, uint16_t bytes_x,
                            uint16_t bytes_y);

    int fd_;
    const std::string path_;
    std::mutex mu_printer_;
};

};

#endif
