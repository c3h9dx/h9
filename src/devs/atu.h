/*
 * Created by crowx on 24/11/2024.
 *
 */

#pragma once

#include "dev.h"
#include "dev_loader.h"

class ATU: public Dev {
  private:
    static std::string dev_type;
    static DevLoader::RegisterDev register_helper;

    uint16_t atu_id;
  public:
    static Dev* create_dev(std::string name, NodeMgr* node_mgr, std::vector<std::uint16_t> nodes) {
        return new ATU(std::move(name), node_mgr, std::move(nodes));
    }

    ATU(std::string name, NodeMgr*node_mgr, std::vector<std::uint16_t> nodes);

    void periodic_task() override;

    bool is_init() override;
    void init() override;

    //    void update_dev_state(std::uint16_t node_id, const ExtH9Frame& frame);
    void on_dev_info(std::uint16_t node_id, std::uint16_t type, std::uint16_t version_major, std::uint16_t version_minor, char hardware_revision) override;
    void on_dev_register_value(std::uint16_t node_id, uint8_t reg, Node::regvalue_t value) override;
    void on_dev_bulk(std::uint16_t node_id, H9frame::Type msg_type, uint8_t dlc, const uint8_t* data) override;

    nlohmann::json get_dev_state(const std::map<std::string, nlohmann::json>& param_map) override;
};
