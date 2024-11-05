/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-19.
 *
 * Copyright (C) 2020-2024 Kamil Palkowski. All rights reserved.
 */

#include "node_mgr.h"

#include <cassert>
#include <utility>

#include "bus.h"
#include "dev.h"
#include "dev_node_exception.h"
#include "dev_status_observer.h"
#include "h9d_configurator.h"
#include "tcpclientthread.h"

void NodeMgr::on_frame_recv(const ExtH9Frame& frame) noexcept {
    frame_queue_mtx.lock();
    frame_queue.push(frame);
    frame_queue_mtx.unlock();
    frame_queue_cv.notify_one();
}

void NodeMgr::nodes_dev_update_thread() {
    // #if defined(__APPLE__) && defined(__MACH__)
    //     pthread_setname_np("node_dev");
    // #elif defined(__linux__)
    //     pthread_setname_np(nodes_update_thread_desc.native_handle(), "node_dev");
    // #endif

    while (nodes_update_thread_run) {
        std::unique_lock<std::mutex> lk(frame_queue_mtx);
        frame_queue_cv.wait(lk, [this]() { return !frame_queue.empty(); });
        ExtH9Frame frame = frame_queue.front();
        frame_queue.pop();
        int remained_frame = frame_queue.size();
        lk.unlock();

        SPDLOG_LOGGER_TRACE(logger, "Devices update thread: pop frame (remained {}).", remained_frame);

        if (nodes[frame.source_id()] == nullptr) {
            nodes[frame.source_id()] = create_node(frame.source_id());
        }

        if (frame.type() == H9frame::Type::NODE_INFO || frame.type() == H9frame::Type::NODE_TURNED_ON) {
            uint16_t node_type = frame.data()[0] << 8 | frame.data()[1];
            uint16_t version_major = frame.data()[2] << 8 | frame.data()[3];
            uint16_t version_minor = frame.data()[4] << 8 | frame.data()[5];

            uint64_t version = version_major;
            version = version << 16 | version_minor;
            if (frame.type() == H9frame::Type::NODE_TURNED_ON) {
                SPDLOG_LOGGER_INFO(logger, "Dev turned on id: {}, type: {}, version: {}.{}{}.", frame.source_id(), frame.data()[0] << 8 | frame.data()[1], version_major, version_minor, (char)frame.data()[6]);
            } else {
                SPDLOG_LOGGER_INFO(logger, "Dev discovered id: {}, type: {}, version: {}.{}{}.", frame.source_id(), frame.data()[0] << 8 | frame.data()[1], version_major, version_minor, (char)frame.data()[6]);
            }

            init_node(frame.source_id(), node_type, version, frame.data()[6], frame.data()[7]);
        }
        else if (! nodes[frame.source_id()]->is_init()) {
            ExtH9Frame req_frame("h9d", H9frame::Type::DISCOVER, frame.source_id(), 0, {});
            bus->send_frame_noblock(req_frame);
        }

        update_node_last_seen_time(frame.source_id(), frame.creation_timestamp());

        // TODO: wykonywac przez workerow
        nodes[frame.source_id()]->on_frame_recv(frame);
    }

    detach();
}

Node* NodeMgr::create_node(std::uint16_t node_id) noexcept {
    return new Node(this, bus, node_id);
}

void NodeMgr::init_node(std::uint16_t node_id, std::uint16_t node_type, std::uint32_t node_version, char hardware_revision, std::uint8_t reset_reason) noexcept {
    if (nodes[node_id])
        nodes[node_id]->init(node_type, node_version, hardware_revision, reset_reason);
}

void NodeMgr::update_node_last_seen_time(std::uint16_t node_id, timestamp_t timestamp) noexcept {
    if (nodes[node_id])
        nodes[node_id]->update_node_last_seen_time(timestamp);
}

NodeMgr::NodeMgr(Bus* bus):
    FrameObserver(bus, H9FrameComparator()),
    bus(bus) {
    logger = spdlog::get(H9dConfigurator::nodes_logger_name);

    for (int id = 0; id <= H9frame::H9FRAME_SOURCE_ID_MAX_VALUE; ++id) {
        nodes[id] = nullptr;
    }

    nodes_update_thread_run = true;
    nodes_update_thread_desc = std::thread([this]() {
        this->nodes_dev_update_thread();
    });
}

NodeMgr::~NodeMgr() {
    nodes_update_thread_run = false;
    if (nodes_update_thread_desc.joinable())
        nodes_update_thread_desc.join();

    for (int id = 0; id <= H9frame::H9FRAME_SOURCE_ID_MAX_VALUE; ++id) {
        delete nodes[id];
        nodes[id] = nullptr;
    }
}

void NodeMgr::load_nodes_description(const std::string& nodes_description_filename) {
    SPDLOG_LOGGER_INFO(logger, "Loading nodes description file: '{}'.", nodes_description_filename);
    Node::nodedescloader.load_file(nodes_description_filename);
}

