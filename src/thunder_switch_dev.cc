/*
 * Created by crowx on 04/11/2024.
 *
 */

#include "thunder_switch_dev.h"
#include "dev_node_exception.h"

ThunderSwitchDev::ThunderSwitchDev(std::string name, NodeMgr*node_mgr, std::vector<std::uint16_t> nodes):
    Dev("ThunderSwitchDev", name, node_mgr, nodes) {

    add_method("switch_on", &ThunderSwitchDev::switch_on_method);
    add_method("switch_off", &ThunderSwitchDev::switch_off_method);
}

void ThunderSwitchDev::init() {

}

void ThunderSwitchDev::update_dev_state(std::uint16_t node_id, const ExtH9Frame& frame) {
    SPDLOG_INFO("@{} update_dev_state", name);
}

nlohmann::json ThunderSwitchDev::get_dev_state(const std::map<std::string, nlohmann::json>& param_map) {
    return {{"state", 1},
            {"error", 0}};
}

nlohmann::json ThunderSwitchDev::switch_on_method(const std::map<std::string, nlohmann::json>& param_map) {
    try {
        node_mgr->set_register(0, 11, 1);
    }
    catch (DevNodeException& e) {
        SPDLOG_ERROR(e.what());
    }
    return {};
}

nlohmann::json ThunderSwitchDev::switch_off_method(const std::map<std::string, nlohmann::json>& param_map) {
    try {
        node_mgr->set_register(0, 11, 0);
    }
    catch (DevNodeException& e) {
        SPDLOG_ERROR(e.what());
    }
    return {};
}
