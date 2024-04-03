/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-23.
 *
 * Copyright (C) 2020-2021 Kamil Palkowski. All rights reserved.
 */

#include "node.h"

#include <cassert>

#include "dev_node_exception.h"
#include "h9d_configurator.h"
#include "tcpclientthread.h"

NodeDescLoader Node::nodedescloader;

void Node::update_node_last_seen_time() noexcept {
    _last_seen_time = std::time(nullptr);
}

Node::Node(NodeDevMgr* node_mgr, Bus* bus, std::uint16_t node_id, std::uint16_t node_type, std::uint64_t node_version) noexcept:
    RawNode(node_mgr, bus, node_id),
    // Node(std::move(node)),
    _device_type(node_type),
    _device_version(node_version),
    _device_name("unknown"),
    _device_description(""),
    _created_time(std::time(nullptr)) {
    logger = spdlog::get(H9dConfigurator::nodes_logger_name);

    SPDLOG_LOGGER_INFO(logger, "Create device descriptor: id: {} type: {} version: {}.{}.{}.", node_id, node_type,
                       device_version_major(), device_version_minor(), device_version_patch());

    _last_seen_time = _created_time;

    if (nodedescloader.get_node_name_by_type(node_type) != "") {
        _device_name = nodedescloader.get_node_name_by_type(node_type);
        _device_description = nodedescloader.get_node_description_by_type(node_type);
    }

    register_map[1] = {1, "Node type", "uint", 16, true, false, {}, ""};
    register_map[2] = {2, "Node version", "uint", 48, true, false, {}, ""};
    register_map[3] = {3, "Build metadata", "str", 48, true, false, {}, ""};
    register_map[4] = {4, "Node id", "uint", 9, true, true, {}, ""};
    register_map[5] = {5, "MCU type", "uint", 8, true, false, {}, ""};

    for (const auto& it : nodedescloader.get_node_register_by_type(node_type)) {
        register_map[it.first] = {it.second.number, it.second.name, it.second.type, it.second.size, it.second.readable, it.second.writable, it.second.bits_names, it.second.description};
    }
}

Node::~Node() {
    detach();

    SPDLOG_LOGGER_TRACE(logger, "~Node() {}", fmt::ptr(this));
}

std::vector<Node::RegisterDsc> Node::get_registers_list() noexcept {
    std::vector<Node::RegisterDsc> ret;
    for (const auto& reg : register_map) {
        ret.push_back(reg.second);
    }
    return ret;
}

std::uint16_t Node::device_type() const noexcept {
    return _device_type;
}

std::uint64_t Node::device_version() const noexcept {
    return _device_version;
}

std::uint16_t Node::device_version_major() const noexcept {
    return static_cast<std::uint16_t>(_device_version >> 32);
}

std::uint16_t Node::device_version_minor() const noexcept {
    return static_cast<std::uint16_t>(_device_version >> 16);
}

std::uint16_t Node::device_version_patch() const noexcept {
    return static_cast<std::uint16_t>(_device_version);
}

std::string Node::device_name() const noexcept {
    return _device_name;
}

std::time_t Node::device_created_time() const noexcept {
    return _created_time;
}

std::time_t Node::device_last_seen_time() const noexcept {
    return _last_seen_time;
}

std::string Node::device_description() const noexcept {
    return _device_description;
}

void Node::node_reset() {
    ssize_t ret;
    if ((ret = reset("h9d")) < 0) {
        if (ret == RawNode::TIMEOUT_ERROR)
            throw TimeoutException();
        else if (ret == RawNode::MALFORMED_FRAME_ERROR)
            throw MalformedFrameException();
        else
            throw NodeException(-ret);
    }
}

