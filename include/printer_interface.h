#ifndef PRINTER_INTERFACE_H
#define PRINTER_INTERFACE_H

#include <cstdint>
#include <expected>
#include <span>

#include "status.h"

namespace sticker_bot {

class PrinterInterface {
  public:
    virtual Status PrintImage(std::span<const uint8_t> data,
                               uint16_t width) = 0;
    virtual Status PrinterStatus() = 0;
};

};

#endif
