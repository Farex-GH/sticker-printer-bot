#ifndef STATUS_H
#define STATUS_H

#include <expected>
#include <string_view>
#include <string>


namespace sticker_bot {

#define RETURN_IF_ERROR(expr)   \
    {                           \
        auto __result = (expr); \
        if (!__result.Ok()) {   \
            return __result;    \
        }                       \
    }

enum class StatusCode {
    kStatusOk = 0x00,
    kInternalError = 0x01,
    kInvalidArgument = 0x02,
    kTimeout = 0x03,
    kNotFoundError = 0x04,
};

class Status {
  public:
    Status(StatusCode status, std::string_view msg,
           std::string_view user_friendly_msg) :
        status_(status),
        msg_(msg),
        user_friendly_msg_(user_friendly_msg) {}
    Status(StatusCode status, std::string_view msg) :
        Status(status, msg, /*user_friendly_msg=*/"") {}
    Status(StatusCode status) :
        Status(status, /*msg=*/"", /*user_friendly_msg=*/"") {}

    void prepend_message(std::string_view str);
    // Prints the message with status code stringified.
    void print_status();

    bool Ok() { return status_ == StatusCode::kStatusOk; }
    StatusCode status() { return status_; }
    std::string user_friendly_message() { return user_friendly_msg_; }
    std::string message() { return msg_; }

  private:
    std::string status_code_stringify(StatusCode status);

    StatusCode status_;
    std::string msg_;
    std::string user_friendly_msg_;
};

};

#endif