Node::regvalue_t Node::set_register(std::uint8_t reg, Node::regvalue_t value) {
    if (register_map.count(reg)) {
        if (register_map[reg].writable) {
            if (std::holds_alternative<std::int64_t>(value) && register_map[reg].type != "str") {
                auto v = std::get<std::int64_t>(value);
                if (register_map[reg].size <= 8) {
                    std::uint8_t tmp = v & ((1 << register_map[reg].size) - 1);
                    std::uint8_t val;
                    ssize_t ret;
                    if ((ret = set_reg("h9d", reg, tmp, &val)) < 0) {
                        if (ret == RawNode::TIMEOUT_ERROR)
                            throw TimeoutException();
                        else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                            throw MalformedFrameException();
                        else
                            throw NodeException(-ret);
                    }
                    else if (ret != sizeof(val)) {
                        throw SizeMismatchException();
                    }
                    return {val};
                }
                else if (register_map[reg].size <= 16) {
                    std::uint16_t tmp = v & ((1 << register_map[reg].size) - 1);
                    std::uint16_t val;
                    ssize_t ret;
                    if ((ret = set_reg("h9d", reg, tmp, &val)) < 0) {
                        if (ret == RawNode::TIMEOUT_ERROR)
                            throw TimeoutException();
                        else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                            throw MalformedFrameException();
                        else
                            throw NodeException(-ret);
                    }
                    else if (ret != sizeof(val)) {
                        throw SizeMismatchException();
                    }
                    return {val};
                }
                else if (register_map[reg].size <= 32) {
                    std::uint32_t tmp = v & ((1 << register_map[reg].size) - 1);
                    std::uint32_t val;
                    ssize_t ret;
                    if ((ret = set_reg("h9d", reg, tmp, &val)) < 0) {
                        if (ret == RawNode::TIMEOUT_ERROR)
                            throw TimeoutException();
                        else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                            throw MalformedFrameException();
                        else
                            throw NodeException(-ret);
                    }
                    else if (ret != sizeof(val)) {
                        throw SizeMismatchException();
                    }
                    return {val};
                }
            }
            else if (std::holds_alternative<std::string>(value) && register_map[reg].type == "str") {
                auto v = std::get<std::string>(value);
                size_t len = register_map[reg].size / 8;
                len = len < v.size() ? len : v.size();

                ssize_t ret_len = (register_map[reg].size + 7) / 8 + 1;
                auto* ret_buf = new std::uint8_t[ret_len];

                ssize_t ret;
                if ((ret = set_reg("h9d", reg, len, reinterpret_cast<const std::uint8_t*>(v.c_str()), ret_buf, ret_len)) < 0) {
                    if (ret == RawNode::TIMEOUT_ERROR)
                        throw TimeoutException();
                    else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                        throw MalformedFrameException();
                    else
                        throw NodeException(-ret);
                }

                ret_buf[ret] = '\0';
                std::string ret_str = {reinterpret_cast<char*>(ret_buf)};
                delete[] ret_buf;

                return {ret_str};
            }
            else if (std::holds_alternative<std::vector<std::uint8_t>>(value)) {
                auto v = std::get<std::vector<std::uint8_t>>(value);
                size_t len = (register_map[reg].size + 7) / 8;
                if (len == v.size()) {
                    auto* ret_buf = new std::uint8_t[len];

                    ssize_t ret;
                    if ((ret = set_reg("h9d", reg, len, v.data(), ret_buf)) < 0) {
                        delete[] ret_buf;
                        if (ret == RawNode::TIMEOUT_ERROR)
                            throw TimeoutException();
                        else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                            throw MalformedFrameException();
                        else
                            throw NodeException(-ret);
                    }
                    else if (ret != len) {
                        throw SizeMismatchException();
                    }

                    Node::regvalue_t ret_v = {std::vector<std::uint8_t>(ret_buf, ret_buf + ret)};
                    delete[] ret_buf;
                    return std::move(ret_v);
                }
            }
            throw UnsupportedRegisterDataConversionException(reg);
        }
        throw RegisterNotWritableException(reg);
    }
    else {
        throw RegisterNotExistException(reg);
    }
}

