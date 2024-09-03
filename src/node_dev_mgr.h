/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-19.
 *
 * Copyright (C) 2020-2023 Kamil Palkowski. All rights reserved.
 */

#pragma once

#include "config.h"

#include <atomic>
#include <jsonrpcpp/jsonrpcpp.hpp>
#include <map>
#include <queue>
#include <shared_mutex>
#include <spdlog/spdlog.h>
#include <thread>

#include "bus.h"
#include "dev_loader.h"
#include "frameobserver.h"
#include "framesubject.h"
#include "node.h"
#include "raw_node.h"

class TCPClientThread;
class DevStatusObserver;
class Dev;

class NodeDevMgr: public FrameSubject {
  private:
    std::shared_ptr<spdlog::logger> logger;
    Bus* bus;

    // TODO: nadpisac notify_frame_observer - żeby zmniejszyc zlozonosc, break po pierwszym match
    class NodeMgrFrameObs: public FrameObserver {
        NodeDevMgr* node_mgr;

      public:
        NodeMgrFrameObs(Bus* bus, NodeDevMgr* node_mgr):
            FrameObserver(bus, H9FrameComparator()),
            node_mgr(node_mgr) {}

        void on_frame_recv(const ExtH9Frame& frame) override {
            node_mgr->on_frame_recv(frame);
        }

        void on_frame_send(const ExtH9Frame& frame) override {
        }
    };

    NodeMgrFrameObs frame_obs;
    int _response_timeout_duration;

    virtual void on_frame_recv(const ExtH9Frame& frame) noexcept;

    std::shared_mutex nodes_map_mtx;
    std::shared_mutex devs_map_mtx;
    std::shared_mutex dev_status_observer_mtx;

    std::map<std::uint16_t, Node*> nodes_map;
    //std::vector<Dev*> loaded_inactive_dev;
    std::map<std::string, Dev*> devs_map;
    std::vector<DevStatusObserver*> dev_status_observer;

    std::map<std::uint16_t, std::vector<Dev*>> node_state_observers;

    std::mutex frame_queue_mtx;
    std::condition_variable frame_queue_cv; // TODO: counting_semaphore (C++20)?
    std::queue<ExtH9Frame> frame_queue;

    DevLoader dev_desc_loader;

    std::atomic_bool nodes_update_thread_run;
    std::thread nodes_update_thread_desc;
    void nodes_dev_update_thread();
    void update_dev_after_node_discovered(std::uint16_t node_id, std::uint16_t node_type);

    Node* build_node(std::uint16_t node_id, std::uint16_t node_type, std::uint64_t node_version) noexcept;
    void add_node(std::uint16_t node_id, std::uint16_t node_type, std::uint64_t node_version) noexcept;
    void update_device_last_seen_time(std::uint16_t node_id) noexcept;
  public:
    struct NodeDsc {
        std::uint16_t id;
        std::uint16_t type;
        std::uint16_t version_major;
        std::uint16_t version_minor;
        std::uint16_t version_patch;
        std::string name;
    };

    struct NodeInfo: public NodeDsc {
        std::time_t created_time;
        std::time_t last_seen_time;
        std::string description;
    };

    explicit NodeDevMgr(Bus* bus);
    NodeDevMgr(const NodeDevMgr& a) = delete;
    ~NodeDevMgr();
    void load_nodes_description(const std::string& nodes_description_filename);
    void load_devs_configuration(const std::string& devs_description_filename);

    void response_timeout_duration(int response_timeout_duration);
    int response_timeout_duration() const;

    int discover();

    int active_devices_count() noexcept;
    bool is_node_exist(std::uint16_t node_id) noexcept;
    std::vector<NodeDevMgr::NodeDsc> get_nodes_list() noexcept;

    void attach_node_state_observer(std::uint16_t node_id, Dev* obs);
    void detach_node_state_observer(std::uint16_t node_id, Dev* obs);

    std::vector<Node::RegisterDsc> get_registers_list(std::uint16_t node_id) noexcept;

    int get_node_info(std::uint16_t node_id, NodeInfo& node_info);
    void node_reset(std::uint16_t node_id);
    Node::regvalue_t set_register(std::uint16_t node_id, std::uint8_t reg, Node::regvalue_t value);
    Node::regvalue_t get_register(std::uint16_t node_id, std::uint8_t reg);
    Node::regvalue_t set_register_bit(std::uint16_t node_id, std::uint8_t reg, std::uint8_t bit_num);
    Node::regvalue_t clear_register_bit(std::uint16_t node_id, std::uint8_t reg, std::uint8_t bit_num);
    Node::regvalue_t toggle_register_bit(std::uint16_t node_id, std::uint8_t reg, std::uint8_t bit_num);

    std::uint8_t get_reg_value_from_frame(std::uint16_t node_id, const ExtH9Frame& frame, Node::regvalue_t* value);

    struct DevDsc {
        std::string name;
        std::string type;
    };

    std::vector<NodeDevMgr::DevDsc> get_devs_list() noexcept;
    nlohmann::json call_dev_method(const std::string& dev_name, const TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json get_dev_description(const std::string& dev_name, const TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json get_dev_state(const std::string& dev_name, const TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);

    void emit_dev_state(const std::string& dev_id, const nlohmann::json& dev_status);
    void attach_dev_state_observer(const std::string& dev_id, DevStatusObserver* obs);
    void detach_dev_state_observer(const std::string& dev_id, DevStatusObserver* obs);

    void add_dev(Dev* dev);
};
