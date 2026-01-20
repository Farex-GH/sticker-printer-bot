#include "status.h"
#include "m02_pro.h"
#include "image_transform.h"

#define DEFAULT_TEST_IMG "test.jpg"
#define DEFAULT_PRINTER_PATH "/dev/rfcomm0"
#define BYTES_X 0x48

namespace sticker_bot {

int real_main(int argc, char *argv[])
{
    std::string printer_path = DEFAULT_PRINTER_PATH;
    std::string img_path = DEFAULT_TEST_IMG;
    if (argc >= 3) {
        printer_path = argv[2];
    }
    if (argc >= 2) {
        img_path = argv[1];
    }

    auto m02_pro = M02Pro::Create(printer_path);
    if (!m02_pro.has_value()) {
        m02_pro.error().print_status();
        return -1;
    }
    std::unique_ptr<PrinterInterface> printer = std::move(m02_pro.value());
    auto image = ImageTransform::ImageFromFile(img_path);
    if (!image.has_value()) {
        return -1;
    }
    std::unique_ptr<ImageTransform> img = std::move(*image);

    std::vector<uint8_t> data = img->RasterImageDitherAtkinson();

    Status status = printer->PrintImage(data, BYTES_X * 8);
    if (!status.Ok()) {
        status.print_status();
        return -1;
    }

    return 0;
}

};

int main(int argc, char *argv[])
{
    return sticker_bot::real_main(argc, argv);
}