void NodeMgr::reload_nodes_description() {
    SPDLOG_LOGGER_INFO(logger, "Reload nodes description file.");
    Node::nodedescloader.reload();

    for (int id = 0; id <= H9frame::H9FRAME_SOURCE_ID_MAX_VALUE; ++id) {
        if (nodes[id] != NULL && nodes[id]->is_init()) {
            nodes[id]->load_description();
        }
    }
}

void NodeMgr::load_devs_configuration(const std::string& devs_description_filename) {
    SPDLOG_LOGGER_INFO(logger, "Loading devs description file: '{}'.", devs_description_filename);

    dev_desc_loader.load_file(devs_description_filename, this);
}

void NodeMgr::response_timeout_duration(int response_timeout_duration) {
    _response_timeout_duration = response_timeout_duration;
}

int NodeMgr::response_timeout_duration() const {
    return _response_timeout_duration;
}

int NodeMgr::discover() {
    ExtH9Frame frame("h9d", H9frame::Type::DISCOVER, H9frame::BROADCAST_ID, 0, {});

    return bus->send_frame(frame);
}

int NodeMgr::active_devices_count() noexcept {
    int ret = 0;
    for (int id = 0; id <= H9frame::H9FRAME_SOURCE_ID_MAX_VALUE; ++id) {
        if (nodes[id])
            ++ret;
    }
    return ret;
}

bool NodeMgr::is_node_exist(std::uint16_t node_id) noexcept {
    return nodes[node_id] != nullptr;
}

std::vector<NodeMgr::NodeDsc> NodeMgr::get_nodes_list() noexcept {
    std::vector<NodeMgr::NodeDsc> ret;

    for (std::uint16_t id = 0; id <= H9frame::H9FRAME_SOURCE_ID_MAX_VALUE; ++id) {
        if (nodes[id])
            ret.push_back({id, nodes[id]->node_type(), nodes[id]->node_version_major(), nodes[id]->node_version_minor(), nodes[id]->node_hardware_revision(), nodes[id]->node_reset_reason()});
        ;
    }

    return std::move(ret);
}

std::vector<Node::RegisterDsc> NodeMgr::get_registers_list(std::uint16_t node_id) noexcept {
    if (nodes[node_id]) {
        return nodes[node_id]->get_registers_list();
    }
    return {};
}

int NodeMgr::get_node_info(std::uint16_t node_id, NodeMgr::NodeInfo& node_info) {
    if (nodes[node_id]) {
        int ret = 0;
        auto node = nodes[node_id];
        node_info.id = node_id;
        node_info.type = node->node_type();
        node_info.name = node->node_name();
        node_info.version_major = node->node_version_major();
        node_info.version_minor = node->node_version_minor();
        node_info.hardware_revision = node->node_hardware_revision();
        node_info.reset_reason = node->node_reset_reason();
        node_info.created_time = node->node_created_time();
        node_info.last_seen_time = node->node_last_seen_time();
        node_info.description = node->node_description();

        return ret;
    }
    throw NodeNotExistException();
}

void NodeMgr::node_reset(std::uint16_t node_id) {
    if (nodes[node_id]) {
        nodes[node_id]->node_reset();
        return;
    }
    throw NodeNotExistException();
}

Node::regvalue_t NodeMgr::set_register(std::uint16_t node_id, std::uint8_t reg, Node::regvalue_t value) {
    if (nodes[node_id]) {
        return nodes[node_id]->set_register(reg, std::move(value));
    }
    throw NodeNotExistException();
}

Node::regvalue_t NodeMgr::get_register(std::uint16_t node_id, std::uint8_t reg) {
    if (nodes[node_id]) {
        return nodes[node_id]->get_register(reg);
    }
    throw NodeNotExistException();
}

Node::regvalue_t NodeMgr::set_register_bit(std::uint16_t node_id, std::uint8_t reg, std::uint8_t bit_num) {
    if (nodes[node_id]) {
        return nodes[node_id]->set_register_bit(reg, bit_num);
    }
    throw NodeNotExistException();
}

Node::regvalue_t NodeMgr::clear_register_bit(std::uint16_t node_id, std::uint8_t reg, std::uint8_t bit_num) {
    if (nodes[node_id]) {
        return nodes[node_id]->clear_register_bit(reg, bit_num);
    }
    throw NodeNotExistException();
}

Node::regvalue_t NodeMgr::toggle_register_bit(std::uint16_t node_id, std::uint8_t reg, std::uint8_t bit_num) {
    if (nodes[node_id]) {
        return nodes[node_id]->toggle_register_bit(reg, bit_num);
    }
    throw NodeNotExistException();
}

