/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-23.
 *
 * Copyright (C) 2020-2024 Kamil Palkowski. All rights reserved.
 */

#ifndef H9_NODE_H
#define H9_NODE_H

#include "config.h"

#include <exception>
#include <map>
#include <mutex>
#include <set>
#include <variant>
#include <vector>
#include <list>

#include "types.h"
#include "node_desc_loader.h"
#include "raw_node.h"

class NodeMgr;
class TCPClientThread;
class Dev;

class Node: protected RawNode {
  public:
    using RegisterDsc = NodeDescLoader::RegisterDesc;
    using regvalue_t = std::variant<std::string, std::int64_t, float, std::vector<std::uint8_t>>;

  private:
    std::shared_ptr<spdlog::logger> logger;
    static NodeDescLoader nodedescloader;

    std::uint16_t _node_type;
    std::uint64_t _node_version;
    char _hardware_revision;
    std::uint8_t _reset_reason;
    std::string _node_name;
    std::string _node_description;
    const timestamp_t _created_time;
    timestamp_t _last_seen_time;

    std::map<std::uint8_t, RegisterDsc> register_map;

    friend class NodeMgr;
    void update_node_last_seen_time(timestamp_t timestamp) noexcept;

    std::list<Dev*> dependent_devices;
    std::mutex dependent_devices_mtx;

    bool _init;
  public:
    ////    void notify_event_observer(std::string event_name, GenericMsg msg) noexcept;
    Node(NodeMgr* node_mgr, Bus* bus, std::uint16_t node_id) noexcept;
    void init(std::uint16_t node_type, std::uint32_t node_version, char hardware_revision, std::uint8_t reset_reason);
    bool is_init() { return _init; };

    void load_description();

    void add_dependent_devices(Dev *dev);
    void del_dependent_devices(Dev *dev);

    void on_frame_recv(const ExtH9Frame& frame);

    ~Node();
    std::vector<RegisterDsc> get_registers_list() noexcept;

    [[nodiscard]] std::uint16_t node_type() const noexcept;
    [[nodiscard]] std::uint64_t node_version() const noexcept;
    [[nodiscard]] std::uint16_t node_version_major() const noexcept;
    [[nodiscard]] std::uint16_t node_version_minor() const noexcept;
    [[nodiscard]] char node_hardware_revision() const noexcept;
    [[nodiscard]] std::uint8_t node_reset_reason() const noexcept;
    [[nodiscard]] std::string node_name() const noexcept;
    [[nodiscard]] timestamp_t node_created_time() const noexcept;
    [[nodiscard]] timestamp_t node_last_seen_time() const noexcept;
    [[nodiscard]] std::string node_description() const noexcept;

    void node_reset();
    void node_discovery(std::uint16_t& type, std::uint16_t& version_major, std::uint16_t& version_minor, char& hardware_revision);

    regvalue_t set_register(std::uint8_t reg, regvalue_t value);
    regvalue_t get_register(std::uint8_t reg);
    regvalue_t set_register_bit(std::uint8_t reg, std::uint8_t bit_num);
    regvalue_t clear_register_bit(std::uint8_t reg, std::uint8_t bit_num);
    regvalue_t toggle_register_bit(std::uint8_t reg, std::uint8_t bit_num);

    /// Read value from frame
    /// @param[in] frame
    /// @param[out] value
    /// @return Reg number or 0 when it is not reg value frame
    std::uint8_t get_reg_value_from_frame(const ExtH9Frame& frame, regvalue_t* value);
};

#endif // H9_NODE_H
