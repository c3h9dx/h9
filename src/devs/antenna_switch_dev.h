/*
 * Created by crowx on 29/10/2023.
 *
 */

#pragma once

#include "dev.h"
#include "dev_loader.h"

class AntennaSwitchDev: public Dev {
  public:
    constexpr static std::uint16_t ANTENNA_SWITCH_NODE_TYPE = 5;
    constexpr static std::uint16_t ANTENNA_CONTROLLER_NODE_TYPE = 3;

    constexpr static int MAX_ANTENNAS = 8;
    constexpr static std::uint8_t ANTENNA_SELECT_REG = 10;
    constexpr static std::uint8_t NUMBER_OF_ANTENNAS_REG = 12;
    constexpr static std::uint8_t FIRST_ANTENNA_NAME = 13;
    constexpr static std::uint8_t SECOND_ANTENNA_NAME = FIRST_ANTENNA_NAME + 1;
    constexpr static std::uint8_t THIRD_ANTENNA_NAME = SECOND_ANTENNA_NAME + 1;
    constexpr static std::uint8_t FOURTH_ANTENNA_NAME = THIRD_ANTENNA_NAME + 1;
    constexpr static std::uint8_t FIFTH_ANTENNA_NAME = FOURTH_ANTENNA_NAME + 1;
    constexpr static std::uint8_t SIXTH_ANTENNA_NAME = FIFTH_ANTENNA_NAME + 1;
    constexpr static std::uint8_t SEVENTH_ANTENNA_NAME = SIXTH_ANTENNA_NAME + 1;
    constexpr static std::uint8_t EIGHTH_ANTENNA_NAME = SEVENTH_ANTENNA_NAME + 1;
  private:
    static std::string dev_type;
    static DevLoader::RegisterDev register_helper;

    std::uint16_t switch_node_id;
    std::uint16_t controller_node_id;

    std::uint8_t selected_antenna;
    std::uint8_t number_of_antenna;
    std::string antenna_name[MAX_ANTENNAS];
  public:
    static Dev* create_dev(std::string name, NodeMgr* node_mgr, std::vector<std::uint16_t> nodes) {
        return new AntennaSwitchDev(std::move(name), node_mgr, std::move(nodes));
    }

    AntennaSwitchDev(std::string name, NodeMgr*node_mgr, std::vector<std::uint16_t> nodes);

    void periodic_task() override;

    bool is_init() override;

//    void update_dev_state(std::uint16_t node_id, const ExtH9Frame& frame) override;
    void on_dev_info(std::uint16_t node_id, std::uint16_t type, std::uint16_t version_major, std::uint16_t version_minor, char hardware_revision) override;
    void on_dev_register_value(std::uint16_t node_id, uint8_t reg, Node::regvalue_t value) override;

    nlohmann::json get_dev_state(const std::map<std::string, nlohmann::json>& param_map) override;

    void select_antenna(int antenna_number);
    nlohmann::json select_antenna_method(const std::map<std::string, nlohmann::json>& param_map);
};
