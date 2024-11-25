/*
 * H9 project
 *
 * Created by SQ8KFH on 2018-07-23.
 *
 * Copyright (C) 2018-2020 Kamil Palkowski. All rights reserved.
 */

#pragma once

#include "bus_driver.h"
#include <queue>

class SlcanDriver: public BusDriver {
  private:
    const std::string _tty;
    const std::string _init_string;
    bool noblock;
    std::string recv_buf;

    std::queue<H9frame> recv_queue;

    std::queue<std::shared_ptr<BusFrame>> last_send;

  public:
    SlcanDriver(const std::string& name, const std::string& tty, const std::string& init_string);
    int open() override;

    static std::string build_slcan_msg(const H9frame& frame);
    static bool parse_slcan_msg(const std::string& slcan_data, H9frame* frame);

  private:
    int recv_data(H9frame* frame) override;
    int send_data(std::shared_ptr<BusFrame> busframe) override;
    void parse_buf();
    void send_ack();
};