std::uint8_t NodeMgr::get_reg_value_from_frame(std::uint16_t node_id, const ExtH9Frame& frame, Node::regvalue_t* value) {
    if (nodes[node_id]) {
        return nodes[node_id]->get_reg_value_from_frame(frame, value);
    }
    throw NodeNotExistException();
}

std::vector<NodeMgr::DevDsc> NodeMgr::get_devs_list() noexcept {
    devs_map_mtx.lock_shared();
    std::vector<NodeMgr::DevDsc> ret;

    ret.reserve(devs_map.size());
    for (auto it : devs_map) {
        ret.push_back({it.first, it.second->type});
    }

    devs_map_mtx.unlock_shared();
    return std::move(ret);
}

nlohmann::json NodeMgr::call_dev_method(const std::string& dev_name, const TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    nlohmann::json r;
    devs_map_mtx.lock_shared();
    if (devs_map.count(dev_name)) {
        try {
            r = devs_map[dev_name]->call_dev_method(client_thread, id, params);
        } catch (...) {
            devs_map_mtx.unlock_shared();
            throw;
        }
    } else {
        devs_map_mtx.unlock_shared();
        throw DeviceNotExistException(dev_name);
    }
    devs_map_mtx.unlock_shared();
    return r;
}

nlohmann::json NodeMgr::get_dev_description(const std::string& dev_name, const TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    nlohmann::json r;
    devs_map_mtx.lock_shared();
    if (devs_map.count(dev_name)) {
        try {
            r = devs_map[dev_name]->get_dev_description(client_thread, id, params);
        } catch (...) {
            devs_map_mtx.unlock_shared();
            throw;
        }
    } else {
        devs_map_mtx.unlock_shared();
        throw DeviceNotExistException(dev_name);
    }
    devs_map_mtx.unlock_shared();
    return r;
}

nlohmann::json NodeMgr::get_dev_state(const std::string& dev_name, const TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    nlohmann::json r;
    devs_map_mtx.lock_shared();
    if (devs_map.count(dev_name)) {
        try {
            r = devs_map[dev_name]->get_dev_state(params.param_map);
        } catch (...) {
            devs_map_mtx.unlock_shared();
            throw;
        }
    } else {
        devs_map_mtx.unlock_shared();
        throw DeviceNotExistException(dev_name);
    }
    devs_map_mtx.unlock_shared();
    return r;
}

void NodeMgr::emit_dev_state(const std::string& dev_id, const nlohmann::json& dev_status) {
    dev_status_observer_mtx.lock_shared();
    for (auto obs : dev_status_observer) {
        obs->on_dev_state_update(dev_status);
    }
    dev_status_observer_mtx.unlock_shared();
}

void NodeMgr::attach_dev_state_observer(const std::string& dev_id, DevStatusObserver* obs) {
    dev_status_observer_mtx.lock();
    dev_status_observer.push_back(obs);
    dev_status_observer_mtx.unlock();
}

void NodeMgr::detach_dev_state_observer(const std::string& dev_id, DevStatusObserver* obs) {
    dev_status_observer_mtx.lock();
    dev_status_observer.erase(std::remove(dev_status_observer.begin(), dev_status_observer.end(), obs), dev_status_observer.end());
    dev_status_observer_mtx.unlock();
}

void NodeMgr::add_dev(Dev* dev) {
    // loaded_inactive_dev.push_back(dev);
    devs_map_mtx.lock();
    if (devs_map.count(dev->name) == 0) {
        SPDLOG_LOGGER_INFO(logger, "Added dev: '{}', type: {}", dev->name, dev->type);
        devs_map[dev->name] = dev;

        devs_map_mtx.unlock();

        for (auto node : dev->get_nodes_id()) {
            if (node <= H9frame::H9FRAME_SOURCE_ID_MAX_VALUE) {
                if (nodes[node] == nullptr) {
                    nodes[node] = create_node(node);
                }

                nodes[node]->add_dependent_devices(dev);
            }
            else {
                SPDLOG_LOGGER_ERROR(logger, "Dev '{}' trying to add dependency to wrong node #{}", dev->name, node);
                continue;
            }
        }

        dev_workers.dev_init(dev);
    } else {
        SPDLOG_LOGGER_ERROR(logger, "Can not add dev: '{}', type: {} - dev exist", dev->name, dev->type);
        delete dev;
        devs_map_mtx.unlock();
    }
}

void NodeMgr::del_dev(const std::string& dev_id) {
    devs_map_mtx.lock();
    if (devs_map.count(dev_id) == 1) {
        Dev *dev = devs_map[dev_id];

        for (auto node: dev->get_nodes_id()) {
            if (node <= H9frame::H9FRAME_SOURCE_ID_MAX_VALUE) {
                nodes[node]->del_dependent_devices(dev);
            }
        }
    }
    devs_map_mtx.unlock();
}
