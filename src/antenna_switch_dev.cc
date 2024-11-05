/*
 * Created by crowx on 29/10/2023.
 *
 */

#include "antenna_switch_dev.h"

#include <utility>

#include "dev_node_exception.h"

AntennaSwitchDev::AntennaSwitchDev(std::string name, NodeMgr* node_mgr, std::uint16_t switch_node_id):
    Dev("AntennaSwitchDev", std::move(name), node_mgr, {switch_node_id}),
    switch_node_id(switch_node_id),
    controller_node_id(0xffff),
    selected_antenna(0),
    number_of_antenna(0) {

    add_method("select_antenna", &AntennaSwitchDev::select_antenna_method);
}

AntennaSwitchDev::AntennaSwitchDev(std::string name, NodeMgr* node_mgr, std::uint16_t switch_node_id, std::uint16_t controller_node_id):
    Dev("AntennaSwitchDev", std::move(name), node_mgr, {switch_node_id, controller_node_id}),
    switch_node_id(switch_node_id),
    controller_node_id(controller_node_id),
    selected_antenna(0),
    number_of_antenna(0) {

    add_method("select_antenna", &AntennaSwitchDev::select_antenna_method);
}

void AntennaSwitchDev::init() {
//    selected_antenna = std::get<std::int64_t>(node_mgr->get_register(switch_node_id, ANTENNA_SELECT_REG));
//    number_of_antenna = std::get<std::int64_t>(node_mgr->get_register(switch_node_id, NUMBER_OF_ANTENNAS_REG));
//
//    for (int i = 0; i < number_of_antenna; ++i) {
//        antenna_name[i] = std::get<std::string>(node_mgr->get_register(switch_node_id, FIRST_ANTENNA_NAME + i));
//    }
}

void AntennaSwitchDev::update_dev_state(std::uint16_t node_id, const ExtH9Frame& frame) {
    if (node_id != switch_node_id)
        return;

    SPDLOG_INFO("update_dev_state");

    Node::regvalue_t reg_value;
    std::uint8_t reg = 0;
    try {
        reg = node_mgr->get_reg_value_from_frame(node_id, frame, &reg_value);
    }
    catch (DeviceException& e) {
        SPDLOG_ERROR(e.what());
    }

    if (reg) {
        switch (reg) {
        case ANTENNA_SELECT_REG:
            selected_antenna = std::get<std::int64_t>(reg_value);
            break;
        case NUMBER_OF_ANTENNAS_REG:
            number_of_antenna = std::get<std::int64_t>(reg_value);
            break;
        case FIRST_ANTENNA_NAME:
            antenna_name[0] = std::get<std::string>(reg_value);
            break;
        case SECOND_ANTENNA_NAME:
            antenna_name[1] = std::get<std::string>(reg_value);
            break;
        case THIRD_ANTENNA_NAME:
            antenna_name[2] = std::get<std::string>(reg_value);
            break;
        case FOURTH_ANTENNA_NAME:
            antenna_name[3] = std::get<std::string>(reg_value);
            break;
        case FIFTH_ANTENNA_NAME:
            antenna_name[4] = std::get<std::string>(reg_value);
            break;
        case SIXTH_ANTENNA_NAME:
            antenna_name[5] = std::get<std::string>(reg_value);
            break;
        case SEVENTH_ANTENNA_NAME:
            antenna_name[6] = std::get<std::string>(reg_value);
            break;
        case EIGHTH_ANTENNA_NAME:
            antenna_name[7] = std::get<std::string>(reg_value);
            break;
        }
    }

    emit_dev_state({{"selected_antenna", selected_antenna},
                    {"number_of_antenna", number_of_antenna},
                    {"antennas_name", std::vector<std::string>(antenna_name, &antenna_name[number_of_antenna])}});
}

nlohmann::json AntennaSwitchDev::get_dev_state(const std::map<std::string, nlohmann::json>& param_map) {
    return {{"selected_antenna", selected_antenna},
            {"number_of_antenna", number_of_antenna},
            {"antennas_name", std::vector<std::string>(antenna_name, &antenna_name[number_of_antenna])}};
}

void AntennaSwitchDev::select_antenna(int antenna_number) {
    try {
        node_mgr->set_register(switch_node_id, ANTENNA_SELECT_REG, antenna_number);
    }
    catch (DevNodeException& e) {
        SPDLOG_ERROR(e.what());
    }
}

nlohmann::json AntennaSwitchDev::select_antenna_method(const std::map<std::string, nlohmann::json>& param_map) {
    int antenna_number = param_map.at("antenna_number").get<int>();
    select_antenna(antenna_number);
    return {};
}
