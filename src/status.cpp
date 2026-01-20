#include "status.h"

#include <iostream>
#include <string>
#include <string_view>

namespace sticker_bot {

void Status::prepend_message(std::string_view str)
{
    msg_.insert(0, str);
}

void Status::print_status()
{
    std::cout << status_code_stringify(status_) << " " << msg_ << std::endl;
}

std::string Status::status_code_stringify(StatusCode status) {
    switch (status) {
    case StatusCode::kStatusOk:
        return "OK";
    case StatusCode::kInternalError:
        return "Internal error";
    case StatusCode::kInvalidArgument:
        return "Invalid argument";
    default:
        return "Unknown";
    }
}

};
