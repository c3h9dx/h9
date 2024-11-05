/*
 * Created by crowx on 25/10/2023.
 *
 */

#include "cli_cache.h"
#include <spdlog/spdlog.h>

void CliCache::refresh_node() {
    jsonrpcpp::Id id(h9d->get_next_id());

    jsonrpcpp::Request req(id, "get_nodes_list");
    h9d->send(std::make_shared<jsonrpcpp::Request>(req));

    jsonrpcpp::entity_ptr raw_msg;
    try {
        raw_msg = h9d->recv();
    }
    catch (std::system_error& e) {
        SPDLOG_ERROR("Messages receiving error: {}.", e.code().message());
        exit(EXIT_FAILURE);
    }
    catch (std::runtime_error& e) {
        SPDLOG_ERROR("Messages receiving error: {}.", e.what());
        exit(EXIT_FAILURE);
    }

    node_list.clear();
    node_name_to_id.clear();

    if (raw_msg->is_response()) {
        jsonrpcpp::response_ptr msg = std::dynamic_pointer_cast<jsonrpcpp::Response>(raw_msg);
        auto j = msg->result();
        for (auto& dev : j) {
            std::string name = dev["name"].get<std::string>();
            node_list.push_back(name);
            node_name_to_id[name] = dev["id"].get<std::uint16_t>();
            //printf("::%s\n", name.c_str());
            //std::cout << dev["name"] << '\n';
        }
    }
}

void CliCache::refresh_dev() {
    jsonrpcpp::Id id(h9d->get_next_id());

    jsonrpcpp::Request req(id, "get_devs_list");
    h9d->send(std::make_shared<jsonrpcpp::Request>(req));

    jsonrpcpp::entity_ptr raw_msg;
    try {
        raw_msg = h9d->recv();
    }
    catch (std::system_error& e) {
        SPDLOG_ERROR("Messages receiving error: {}.", e.code().message());
        exit(EXIT_FAILURE);
    }
    catch (std::runtime_error& e) {
        SPDLOG_ERROR("Messages receiving error: {}.", e.what());
        exit(EXIT_FAILURE);
    }

    dev_list.clear();

    if (raw_msg->is_response()) {
        jsonrpcpp::response_ptr msg = std::dynamic_pointer_cast<jsonrpcpp::Response>(raw_msg);
        auto j = msg->result();
        for (auto& dev : j) {
            std::string name = dev["name"].get<std::string>();
            dev_list.push_back(name);
        }
    }
}

void CliCache::refresh_register(std::uint16_t node_id) {
    jsonrpcpp::Id id(h9d->get_next_id());

    jsonrpcpp::Request req(id, "get_registers_list", nlohmann::json({{"node_id", node_id}}));
    h9d->send(std::make_shared<jsonrpcpp::Request>(req));

    jsonrpcpp::entity_ptr raw_msg;
    try {
        raw_msg = h9d->recv();
    }
    catch (std::system_error& e) {
        SPDLOG_ERROR("Messages receiving error: {}.", e.code().message());
        exit(EXIT_FAILURE);
    }
    catch (std::runtime_error& e) {
        SPDLOG_ERROR("Messages receiving error: {}.", e.what());
        exit(EXIT_FAILURE);
    }

    node_registries_list[node_id].clear();
    node_registries_name_to_number[node_id].clear();
    node_registries_bits_list[node_id].clear();

    if (raw_msg->is_response()) {
        jsonrpcpp::response_ptr msg = std::dynamic_pointer_cast<jsonrpcpp::Response>(raw_msg);
        auto j = msg->result();
        for (auto& reg : j) {
            std::string name = reg["name"].get<std::string>();
            std::uint8_t reg_number = reg["number"].get<std::uint8_t>();
            node_registries_list[node_id].push_back(name);
            node_registries_name_to_number[node_id][name] = reg_number;

            node_registries_bits_list[node_id][reg_number] = reg["bits_names"].get<std::vector<std::string>>();

            std::uint8_t bit_index = 0;
            for (auto& bit : reg["bits_names"]) {
                node_registries_bits_name_to_number[node_id][reg_number][bit.get<std::string>()] = bit_index;
                ++bit_index;
            }
        }
    }
}