Node::regvalue_t Node::get_register(std::uint8_t reg) {
    if (register_map.count(reg)) {
        if (register_map[reg].readable) {
            if (register_map[reg].type != "str") {
                if (register_map[reg].size <= 8) {
                    std::uint8_t val;
                    ssize_t ret;
                    if ((ret = get_reg("h9d", reg, &val)) < 0) {
                        if (ret == RawNode::TIMEOUT_ERROR)
                            throw TimeoutException();
                        else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                            throw MalformedFrameException();
                        else
                            throw NodeException(-ret);
                    }
                    else if (ret != sizeof(val)) {
                        throw SizeMismatchException();
                    }
                    return {val};
                }
                else if (register_map[reg].size <= 16) {
                    std::uint16_t val;
                    ssize_t ret;
                    if ((ret = get_reg("h9d", reg, &val)) < 0) {
                        if (ret == RawNode::TIMEOUT_ERROR)
                            throw TimeoutException();
                        else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                            throw MalformedFrameException();
                        else
                            throw NodeException(-ret);
                    }
                    else if (ret != sizeof(val)) {
                        throw SizeMismatchException();
                    }
                    return {val};
                }
                else if (register_map[reg].size <= 32) {
                    std::uint32_t val;
                    ssize_t ret;
                    if ((ret = get_reg("h9d", reg, &val)) < 0) {
                        if (ret == RawNode::TIMEOUT_ERROR)
                            throw TimeoutException();
                        else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                            throw MalformedFrameException();
                        else
                            throw NodeException(-ret);
                    }
                    else if (ret != sizeof(val)) {
                        throw SizeMismatchException();
                    }
                    return {val};
                }
                else {
                    size_t len = (register_map[reg].size + 7) / 8;
                    auto* buf = new std::uint8_t[len];

                    ssize_t ret;
                    if ((ret = get_reg("h9d", reg, len, buf)) < 0) {
                        delete[] buf;
                        if (ret == RawNode::TIMEOUT_ERROR)
                            throw TimeoutException();
                        else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                            throw MalformedFrameException();
                        else
                            throw NodeException(-ret);
                    }
                    else if (ret != len) {
                        throw SizeMismatchException();
                    }
                    Node::regvalue_t ret_v = {std::vector<std::uint8_t>(buf, buf + ret)};
                    delete[] buf;
                    return std::move(ret_v);
                }
            }
            else if (register_map[reg].type == "str") {
                size_t len = (register_map[reg].size + 7) / 8 + 1;
                auto* buf = new std::uint8_t[len];
                ssize_t ret;
                if ((ret = get_reg("h9d", reg, len - 1, buf)) < 0) {
                    delete[] buf;
                    if (ret == RawNode::TIMEOUT_ERROR)
                        throw TimeoutException();
                    else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                        throw MalformedFrameException();
                    else
                        throw NodeException(-ret);
                }
                buf[ret] = '\0';
                std::string ret_str = {reinterpret_cast<char*>(buf)};
                delete[] buf;
                return std::move(ret_str);
            }
            throw UnsupportedRegisterDataConversionException(reg);
        }
        throw RegisterNotReadableException(reg);
    }
    else {
        throw RegisterNotExistException(reg);
    }
}

Node::regvalue_t Node::set_register_bit(std::uint8_t reg, std::uint8_t bit_num) {
    if (register_map.count(reg)) {
        if (register_map[reg].writable) {
            if (register_map[reg].type != "str") {
                size_t result_len = (register_map[reg].size + 7) / 8;
                auto* result_buf = new std::uint8_t[result_len];

                ssize_t ret;
                if ((ret = set_bit("h9d", reg, bit_num, result_len, result_buf)) < 0) {
                    delete[] result_buf;
                    if (ret == RawNode::TIMEOUT_ERROR)
                        throw TimeoutException();
                    else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                        throw MalformedFrameException();
                    else
                        throw NodeException(-ret);
                }

                Node::regvalue_t ret_v = {std::vector<std::uint8_t>(result_buf, result_buf + ret)};
                delete[] result_buf;
                return std::move(ret_v);
            }
            throw UnsupportedRegisterDataConversionException(reg);
        }
        throw RegisterNotWritableException(reg);
    }
    else {
        throw RegisterNotExistException(reg);
    }
}

Node::regvalue_t Node::clear_register_bit(std::uint8_t reg, std::uint8_t bit_num) {
    if (register_map.count(reg)) {
        if (register_map[reg].writable) {
            if (register_map[reg].type != "str") {
                size_t result_len = (register_map[reg].size + 7) / 8;
                auto* result_buf = new std::uint8_t[result_len];

                ssize_t ret;
                if ((ret = clear_bit("h9d", reg, bit_num, result_len, result_buf)) < 0) {
                    delete[] result_buf;
                    if (ret == RawNode::TIMEOUT_ERROR)
                        throw TimeoutException();
                    else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                        throw MalformedFrameException();
                    else
                        throw NodeException(-ret);
                }

                Node::regvalue_t ret_v = {std::vector<std::uint8_t>(result_buf, result_buf + ret)};
                delete[] result_buf;
                return std::move(ret_v);
            }
            throw UnsupportedRegisterDataConversionException(reg);
        }
        throw RegisterNotWritableException(reg);
    }
    else {
        throw RegisterNotExistException(reg);
    }
}

