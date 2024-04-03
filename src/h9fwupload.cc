/*
 * H9 project
 *
 * Created by crowx on 2023-10-19.
 *
 * Copyright (C) 2023 Kamil Palkowski. All rights reserved.
 */

#include <chrono>
#include <cstdlib>
#include <spdlog/spdlog.h>

#include "firmware_uploader.h"
#include "h9_configurator.h"

static std::uint8_t hex2bin(const char* hex);
static size_t read_ihex(const std::string& ihex_file, std::uint8_t** fw_data);

class H9FwuploadConfigurator: public H9Configurator {
  private:
    void add_app_specific_opt() {
        // clang-format off
        options.add_options("")
                ("s,src_id", "Source id, if set, a raw frame will be sent", cxxopts::value<std::uint16_t>())
                ("d,dst_id", "Destination id", cxxopts::value<std::uint16_t>())
                ("b,noupgrademsg", "Don't send the NODE_UPGRADE frame before")
                ;
        // clang-format on
        options.parse_positional("ihex");
        options.positional_help("<Intel HEX file>");

        options.add_options()("ihex", "<Intel HEX file>", cxxopts::value<std::string>());
    }

    void parse_app_specific_opt(const cxxopts::ParseResult& result) {
        if (result.count("src_id")) {
            src_id = result["src_id"].as<std::uint16_t>();
        }
        else {
            src_id = -1;
        }

        if (result.count("dst_id")) {
            dst_id = result["dst_id"].as<std::uint16_t>();
        }
        else {
            SPDLOG_ERROR("destination id must be set");
            exit(EXIT_FAILURE);
        }

        noupgrademsg = result.count("noupgrademsg");

        if (result.count("ihex")) {
            ihex_filename = result["ihex"].as<std::string>();
        }
        else {
            SPDLOG_ERROR("missing a Intel HEX file");
            exit(EXIT_FAILURE);
        }
    }

  public:
    bool noupgrademsg;
    std::uint16_t dst_id;
    std::int16_t src_id;
    std::string ihex_filename;

    H9FwuploadConfigurator():
        H9Configurator("h9fwupload", "Uploads a firmware to devices via the H9 bus.") {}
};

class BusOverH9TCP: public FirmwareUploader::BusProxy {
  private:
    bool raw;
    std::uint16_t source_id;
    std::uint8_t next_seqnum;
    H9Connector* connector;

  public:
    BusOverH9TCP(H9Connector* connector):
        raw(false),
        connector(connector) {}

    BusOverH9TCP(H9Connector* connector, std::uint16_t source_id, std::uint8_t next_seqnum):
        raw(true),
        connector(connector),
        source_id(source_id),
        next_seqnum(next_seqnum) {}

    virtual ExtH9Frame get_frame() {
        while (true) {
            jsonrpcpp::entity_ptr raw_msg;
            try {
                raw_msg = connector->recv();
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
                    return frame;
                }
            }
        }
    }

    virtual void put_frame(ExtH9Frame frame) {
        if (raw) {
            frame.source_id(source_id);
            frame.seqnum(next_seqnum++);
        }
        jsonrpcpp::Id id(connector->get_next_id());
        jsonrpcpp::Request rf(std::move(id), "send_frame", nlohmann::json({{"frame", frame}, {"raw", raw}}));

        connector->send(std::make_shared<jsonrpcpp::Request>(std::move(rf)));
    }
};

