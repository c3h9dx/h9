/*
 * Created by crowx on 04/11/2024.
 *
 */

#include "thunder_switch_dev.h"
#include "dev_node_exception.h"

std::string ThunderSwitchDev::dev_type = "ThunderSwitch";
DevLoader::RegisterDev ThunderSwitchDev::register_helper(ThunderSwitchDev::dev_type, ThunderSwitchDev::create_dev);

ThunderSwitchDev::ThunderSwitchDev(std::string name, NodeMgr*node_mgr, std::vector<std::uint16_t> nodes):
    Dev(ThunderSwitchDev::dev_type, std::move(name), node_mgr, std::move(nodes)),
    switch_id(0xffff) {

    add_method("switch_on", &ThunderSwitchDev::switch_on_method);
    add_method("switch_off", &ThunderSwitchDev::switch_off_method);
}

void ThunderSwitchDev::periodic_task() {
    SPDLOG_INFO("{} periodic_task", name);
}

bool ThunderSwitchDev::is_init() {
    return switch_id <= H9frame::H9FRAME_SOURCE_ID_MAX_VALUE;
}

//void ThunderSwitchDev::update_dev_state(std::uint16_t node_id, const ExtH9Frame& frame) {
//    SPDLOG_INFO("@{} update_dev_state", name);
//}

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
