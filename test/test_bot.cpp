#include <string>
#include <memory>

#include "status.h"
#include "m02_pro.h"
#include "image_transform.h"
#include "bot.h"

#define DEFAULT_PRINTER_PATH "/dev/rfcomm0"
#define BYTES_X 0x48

namespace sticker_bot {

int real_main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <Telegram API key> [Printer Path]\n", argv[0]);
        return -1;
    }
    std::string token = argv[1];

    std::string printer_path = DEFAULT_PRINTER_PATH;
    if (argc >= 3) {
        printer_path = argv[2];
    }

    auto m02_pro = M02Pro::Create(printer_path);
    if (!m02_pro.has_value()) {
        m02_pro.error().print_status();
        return -1;
    }
    std::unique_ptr<PrinterInterface> printer = std::move(m02_pro.value());

    Bot bot(token, std::move(printer));

    bot.InitBot();
    bot.RunBot();

    return 0;
}

};

int main(int argc, char *argv[])
{
    return sticker_bot::real_main(argc, argv);
}

