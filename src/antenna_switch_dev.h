/*
 * Created by crowx on 29/10/2023.
 *
 */

#pragma once

#include "dev.h"

class AntennaSwitchDev: public Dev {
  public:
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
    const std::uint16_t switch_node_id;
    const std::uint16_t controller_node_id;

    std::uint8_t selected_antenna;
    std::uint8_t number_of_antenna;
    std::string antenna_name[MAX_ANTENNAS];
  public:
    AntennaSwitchDev(std::string name, NodeMgr*node_mgr, std::uint16_t switch_node_id);
    AntennaSwitchDev(std::string name, NodeMgr*node_mgr, std::uint16_t switch_node_id, std::uint16_t controller_node_id);

    void init() override;
    void update_dev_state(std::uint16_t node_id, const ExtH9Frame& frame) override;
    nlohmann::json get_dev_state(const std::map<std::string, nlohmann::json>& param_map) override;

    void select_antenna(int antenna_number);
    nlohmann::json select_antenna_method(const std::map<std::string, nlohmann::json>& param_map);
};
