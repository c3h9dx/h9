/*
 * H9 project
 *
 * Created by crowx on 2023-09-07.
 *
 * Copyright (C) 2023 Kamil Palkowski. All rights reserved.
 */

#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>

#include "types.h"
#include "h9frame.h"

class ExtH9Frame {
  private:
    H9frame _frame;
    std::string _origin;

    unsigned int valid;

    timestamp_t _creation_timestamp;
  public:
    constexpr static unsigned int VALID_ORIGIN = 1 << 0;
    constexpr static unsigned int VALID_PRIORITY = 1 << 1;
    constexpr static unsigned int VALID_TYPE = 1 << 2;
    constexpr static unsigned int VALID_SEQNUM = 1 << 3;
    constexpr static unsigned int VALID_DESTINATION_ID = 1 << 4;
    constexpr static unsigned int VALID_SOURCE_ID = 1 << 5;
    constexpr static unsigned int VALID_DLC = 1 << 6;
    constexpr static unsigned int VALID_DATA = 1 << 7;

    constexpr static unsigned int VALID_ALL = (VALID_ORIGIN | VALID_PRIORITY | VALID_TYPE | VALID_SEQNUM | VALID_DESTINATION_ID | VALID_SOURCE_ID | VALID_DLC | VALID_DATA);
    constexpr static unsigned int VALID_UNUSED = ~VALID_ALL;

    ExtH9Frame();
    ExtH9Frame(const H9frame& frame, const std::string& origin);
    ExtH9Frame(const std::string& origin, H9frame::Type type, std::uint16_t dst, std::uint8_t dlc = 0, const std::vector<std::uint8_t>& data = {});
    ExtH9Frame(const std::string& origin, H9frame::Priority priority, H9frame::Type type, std::uint16_t dst, std::uint8_t dlc = 0, const std::vector<std::uint8_t>& data = {});

    unsigned int valid_member();
    unsigned int invalid_member();

    timestamp_t creation_timestamp() const { return _creation_timestamp; }

    const H9frame& frame() const { return _frame; }

    const std::string& origin() const { return _origin; }

    H9frame::Priority priority() const { return _frame.priority; }

    H9frame::Type type() const { return _frame.type; }

    std::uint8_t seqnum() const { return _frame.seqnum; }

    std::uint16_t destination_id() const { return _frame.destination_id; }

    std::uint16_t source_id() const { return _frame.source_id; }

    std::uint8_t dlc() const { return _frame.dlc; }

    const std::uint8_t* data() const { return _frame.data; };

    void origin(const std::string& origin);
    void priority(H9frame::Priority priority);
    void type(H9frame::Type type);
    void type(std::uint8_t type);
    void seqnum(std::uint8_t seqnum);
    void destination_id(std::uint16_t destination_id);
    void source_id(std::uint16_t source_id);
    void dlc(std::uint8_t dlc);
    void data(const std::vector<std::uint8_t>& data);
};

void to_json(nlohmann::json& j, const ExtH9Frame& f);
void from_json(const nlohmann::json& j, ExtH9Frame& f);
