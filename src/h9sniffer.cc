/*
 * H9 project
 *
 * Created by SQ8KFH on 2019-04-09.
 *
 * Copyright (C) 2019-2023 Kamil Palkowski. All rights reserved.
 */

#include "config.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>

#include "ext_h9frame.h"
#include "h9_configurator.h"
#include "h9connector.h"

namespace {

class H9SnifferConfigurator: public H9Configurator {
  private:
    void add_app_specific_opt() {
        // clang-format off
        options.add_options("")
                ("e,extended", "Extended output")
                ("s,simple", "Simple output")
                ;
        // clang-format on
    }

    void parse_app_specific_opt(const cxxopts::ParseResult& result) {
        extended = result.count("extended");
        simple = result.count("simple");
    }

  public:
    bool extended;
    bool simple;

    H9SnifferConfigurator():
        H9Configurator("h9sniffer", "The H9 bus packets sniffer.") {}
};

} // namespace

void print_reg_value(const H9frame& frame) {
    if (frame.dlc > 1) {
        std::cout << "    value: ";
        switch (frame.dlc) {
        case 2:
            std::cout << static_cast<int>(frame.data[1]);
            break;
        case 3:
            std::cout << static_cast<int>((frame.data[1] << 8) | frame.data[2]);
            break;
        case 5:
            std::cout << static_cast<int>((frame.data[1] << 24) | (frame.data[2] << 16) | (frame.data[3] << 8) | frame.data[4]);
            break;
        }
        char buf[8] = {'\0'};
        for (int i = 0; i < frame.dlc-1; ++i) {
            if (isprint(frame.data[i+1])) {
                buf[i] = frame.data[i+1];
            }
            else {
                break;
            }
        }
        std::cout << " '" << buf << "'" << std::endl;
    }
}

void print_frame(const H9frame& frame) {
    std::cout << "    type name: " << H9frame::type_to_string(frame.type) << std::endl;
    if (frame.destination_id == H9frame::BROADCAST_ID)
        std::cout << "    destination: BROADCAST\n";
    if (frame.type == H9frame::Type::REG_EXTERNALLY_CHANGED ||
        frame.type == H9frame::Type::REG_INTERNALLY_CHANGED ||
        frame.type == H9frame::Type::REG_VALUE_BROADCAST ||
        frame.type == H9frame::Type::REG_VALUE ||
        frame.type == H9frame::Type::SET_REG ||
        frame.type == H9frame::Type::GET_REG) {

        std::cout << "    reg: " << static_cast<unsigned int>(frame.data[0]) << std::endl;
        print_reg_value(frame);
    }
    else if (frame.type == H9frame::Type::SET_BIT) {
        std::cout << "    reg: " << static_cast<unsigned int>(frame.data[0]) << std::endl;
        std::cout << "    set bit: " << static_cast<unsigned int>(frame.data[1]) << std::endl;
    }
    else if (frame.type == H9frame::Type::CLEAR_BIT) {
        std::cout << "    reg: " << static_cast<unsigned int>(frame.data[0]) << std::endl;
        std::cout << "    clear bit: " << static_cast<unsigned int>(frame.data[1]) << std::endl;
    }
    else if (frame.type == H9frame::Type::TOGGLE_BIT) {
        std::cout << "    reg: " << static_cast<unsigned int>(frame.data[0]) << std::endl;
        std::cout << "    toggle bit: " << static_cast<unsigned int>(frame.data[1]) << std::endl;
    }
    else if (frame.type == H9frame::Type::NODE_TURNED_ON || frame.type == H9frame::Type::NODE_INFO) {
        std::cout << "    node type: " << static_cast<unsigned int>(frame.data[0] << 8 | frame.data[1]) << std::endl;
        std::cout << "    node firmware: " << static_cast<unsigned int>(frame.data[2] << 8 | frame.data[3])
                  << '.' << static_cast<unsigned int>(frame.data[4] << 8 | frame.data[5])
                  << '.' << static_cast<unsigned int>(frame.data[6] << 8 | frame.data[7]) << std::endl;
    }
    else if (frame.type == H9frame::Type::BOOTLOADER_TURNED_ON) {
        std::cout << "    bootloader version: " << static_cast<unsigned int>(frame.data[0]) << '.' << static_cast<unsigned int>(frame.data[1]) << std::endl;
        std::cout << "    node MCU type: " << static_cast<unsigned int>(frame.data[2]) << std::endl;
        std::cout << "    node type: " << static_cast<unsigned int>(frame.data[3] << 8 | frame.data[4]) << std::endl;
    }
    else if (frame.type == H9frame::Type::ERROR) {
        int err_num = static_cast<int>(frame.data[0]);
        std::cout << "    error: " << err_num << " - " << H9frame::error_to_string(H9frame::from_underlying<H9frame::Error>(err_num)) << std::endl;
    }
}