Node::regvalue_t Node::toggle_register_bit(std::uint8_t reg, std::uint8_t bit_num) {
    if (register_map.count(reg)) {
        if (register_map[reg].writable) {
            if (register_map[reg].type != "str") {
                size_t result_len = (register_map[reg].size + 7) / 8;
                auto* result_buf = new std::uint8_t[result_len];

                ssize_t ret;
                if ((ret = toggle_bit("h9d", reg, bit_num, result_len, result_buf)) < 0) {
                    delete[] result_buf;
                    if (ret == RawNode::TIMEOUT_ERROR)
                        throw TimeoutException();
                    else if (ret == RawNode::MALFORMED_FRAME_ERROR)
                        throw MalformedFrameException();
                    else
                        throw NodeException(-ret);
                }

                Node::regvalue_t ret_v = {std::vector<std::uint8_t>(result_buf, result_buf + ret)};
                delete[] result_buf;
                return std::move(ret_v);
            }
            throw UnsupportedRegisterDataConversionException(reg);
        }
        throw RegisterNotWritableException(reg);
    }
    else {
        throw RegisterNotExistException(reg);
    }
}

std::uint8_t Node::get_reg_value_from_frame(const ExtH9Frame& frame, regvalue_t* value) {
    if (frame.type() == H9frame::Type::REG_VALUE ||
         frame.type() == H9frame::Type::REG_EXTERNALLY_CHANGED ||
         frame.type() == H9frame::Type::REG_INTERNALLY_CHANGED ||
         frame.type() == H9frame::Type::REG_VALUE_BROADCAST) {

        if (frame.dlc() < 2)
            throw MalformedFrameException();

        std::uint8_t reg = frame.data()[0];

        if (register_map.count(reg) == 0)
            throw RegisterNotExistException(reg);

        if (register_map[reg].type != "str") {
            if (register_map[reg].size <= 8) {
                std::uint8_t val = frame.data()[1];
                if (frame.dlc() - 1 != sizeof(val)) {
                    throw SizeMismatchException();
                }
                *value = {val};
                return reg;
            }
            else if (register_map[reg].size <= 16) {
                std::uint16_t val;
                std::memcpy(&val, &frame.data()[1], sizeof(val));

                if (frame.dlc() - 1 != sizeof(val)) {
                    throw SizeMismatchException();
                }
                *value = { ntohs(val) };
                return reg;
            }
            else if (register_map[reg].size <= 32) {
                std::uint32_t val;
                std::memcpy(&val, &frame.data()[1], sizeof(val));

                if (frame.dlc() - 1 != sizeof(val)) {
                    throw SizeMismatchException();
                }
                *value = { ntohl(val) };
                return reg;
            }
            else {
                size_t len = (register_map[reg].size + 7) / 8;

                std::size_t frame_reg_size = frame.dlc()-1;

                if (frame_reg_size > len) {
                    throw SizeMismatchException();
                }

                Node::regvalue_t ret_v = {std::vector<std::uint8_t>(&frame.data()[1], &frame.data()[1] + frame_reg_size)};
                *value = { ret_v };
                return reg;
            }
        }
        else if (register_map[reg].type == "str") {
            size_t len = (register_map[reg].size + 7) / 8;

            std::size_t frame_reg_size = frame.dlc()-1;
            std::string tmp = {reinterpret_cast<const char*>(&frame.data()[1]), std::min(frame_reg_size, std::strlen(reinterpret_cast<const char*>(&frame.data()[1])))};

            if (frame_reg_size > len) {
                throw SizeMismatchException();
            }
            *value = { tmp };
            return reg;
        }
        throw UnsupportedRegisterDataConversionException(reg);
    }
    return 0;
}
