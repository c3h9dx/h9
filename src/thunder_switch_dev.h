/*
 * Created by crowx on 04/11/2024.
 *
 */

#pragma once

#include "dev.h"

class ThunderSwitchDev: public Dev {
  public:
    ThunderSwitchDev(std::string name, NodeMgr*node_mgr, std::vector<std::uint16_t> nodes);

    void init();
    void update_dev_state(std::uint16_t node_id, const ExtH9Frame& frame);

    nlohmann::json get_dev_state(const std::map<std::string, nlohmann::json>& param_map);

    nlohmann::json switch_on_method(const std::map<std::string, nlohmann::json>& param_map);
    nlohmann::json switch_off_method(const std::map<std::string, nlohmann::json>& param_map);
};
