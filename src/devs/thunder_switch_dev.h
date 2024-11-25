/*
 * Created by crowx on 04/11/2024.
 *
 */

#pragma once

#include "dev.h"
#include "dev_loader.h"

class ThunderSwitchDev: public Dev {
  private:
    static std::string dev_type;
    static DevLoader::RegisterDev register_helper;

    uint16_t switch_id;
  public:
    static Dev* create_dev(std::string name, NodeMgr* node_mgr, std::vector<std::uint16_t> nodes) {
        return new ThunderSwitchDev(std::move(name), node_mgr, std::move(nodes));
    }

    ThunderSwitchDev(std::string name, NodeMgr*node_mgr, std::vector<std::uint16_t> nodes);

    void periodic_task() override;

    bool is_init() override;

//    void update_dev_state(std::uint16_t node_id, const ExtH9Frame& frame) override;

    nlohmann::json get_dev_state(const std::map<std::string, nlohmann::json>& param_map) override;

    nlohmann::json switch_on_method(const std::map<std::string, nlohmann::json>& param_map);
    nlohmann::json switch_off_method(const std::map<std::string, nlohmann::json>& param_map);
};