int main(int argc, char** argv) {
    H9SnifferConfigurator h9;
    h9.logger_initial_setup();
    h9.parse_command_line_arg(argc, argv);
    h9.logger_setup();
    h9.load_configuration();

    H9Connector h9_connector = h9.get_connector();

    try {
        h9_connector.connect("h9sniffer");
    }
    catch (std::system_error& e) {
        SPDLOG_ERROR("Can not connect to h9bus {}:{}: {}.", h9.get_host(), h9.get_port(), e.code().message());
        exit(EXIT_FAILURE);
    }
    catch (std::runtime_error& e) {
        SPDLOG_ERROR("Can not connect to h9bus {}:{}: {}.", h9.get_host(), h9.get_port(), e.what());
        exit(EXIT_FAILURE);
    }

    jsonrpcpp::Id id(h9_connector.get_next_id());

    jsonrpcpp::Request req(id, "subscribe", nlohmann::json({{"event", "frame"}}));
    h9_connector.send(std::make_shared<jsonrpcpp::Request>(req));

    // std::cout << r.to_json().dump() << std::endl;
    int output = 1;

    if (h9.simple && !h9.extended) {
        output = 0;
        std::cout << "source_id,destination_id,priority,type,seqnum,dlc,data\n";
    }
    if (h9.extended && !h9.simple) {
        output = 2;
    }

    while (true) {
        jsonrpcpp::entity_ptr raw_msg;
        try {
            raw_msg = h9_connector.recv();
        }
        catch (std::system_error& e) {
            SPDLOG_ERROR("Messages receiving error: {}.", e.code().message());
            exit(EXIT_FAILURE);
        }
        catch (std::runtime_error& e) {
            SPDLOG_ERROR("Messages receiving error: {}.", e.what());
            exit(EXIT_FAILURE);
        }

        if (raw_msg->is_notification()) {
            jsonrpcpp::notification_ptr notification = std::dynamic_pointer_cast<jsonrpcpp::Notification>(raw_msg);
            if (notification->method() == "on_frame") {
                ExtH9Frame frame = std::move(notification->params().param_map.at("frame").get<ExtH9Frame>());

                if (output == 0) {
                    std::cout << frame.source_id() << ","
                              << frame.destination_id() << ","
                              << (frame.priority() == H9frame::Priority::HIGH ? 'H' : 'L') << ","
                              << static_cast<unsigned int>(H9frame::to_underlying(frame.type())) << ','
                              << static_cast<unsigned int>(frame.seqnum()) << ","
                              << static_cast<unsigned int>(frame.dlc()) << ",";
                    std::ios oldState(nullptr);
                    oldState.copyfmt(std::cout);

                    for (int i = 0; i < frame.dlc(); ++i) {
                        std::cout << std::setfill('0') << std::hex << std::setw(2) << static_cast<unsigned int>(frame.data()[i]);
                    }
                    std::cout.copyfmt(oldState);
                    std::cout << std::endl;
                }
                else {
                    std::cout << frame.frame() << std::endl;
                    if (output == 2) {
                        print_frame(frame.frame());
                    }
                }
            }
            //std::cout << notification->to_json() << std::endl;
        }
        else {
            // h9_log_info(" recv msg: [%s]", raw_msg.serialize().c_str());
        }
    }
    return EXIT_SUCCESS;
}