void CliCache::refresh_dev_method(const std::string& dev_name) {
    jsonrpcpp::Id id(h9d->get_next_id());

    jsonrpcpp::Request req(id, "get_dev_description", nlohmann::json({{"dev_name", dev_name}}));
    h9d->send(std::make_shared<jsonrpcpp::Request>(req));

    jsonrpcpp::entity_ptr raw_msg;
    try {
        raw_msg = h9d->recv();
    }
    catch (std::system_error& e) {
        SPDLOG_ERROR("Messages receiving error: {}.", e.code().message());
        exit(EXIT_FAILURE);
    }
    catch (std::runtime_error& e) {
        SPDLOG_ERROR("Messages receiving error: {}.", e.what());
        exit(EXIT_FAILURE);
    }

    dev_method_list[dev_name].clear();

    if (raw_msg->is_response()) {
        jsonrpcpp::response_ptr msg = std::dynamic_pointer_cast<jsonrpcpp::Response>(raw_msg);
        auto dev_description = msg->result();
        dev_method_list[dev_name] = dev_description["dev_methods"].get<std::vector<std::string>>();
    }
}

CliCache::CliCache(H9Connector* connector): h9d(connector) {

}

std::vector<std::string>* CliCache::get_nodes_list() {
    if (node_list.empty()) {
        refresh_node();
    }
    return &node_list;
}

std::vector<std::string>* CliCache::get_registers_list(std::uint16_t node_id) {
    if (node_registries_list.count(node_id) == 0) {
        refresh_register(node_id);
    }
    return &node_registries_list[node_id];
}

std::vector<std::string>* CliCache::get_bits_list(std::uint16_t node_id, std::uint8_t reg_number) {
    if (node_registries_bits_list.count(node_id) == 0) {
        refresh_register(node_id);
    }
    return &node_registries_bits_list[node_id][reg_number];
}

std::vector<std::string>* CliCache::get_dev_list() {
    if (dev_list.empty()) {
        refresh_dev();
    }
    return &dev_list;
}

std::vector<std::string>* CliCache::get_dev_method_list(const std::string& dev_name) {
    if (dev_method_list.count(dev_name) == 0) {
        refresh_dev_method(dev_name);
    }
    return &dev_method_list[dev_name];
}

std::uint16_t CliCache::get_node_id_by_name(const std::string& name) {
    if (node_name_to_id.count(name) == 0) {
        refresh_node();
    }

    if (node_name_to_id.count(name) == 0) {
        SPDLOG_ERROR("Unknow node: '{}'.", name);
        return 0xffff;
    }

    return node_name_to_id[name];
}

std::uint8_t CliCache::get_register_number_by_name(std::uint16_t node_id, const std::string& reg_name) {
    if (node_registries_name_to_number.count(node_id) == 0 || node_registries_name_to_number[node_id].count(reg_name) == 0) {
        refresh_register(node_id);
    }

    if (node_registries_name_to_number.count(node_id) == 0 || node_registries_name_to_number[node_id].count(reg_name) == 0) {
        if (node_registries_name_to_number.count(node_id) == 0)
            SPDLOG_ERROR("Node {} not exist.", node_id);
        else
            SPDLOG_ERROR("Unknow register: '{}' in node: {}.", reg_name, node_id);
        return 0;
    }

    return node_registries_name_to_number[node_id][reg_name];
}

std::uint8_t CliCache::get_bit_number_by_name(std::uint16_t node_id, std::uint8_t reg_number, const std::string& bit_name) {
    if (node_registries_bits_name_to_number.count(node_id) == 0 || node_registries_bits_name_to_number[node_id].count(reg_number) == 0) {
        refresh_register(node_id);
    }

    if (node_registries_bits_name_to_number.count(node_id) == 0
        || node_registries_bits_name_to_number[node_id].count(reg_number) == 0
        || node_registries_bits_name_to_number[node_id][reg_number].count(bit_name) == 0) {
        if (node_registries_bits_name_to_number.count(node_id) == 0)
            SPDLOG_ERROR("Node {} not exist.", node_id);
        else if (node_registries_bits_name_to_number[node_id].count(reg_number) == 0)
            SPDLOG_ERROR("Unknow register: {} in node: {}.", reg_number, node_id);
        else
            SPDLOG_ERROR("Unknow bit: '{}' in reg: {} node: {}.", bit_name, reg_number, node_id);
        return 0xff;
    }

    return node_registries_bits_name_to_number[node_id][reg_number][bit_name];
}

void CliCache::clear() {
    node_list.clear();
    node_name_to_id.clear();

    dev_list.clear();
    dev_method_list.clear();

    node_registries_list.clear();
    node_registries_name_to_number.clear();
    node_registries_bits_list.clear();
    node_registries_bits_name_to_number.clear();
}
