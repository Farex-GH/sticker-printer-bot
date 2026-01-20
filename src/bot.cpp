#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

#include "tgbot/tgbot.h"
#include "utils.h"
#include "status.h"
#include "bot.h"

#define BYTES_X 0x48

namespace sticker_bot {

static Status WriteFile(const std::string &path, const std::string &data)
{
    FILE *f = fopen(path.c_str(), "wb");
    if (f == NULL) {
        return Status(StatusCode::kInternalError, "Failed to open file");
    }

    const char *contents = data.c_str();
    size_t total_written = 0;
    do {
        size_t num_written = fwrite(&contents[total_written],
                /*size=*/sizeof(*contents), data.size() - total_written, f);
        total_written += num_written;
    } while (total_written != data.size());

    DB_PRINT("Wrote file %s\n", path.c_str());
    fclose(f);
    return Status(StatusCode::kStatusOk);
}

/*
 * Imagemagick cannot automatically determine if something is a webm or not,
 * unless we add a .webm extension to the file. This determines if we need to
 * do so.
 */
static bool IsFileWebm(const std::string &data)
{
    std::string extension = data.substr(0x18, 4);
    return extension == "webm";
}

Status Bot::PrintStickers(const std::vector<std::string> &file_paths)
{

    for (const auto &file : file_paths) {
        auto image = ImageTransform::ImageFromFile(file);
        if (!image.has_value()) {
            return image.error();
        }
        std::unique_ptr<ImageTransform> img = std::move(*image);

        std::vector<uint8_t> data = img->RasterImageDitherAtkinson();
        Status status = printer_->PrintImage(data, BYTES_X * 8);
        if (!status.Ok()) {
           return status;
        }

        int ret = remove(file.c_str());
        if (ret) {
            printf("Couldn't remove file %s, errno %d\n", file.c_str(), ret);
        }
    }

    return Status(StatusCode::kStatusOk);
}

void Bot::PrintStickersAsync(const std::vector<std::string> &file_paths,
                             TgBot::Message::Ptr message)
{
    Status status = PrintStickers(file_paths);

    if (!status.Ok() && status.status() != StatusCode::kTimeout) {
        std::string user_message;

        if (file_paths.size() > 1) {
            user_message = "I couldn't print the stickers";
        } else {
            user_message = "I couldn't print the sticker";
        }
        bot_.getApi().sendMessage(message->chat->id, user_message);
    }
    printer_->PrinterStatus();
}

std::expected<const TgBot::PhotoSize::Ptr, Status> Bot::FindBestPhoto(
        std::span<const TgBot::PhotoSize::Ptr> photos)
{
    ssize_t best_size = 0;
    size_t best_photo_index = 0;

    if (photos.size() == 0) {
        return std::unexpected(Status(StatusCode::kInvalidArgument,
                               "Tried to find the best photo, but none were "
                               "passed in"));
    }

    /*
     * This is usually just the 3rd photo, but compare anyway since it's not
     * expensive.
     */
    for (size_t i = 0; i < photos.size(); i++) {
        if (photos[i]->fileSize > best_size) {
            best_size = photos[i]->fileSize;
            best_photo_index = i;
        }
    }

    return photos[best_photo_index];
}

void Bot::InitBot()
{
    bot_.getEvents().onAnyMessage([this](TgBot::Message::Ptr message) {
        printf("Got message %s\n", message->text.c_str());
        if (StringTools::startsWith(message->text, "/start")) {
            return;
        }

        std::vector<std::string> files;
        /* The path to the file we write to disk. */
        std::vector<std::string> file_paths;

        /*
         * Handle photos.
         * The way photos work is that each photo attached will show up as its
         * own message (each invoking this function), with 4 different sizes of
         * the photo.
         */
        if (message->photo.size() > 0) {
            std::expected<const TgBot::PhotoSize::Ptr, Status> photo =
                    FindBestPhoto(message->photo);
            if (!photo) {
                photo.error().print_status();
            } else {
                TgBot::File::Ptr file =
                    bot_.getApi().getFile(photo.value()->fileId);
                files.push_back(bot_.getApi().downloadFile(file->filePath));
            }
        }

        /* Handle messages with files. */
        if (message->document) {
            TgBot::File::Ptr file =
                bot_.getApi().getFile(message->document->fileId);
            files.push_back(bot_.getApi().downloadFile(file->filePath));
        }

        /* Handle stickers. */
        if (message->sticker) {
            TgBot::File::Ptr file =
                bot_.getApi().getFile(message->sticker->fileId);
            files.push_back(bot_.getApi().downloadFile(file->filePath));
        }

        if (files.empty()) {
            return;
        }

        DB_PRINT("Downloaded %zu files\n", files.size());
        for (size_t i = 0; i < files.size(); i++) {
            /* std::format isn't supported until gcc 13, so do this. */
            char buf[64];
            {
                std::lock_guard<std::mutex> lock(mu_file_num_);
                snprintf(buf, sizeof(buf), "file-%zu", file_num_);
                file_num_++;
            }
            std::string file_name = buf;
            if (IsFileWebm(files[i])) {
                file_name.append(".webm");
            }

            file_paths.push_back(file_name);

            Status status = WriteFile(file_name, files[i]);
            if (!status.Ok()) {
                std::string user_message = files.size() > 1 ?
                    "I couldn't save the images to print them!" :
                    "I couldn't save the image to print it!";
                bot_.getApi().sendMessage(message->chat->id, user_message);
                return;
            }
        }


        /*
         * Make the printing async, because the next message won't be processed
         * until this function returns.
         */
        std::thread t(&Bot::PrintStickersAsync, this, file_paths, message);
        t.detach();
    });
}

Status Bot::RunBot()
{
    try {
        printf("Bot username: %s\n", bot_.getApi().getMe()->username.c_str());
        TgBot::TgLongPoll longPoll(bot_);
        while (true) {
            printf("Long poll started\n");
            longPoll.start();
        }
    } catch (TgBot::TgException& e) {
        return Status(StatusCode::kInternalError, e.what());
    }

    return Status(StatusCode::kStatusOk);
}

};

