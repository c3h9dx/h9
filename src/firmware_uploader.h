/*
 * H9 project
 *
 * Created by crowx on 2023-10-19.
 *
 * Copyright (C) 2023 Kamil Palkowski. All rights reserved.
 */
#pragma once

#include <string>
#include "ext_h9frame.h"
class FirmwareUploader {
  private:
    const std::uint8_t* firmware;
    const std::size_t fw_size;
    const std::uint16_t node_id;
  public:
    uint32_t recv_frame_count;
    uint32_t sent_frame_count;

    constexpr static const char* mcu_map[] = {"UNKNOWN",      // 0
                                              "ATmega16M1",   // 1
                                              "ATmega32M1",   // 2
                                              "ATmega64M1",   // 3
                                              "AT90CAN128",   // 4
                                              "PIC18F46K80"}; // 5

    class BusProxy {
      public:
        virtual ExtH9Frame get_frame() = 0;
        virtual void put_frame(ExtH9Frame frame) = 0;
    };

    FirmwareUploader(std::uint8_t* firmware, std::size_t fw_size, std::uint16_t dst);
    void upload(BusProxy* bus);
};
