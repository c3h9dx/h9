/*
 * H9 project
 *
 * Created by crowx on 2023-09-07.
 *
 * Copyright (C) 2023 Kamil Palkowski. All rights reserved.
 */

#include "ext_h9frame.h"

ExtH9Frame::ExtH9Frame():
    _creation_timestamp(timestamp_t::clock::now()) {
}

ExtH9Frame::ExtH9Frame(const H9frame& frame, const std::string& origin):
    _frame(frame),
    _origin(origin),
    _creation_timestamp(timestamp_t::clock::now()) {
    valid = (VALID_PRIORITY | VALID_TYPE | VALID_SEQNUM | VALID_DESTINATION_ID | VALID_DATA);
    if (origin != "")
        valid |= VALID_ORIGIN;
    if (_frame.source_id <= H9frame::H9FRAME_SOURCE_ID_MAX_VALUE)
        valid |= VALID_SOURCE_ID;
    if (_frame.dlc <= H9frame::H9FRAME_DATA_LENGTH)
        valid |= VALID_DLC;
}

ExtH9Frame::ExtH9Frame(const std::string& origin, H9frame::Type type, std::uint16_t dst, std::uint8_t dlc, const std::vector<std::uint8_t>& data):
    ExtH9Frame(origin, H9frame::Priority::LOW, type, dst, dlc, data) {
}

ExtH9Frame::ExtH9Frame(const std::string& origin, H9frame::Priority priority, H9frame::Type type, std::uint16_t dst, std::uint8_t dlc, const std::vector<std::uint8_t>& data):
    _creation_timestamp(timestamp_t::clock::now()) {
    valid = 0;
    this->origin(origin);
    this->priority(priority);
    this->type(type);
    this->destination_id(dst);
    this->dlc(dlc);
    this->data(data);
}

unsigned int ExtH9Frame::valid_member() {
    return (valid & (VALID_ORIGIN | VALID_PRIORITY | VALID_TYPE | VALID_SEQNUM | VALID_DESTINATION_ID | VALID_SOURCE_ID | VALID_DLC | VALID_DATA)) | VALID_UNUSED;
}

unsigned int ExtH9Frame::invalid_member() {
    return ~valid & (VALID_ORIGIN | VALID_PRIORITY | VALID_TYPE | VALID_SEQNUM | VALID_DESTINATION_ID | VALID_SOURCE_ID | VALID_DLC | VALID_DATA);
}

void ExtH9Frame::origin(const std::string& origin) {
    _origin = origin;
    if (_origin != "")
        valid |= VALID_ORIGIN;
}

void ExtH9Frame::priority(H9frame::Priority priority) {
    _frame.priority = priority;
    valid |= VALID_PRIORITY;
}

void ExtH9Frame::type(H9frame::Type type) {
    _frame.type = type;
    valid |= VALID_TYPE;
}

void ExtH9Frame::type(std::uint8_t type) {
    if (type <= H9frame::H9FRAME_TYPE_MAX_VALUE) {
        _frame.set_type_from_underlying(type);
        valid |= VALID_TYPE;
    }
}

void ExtH9Frame::seqnum(std::uint8_t seqnum) {
    if (seqnum <= H9frame::H9FRAME_SEQNUM_MAX_VALUE) {
        _frame.seqnum = seqnum;
        valid |= VALID_SEQNUM;
    }
}

void ExtH9Frame::destination_id(std::uint16_t destination_id) {
    if (destination_id <= H9frame::H9FRAME_DESTINATION_ID_MAX_VALUE) {
        _frame.destination_id = destination_id;
        valid |= VALID_DESTINATION_ID;
    }
}

void ExtH9Frame::source_id(std::uint16_t source_id) {
    if (source_id <= H9frame::H9FRAME_SOURCE_ID_MAX_VALUE) {
        _frame.source_id = source_id;
        valid |= VALID_SOURCE_ID;
    }
}

void ExtH9Frame::dlc(std::uint8_t dlc) {
    if (dlc <= H9frame::H9FRAME_DATA_LENGTH) {
        _frame.dlc = dlc;
        valid |= VALID_DLC;
    }
}

void ExtH9Frame::data(const std::vector<std::uint8_t>& data) {
    int i = 0;
    for (auto& d : data) {
        _frame.data[i] = d;
        ++i;
        if (i > 8)
            break;
    }
    valid |= VALID_DATA;
}

void to_json(nlohmann::json& j, const ExtH9Frame& f) {
    std::vector<std::uint8_t> data(f.data(), f.data() + f.dlc());
    j = nlohmann::json{{"origin", f.origin()},
                       {"priority", f.priority() == H9frame::Priority::HIGH ? "H" : "L"},
                       {"type", H9frame::to_underlying(f.type())},
                       {"seqnum", f.seqnum()},
                       {"destination_id", f.destination_id()},
                       {"source_id", f.source_id()},
                       {"dlc", f.dlc()},
                       {"data", data}};
}

void from_json(const nlohmann::json& j, ExtH9Frame& f) {
    if (j.count("origin"))
        f.origin(j.at("origin").get<std::string>());
    else
        f.origin("");

    if (j.count("priority") && j.at("priority").get<std::string>() == "H") {
        f.priority(H9frame::Priority::HIGH);
    }
    else {
        f.priority(H9frame::Priority::LOW);
    }
    if (j.count("type"))
        f.type(j.at("type").get<std::uint8_t>());
    if (j.count("seqnum"))
        f.seqnum(j.at("seqnum").get<std::uint8_t>());
    if (j.count("destination_id"))
        f.destination_id(j.at("destination_id").get<std::uint16_t>());
    if (j.count("source_id"))
        f.source_id(j.at("source_id").get<std::uint16_t>());
    if (j.count("dlc"))
        f.dlc(j.at("dlc").get<std::uint8_t>());
    if (j.count("data"))
        f.data(j.at("data").get<std::vector<std::uint8_t>>());
}
