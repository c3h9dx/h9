/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-23.
 *
 * Copyright (C) 2020-2024 Kamil Palkowski. All rights reserved.
 */

#include "node.h"

#include <cassert>

#include <h9def.h>

#include "dev_node_exception.h"
#include "h9d_configurator.h"
#include "tcpclientthread.h"
#include "dev.h"

NodeDescLoader Node::nodedescloader;

void Node::update_node_last_seen_time(timestamp_t timestamp) noexcept {
    _last_seen_time = timestamp;
}

Node::Node(NodeMgr* node_mgr, Bus* bus, std::uint16_t node_id) noexcept:
    RawNode(node_mgr, bus, node_id),
    _node_type(0),
    _node_version(0),
    _node_name("unknown"),
    _node_description(""),
    _created_time(timestamp_t::clock::now()),
    _init(false) {
    logger = spdlog::get(H9dConfigurator::nodes_logger_name);

    SPDLOG_LOGGER_INFO(logger, "Create node descriptor: id: {}.", node_id);

    //_last_seen_time = _created_time;
}

void Node::init(std::uint16_t node_type, std::uint32_t node_version, char hardware_revision, std::uint8_t reset_reason) {
    _reset_reason = reset_reason;

    _init = true;

    if (_node_type == node_type && _node_version == node_version && _hardware_revision == hardware_revision)
        return;

    _node_version = node_version;
    _hardware_revision = hardware_revision;
    _reset_reason = reset_reason;

    if (_node_type == 0) {
        SPDLOG_LOGGER_INFO(logger, "Init node id: {} type: {} version: {}.{}{}.", _node_id, node_type, node_version_major(), node_version_minor(), _hardware_revision);
    }
    else if (_node_type != node_type) {
        SPDLOG_LOGGER_WARN(logger, "Reinit node id: {} type: {} -> {} version: {}.{}{}.", _node_id, _node_type, node_type, node_version_major(), node_version_minor(), _hardware_revision);
        register_map.clear();
    }
    else {
        SPDLOG_LOGGER_INFO(logger, "Change version node id: {} type: {} version: {}.{}{}.", _node_id, node_type, node_version_major(), node_version_minor(), _hardware_revision);
    }

    _node_type = node_type;

    load_description();
}

void Node::load_description() {
    if (nodedescloader.get_node_name_by_type(_node_type) != "") {
        _node_name = nodedescloader.get_node_name_by_type(_node_type);
        _node_description = nodedescloader.get_node_description_by_type(_node_type);
    }

    register_map.clear();

    register_map[NODE_TYPE_STD_REGISTER] = {NODE_TYPE_STD_REGISTER, "Node type", "uint", 16, true, false, {}, ""};
    register_map[NODE_HARDWARE_REVISION_STD_REGISTER] = {NODE_HARDWARE_REVISION_STD_REGISTER, "Node hardware revision", "char", 8, true, false, {}, ""};
    register_map[NODE_VERSION_STD_REGISTER] = {NODE_VERSION_STD_REGISTER, "Node version", "uint", 32, true, false, {}, ""};
    register_map[NODE_BUILD_INFO_STD_REGISTER] = {NODE_BUILD_INFO_STD_REGISTER, "Build metadata", "str", 48, true, false, {}, ""};
    register_map[NODE_ID_STD_REGISTER] = {NODE_ID_STD_REGISTER, "Node id", "uint", 9, true, true, {}, ""};
    register_map[NODE_MCU_TYPE_STD_REGISTER] = {NODE_MCU_TYPE_STD_REGISTER, "MCU type", "uint", 8, true, false, {}, ""};
    register_map[NODE_RESET_REASON_STD_REGISTER] = {NODE_RESET_REASON_STD_REGISTER, "Node reset reason", "uint", 8, true, false, {}, ""};

    for (const auto& it : nodedescloader.get_node_register_by_type(_node_type)) {
        register_map[it.first] = {it.second.number, it.second.name, it.second.type, it.second.size, it.second.readable, it.second.writable, it.second.bits_names, it.second.description};
    }
}

void Node::add_dependent_devices(Dev *dev) {
    dependent_devices_mtx.lock();
    dependent_devices.push_back(dev);
    dependent_devices_mtx.unlock();
}

void Node::del_dependent_devices(Dev *dev) {
    dependent_devices_mtx.lock();
    dependent_devices.remove(dev);
    dependent_devices_mtx.unlock();
}

void Node::on_frame_recv(const ExtH9Frame& frame) {
    RawNode::on_frame_recv(frame);

    dependent_devices_mtx.lock();
    for (auto d: dependent_devices) {
        node_mgr->dev_workers.update_dev_state(d, _node_id, frame);
    }
    dependent_devices_mtx.unlock();
}

Node::~Node() {
    SPDLOG_LOGGER_TRACE(logger, "~Node() {}", fmt::ptr(this));
}

std::vector<Node::RegisterDsc> Node::get_registers_list() noexcept {
    std::vector<Node::RegisterDsc> ret;
    for (const auto& reg : register_map) {
        ret.push_back(reg.second);
    }
    return ret;
}

std::uint16_t Node::node_type() const noexcept {
    return _node_type;
}

std::uint64_t Node::node_version() const noexcept {
    return _node_version;
}

std::uint16_t Node::node_version_major() const noexcept {
    return static_cast<std::uint16_t>(_node_version >> 16);
}

std::uint16_t Node::node_version_minor() const noexcept {
    return static_cast<std::uint16_t>(_node_version);
}

char Node::node_hardware_revision() const noexcept {
    return _hardware_revision;
}

std::uint8_t Node::node_reset_reason() const noexcept {
    return _reset_reason;
}

std::string Node::node_name() const noexcept {
    return _node_name;
}

timestamp_t Node::node_created_time() const noexcept {
    return _created_time;
}

timestamp_t Node::node_last_seen_time() const noexcept {
    return _last_seen_time;
}

std::string Node::node_description() const noexcept {
    return _node_description;
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
