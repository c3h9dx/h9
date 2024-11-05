/*
 * H9 project
 *
 * Created by crowx on 2023-10-14.
 *
 * Copyright (C) 2023-2024 Kamil Palkowski. All rights reserved.
 */

#pragma once

#include <exception>
#include <string>

#include "h9frame.h"
#include "api.h"

class DevNodeException: public std::exception {
  protected:
    std::string msg;
    const int _code;

    explicit DevNodeException(int code): _code(code) {};

  public:
    [[nodiscard]] const char* what() const noexcept final {
        return msg.c_str();
    }

    [[nodiscard]] int code() const noexcept {
        return _code;
    }
};

class NodeException: public DevNodeException {
  public:
    explicit NodeException(int code): DevNodeException(-1000 - code) {
        msg = "Node exception: " + std::to_string(code) + " - " + H9frame::error_to_string(H9frame::from_underlying<H9frame::Error>(code)) + ".";
    }
};

class DeviceException: public DevNodeException {
  protected:
    explicit DeviceException(int code): DevNodeException(code) {};
};

class TimeoutException: public DeviceException {
  public:
    TimeoutException(): DeviceException(API::EXECUTION_TIMEOUT) {
        msg = "Timeout exception";
    }
};

class MalformedFrameException: public DeviceException {
  public:
    MalformedFrameException():
        DeviceException(API::MALFORMED_FRAME) {
        msg = "Malformed frame exception";
    }
};

class SizeMismatchException: public DeviceException {
  public:
    SizeMismatchException():
        DeviceException(API::FRAME_SIZE_MISMATCH) {
        msg = "Size mismatch exception";
    }
};

class NodeNotExistException: public DeviceException {
  public:
    NodeNotExistException():
        DeviceException(API::NODE_IS_NOT_EXIST) {
        msg = "Node not exist exception";
    }
};

class InvalidRegisterException: public DeviceException {
  protected:
    const std::uint8_t reg;
    InvalidRegisterException(int code, std::uint8_t reg):
        DeviceException(code), reg(reg) {
        msg = "Register " + std::to_string(reg);
    }
};

class RegisterNotExistException: public InvalidRegisterException {
  public:
    explicit RegisterNotExistException(std::uint8_t reg): InvalidRegisterException(API::REGISTER_IS_NOT_EXIST, reg) {
        msg += " does not exist.";
    }
};

class RegisterNotWritableException: public InvalidRegisterException {
  public:
    explicit RegisterNotWritableException(std::uint8_t reg): InvalidRegisterException(API::REGISTER_IS_NOT_WRITABLE, reg) {
        msg += " is not writable.";
    }
};

class RegisterNotReadableException: public InvalidRegisterException {
  public:
    explicit RegisterNotReadableException(std::uint8_t reg): InvalidRegisterException(API::REGISTER_IS_NOT_READABLE, reg) {
        msg += " is not readable.";
    }
};

class UnsupportedRegisterDataConversionException: public InvalidRegisterException {
  public:
    explicit UnsupportedRegisterDataConversionException(std::uint8_t reg): InvalidRegisterException(API::UNSUPPORTED_REGISTER_DATA_CONVERSION, reg) {
        msg += " unsupported data conversion.";
    }
};

class DeviceNotExistException: public DeviceException {
  public:
    explicit DeviceNotExistException(const std::string& dev_name):
        DeviceException(API::DEV_IS_NOT_EXIST) {
        msg = "Device '" + dev_name + "'not exist exception";
    }
};
