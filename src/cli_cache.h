/*
 * Created by crowx on 25/10/2023.
 *
 */

#pragma once

#include "h9connector.h"

class CliCache {
  private:
    H9Connector* h9d;

    std::vector<std::string> node_list;
    std::map<std::string, std::uint16_t> node_name_to_id;

    std::vector<std::string> dev_list;

    std::map<std::uint16_t, std::vector<std::string>> node_registries_list;
    std::map<std::uint16_t, std::map<std::string, std::uint16_t>> node_registries_name_to_number;

    std::map<std::uint16_t, std::map<std::uint8_t, std::vector<std::string>>> node_registries_bits_list;
    std::map<std::uint16_t, std::map<std::uint8_t, std::map<std::string, std::uint8_t>>> node_registries_bits_name_to_number;

    void refresh_node();
    void refresh_dev();
    void refresh_register(std::uint16_t node_id);
  public:
    CliCache(H9Connector* connector);

    std::vector<std::string>* get_nodes_list();
    std::vector<std::string>* get_registers_list(std::uint16_t node_id);
    std::vector<std::string>* get_bits_list(std::uint16_t node_id, std::uint8_t reg_number);

    std::vector<std::string>* get_dev_list();

    std::uint16_t get_node_id_by_name(const std::string& name);
    std::uint8_t get_register_number_by_name(std::uint16_t node_id, const std::string& reg_name);
    std::uint8_t get_bit_number_by_name(std::uint16_t node_id, std::uint8_t reg_number, const std::string& bit_name);

    void clear();
};
