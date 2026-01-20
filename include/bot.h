#ifndef BOT_H
#define BOT_H

#include <string>
#include <string_view>
#include <vector>

#include "tgbot/tgbot.h"
#include "status.h"
#include "printer_interface.h"
#include "image_transform.h"

namespace sticker_bot {

class Bot {
  public:
    /* Token needs to be mutable for the tgbot ctor. */
    Bot(std::string token, std::unique_ptr<PrinterInterface> printer) :
        bot_(token),
        printer_(std::move(printer)) {}

    void InitBot();
    Status RunBot();

  private:
    Status PrintStickers(const std::vector<std::string> &file_paths);
    void PrintStickersAsync(const std::vector<std::string> &file_paths,
                            TgBot::Message::Ptr message);
    std::expected<const TgBot::PhotoSize::Ptr, Status> FindBestPhoto(
            std::span<const TgBot::PhotoSize::Ptr> photos);

    TgBot::Bot bot_;
    std::unique_ptr<PrinterInterface> printer_;
    uint64_t file_num_;  // For creating a unique file name.
    std::mutex mu_file_num_;
};

};

#endif
