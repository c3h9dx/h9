/*
 * Created by crowx on 24/11/2024.
 *
 */

#include "atu.h"
#include "dev_node_exception.h"

std::string ATU::dev_type = "ATU";
DevLoader::RegisterDev ATU::register_helper(ATU::dev_type, ATU::create_dev);

ATU::ATU(std::string name, NodeMgr*node_mgr, std::vector<std::uint16_t> nodes):
    Dev(ATU::dev_type, std::move(name), node_mgr, std::move(nodes)),
    atu_id(0xffff) {

//    add_method("power_on", &PowerSwitch::power_on_method);
//    add_method("power_off", &PowerSwitch::power_off_method);
}

void ATU::periodic_task() {
    SPDLOG_INFO("{} periodic_task", name);
}

bool ATU::is_init() {
    return atu_id <= H9frame::H9FRAME_SOURCE_ID_MAX_VALUE;
}

void ATU::init() {
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

void ATU::on_dev_info(std::uint16_t node_id, std::uint16_t type, std::uint16_t version_major, std::uint16_t version_minor, char hardware_revision) {
    SPDLOG_INFO("{} {}: on_dev_info ########################", name, node_id);
    atu_id = node_id;
}

void ATU::on_dev_register_value(std::uint16_t node_id, uint8_t reg, Node::regvalue_t value) {
    //    emit_dev_state({{"selected_antenna", selected_antenna},
    //                    {"number_of_antenna", number_of_antenna},
    //                    {"antennas_name", std::vector<std::string>(antenna_name, &antenna_name[number_of_antenna])}});
}

void ATU::on_dev_bulk(std::uint16_t node_id, H9frame::Type msg_type, uint8_t dlc, const uint8_t* data) {
    if (msg_type == H9frame::Type::NODE_SPECIFIC_BULK0) {
        std::uint16_t swr = data[0] << 8 | data[1];
        std::uint16_t pwr = data[2] << 8 | data[3];

        emit_dev_state({{"swr", swr}, {"pwr", pwr}});
    }
}

//void PowerSwitch::update_dev_state(std::uint16_t node_id, const ExtH9Frame& frame) {
//    SPDLOG_INFO("@{} update_dev_state", name);
//
//    emit_dev_state({{"selected_antenna", selected_antenna},
//                    {"number_of_antenna", number_of_antenna},
//                    {"antennas_name", std::vector<std::string>(antenna_name, &antenna_name[number_of_antenna])}});
//}

nlohmann::json ATU::get_dev_state(const std::map<std::string, nlohmann::json>& param_map) {
    return {{"state", 1},
            {"error", 0}};
}
