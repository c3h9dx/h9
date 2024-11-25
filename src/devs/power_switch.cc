/*
 * Created by crowx on 22/11/2024.
 *
 */

#include "power_switch.h"
#include "dev_node_exception.h"

std::string PowerSwitch::dev_type = "PowerSwitch";
DevLoader::RegisterDev PowerSwitch::register_helper(PowerSwitch::dev_type, PowerSwitch::create_dev);

PowerSwitch::PowerSwitch(std::string name, NodeMgr*node_mgr, std::vector<std::uint16_t> nodes):
    Dev(PowerSwitch::dev_type, std::move(name), node_mgr, std::move(nodes)),
    switch_id(0xffff) {

    add_method("power_on", &PowerSwitch::power_on_method);
    add_method("power_off", &PowerSwitch::power_off_method);
}

void PowerSwitch::periodic_task() {
    SPDLOG_INFO("{} periodic_task", name);
}

bool PowerSwitch::is_init() {
    return switch_id <= H9frame::H9FRAME_SOURCE_ID_MAX_VALUE;
}

void PowerSwitch::init() {
    for (auto node: get_nodes_id()) {
        uint16_t node_type;
        uint16_t version_major;
        uint16_t version_minor;
        char hardware_revision;

        try {
            node_mgr->node_discovery(node, node_type, version_major, version_minor, hardware_revision);
        }
        catch (...) {
            continue;
        }

        on_dev_info(node, node_type, version_major, version_minor, hardware_revision);
    }
//    node_mgr->
}

void PowerSwitch::on_dev_info(std::uint16_t node_id, std::uint16_t type, std::uint16_t version_major, std::uint16_t version_minor, char hardware_revision) {
    SPDLOG_INFO("{} {}: on_dev_info ########################", name, node_id);
    switch_id = node_id;
}

void PowerSwitch::on_dev_register_value(std::uint16_t node_id, uint8_t reg, Node::regvalue_t value) {
    //    emit_dev_state({{"selected_antenna", selected_antenna},
    //                    {"number_of_antenna", number_of_antenna},
    //                    {"antennas_name", std::vector<std::string>(antenna_name, &antenna_name[number_of_antenna])}});
}

//void PowerSwitch::update_dev_state(std::uint16_t node_id, const ExtH9Frame& frame) {
//    SPDLOG_INFO("@{} update_dev_state", name);
//
//    emit_dev_state({{"selected_antenna", selected_antenna},
//                    {"number_of_antenna", number_of_antenna},
//                    {"antennas_name", std::vector<std::string>(antenna_name, &antenna_name[number_of_antenna])}});
//}

nlohmann::json PowerSwitch::get_dev_state(const std::map<std::string, nlohmann::json>& param_map) {
    return {{"state", 1},
            {"error", 0}};
}

nlohmann::json PowerSwitch::power_on_method(const std::map<std::string, nlohmann::json>& param_map) {
    try {
        SPDLOG_INFO("{} {}", name, switch_id);
        node_mgr->set_register(switch_id, 11, 1);
    }
    catch (DevNodeException& e) {
        SPDLOG_ERROR(e.what());
    }
    return {};
}

nlohmann::json PowerSwitch::power_off_method(const std::map<std::string, nlohmann::json>& param_map) {
    try {
        node_mgr->set_register(switch_id, 11, 0);
    }
    catch (DevNodeException& e) {
        SPDLOG_ERROR(e.what());
    }
    return {};
}