int main(int argc, char** argv) {
    H9FwuploadConfigurator h9;
    h9.logger_initial_setup();
    h9.parse_command_line_arg(argc, argv);
    h9.logger_setup();
    h9.load_configuration();

    std::uint8_t* fw = nullptr;
    size_t fw_size = 0;
    fw_size = read_ihex(h9.ihex_filename, &fw);

    H9Connector h9_connector = h9.get_connector();

    try {
        h9_connector.connect("h9fwupload");
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

    BusOverH9TCP* bus = nullptr;
    if (h9.src_id >= 0) {
        bus = new BusOverH9TCP(&h9_connector, h9.src_id, 0);
    }
    else {
        bus = new BusOverH9TCP(&h9_connector);
    }

    auto start_time = std::chrono::steady_clock::now();

    if (h9.noupgrademsg) { //skip NODE_UPGRADE frame
        ExtH9Frame frame;
        frame.type(H9frame::Type::PAGE_START);
        frame.destination_id(h9.dst_id);
        frame.dlc(2);
        std::uint16_t page = 0;
        frame.data({(uint8_t)((page >> 8) & 0xff), (uint8_t)((page) & 0xff)});
        bus->put_frame(frame);
    }
    else {
        ExtH9Frame frame;
        frame.type(H9frame::Type::NODE_UPGRADE);
        frame.destination_id(h9.dst_id);
        frame.dlc(0);
        bus->put_frame(frame);
    }

    FirmwareUploader fw_up = {fw, fw_size, h9.dst_id};
    fw_up.upload(bus);

    auto stop_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>( stop_time - start_time ).count();

    fmt::println("Frame recv: {} ({:.2f} f/s)", fw_up.recv_frame_count, (fw_up.recv_frame_count * 1000.0) / duration_ms);
    fmt::println("Frame sent: {} ({:.2f} f/s)", fw_up.sent_frame_count, (fw_up.sent_frame_count * 1000.0) / duration_ms);
    fmt::println("Time: {}", duration_ms);
    fmt::println("Speed: {:.2f} B/s", (fw_size * 1000.0) / duration_ms);

    delete bus;
    return EXIT_SUCCESS;
}

static std::uint8_t hex2bin(const char* hex) {
    std::uint8_t val = 0;
    for (size_t n = 0; n < 2; n++) {
        char c = hex[n];
        if (c == '\0')
            break;

        if (c >= '0' && c <= '9')
            c = c - '0';
        else if (c >= 'a' && c <= 'f')
            c = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            c = c - 'A' + 10;

        val = (val << 4) | (c & 0xf);
    }

    return val;
}

static size_t read_ihex(const std::string& ihex_file, std::uint8_t** fw_data) {
    *fw_data = static_cast<uint8_t*>(std::malloc(128 * 1024));
    memset(*fw_data, 0xff, 128 * 1024);

    uint32_t extended_address = 0;
    size_t data_length = 0;

    // size_t ret = 0;
    FILE* fp = std::fopen(ihex_file.c_str(), "r");
    if (fp == nullptr) {
        SPDLOG_ERROR("Failed to open file: %s", ihex_file.c_str());
        exit(EXIT_FAILURE);
    }

    char* line = nullptr;
    size_t linecap = 0;

    while (getline(&line, &linecap, fp) > 0) {
        if (line[0] != ':')
            continue;
        std::uint8_t tmp_crc = 0;

        std::uint8_t size = hex2bin(line + 1); // byte count
        tmp_crc += size;
        // TODO: fw_data size
        std::uint32_t address = extended_address;
        std::uint8_t tmp = hex2bin(line + 3); // address MSB
        tmp_crc += tmp;
        address += (tmp << 8);

        tmp = hex2bin(line + 5); // address LSB
        tmp_crc += tmp;
        address += tmp;

        std::uint8_t rtype = hex2bin(line + 7); // record type
        tmp_crc += rtype;
        if (rtype == 0) {
            if (data_length < address)
                data_length = address;

            size_t n = 0;
            for (; n < size; n++) {
                std::uint8_t tmp = hex2bin(line + 9 + n * 2);
                (*fw_data)[data_length++] = tmp;
                tmp_crc += tmp;
            }
            std::uint8_t crc = hex2bin(line + 9 + n * 2);

            if (static_cast<std::uint8_t>(0x100 - tmp_crc) != crc) {
                SPDLOG_ERROR("Intel HEX file corrupted");
                data_length = 0;
                break;
            }
        }
        else if (rtype == 4) {
            std::uint8_t tmp = hex2bin(line + 9 + 0 * 2);
            tmp_crc += tmp;
            extended_address = tmp;
            extended_address <<= 8;

            tmp = hex2bin(line + 9 + 1 * 2);
            tmp_crc += tmp;
            extended_address += tmp;
            extended_address <<= 16;

            std::uint8_t crc = hex2bin(line + 9 + 2 * 2);

            if (static_cast<std::uint8_t>(0x100 - tmp_crc) != crc) {
                SPDLOG_ERROR("Intel HEX file corrupted");
                data_length = 0;
                break;
            }

            if (extended_address > (1 << 16)) { // more then 128kB
                break;                          // skip reading EEPROM, CONFIG e.t.c.
            }
        }
    }
    fclose(fp);
    if (line) {
        free(line);
    }
    return data_length;
}
