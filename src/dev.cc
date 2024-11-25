/*
 * Created by crowx on 29/10/2023.
 *
 */

#include "dev.h"

#include <utility>
#include <sys/resource.h>
#include <errno.h>

#ifdef __MACH__
#include <mach/mach.h>
#endif

#include "dev_status_observer.h"
#include "dev_node_exception.h"

static int getrusage_thread(struct rusage *rusage) {
#ifdef __MACH__
    int ret = -1;
    thread_basic_info_data_t info = { 0 };
    mach_msg_type_number_t info_count = THREAD_BASIC_INFO_COUNT;
    kern_return_t kern_err;

    mach_port_t port = mach_thread_self();
    kern_err = thread_info(port,
                           THREAD_BASIC_INFO,
                           (thread_info_t)&info,
                           &info_count);
    mach_port_deallocate(mach_task_self(), port);

    if (kern_err == KERN_SUCCESS) {
        memset(rusage, 0, sizeof(struct rusage));
        rusage->ru_utime.tv_sec = info.user_time.seconds;
        rusage->ru_utime.tv_usec = info.user_time.microseconds;
        rusage->ru_stime.tv_sec = info.system_time.seconds;
        rusage->ru_stime.tv_usec = info.system_time.microseconds;
        ret = 0;
    } else {
        errno = EINVAL;
    }

    return ret;
#else
    return getrusage(RUSAGE_THREAD, &rusage);
#endif
}

Dev::Dev(std::string type, std::string name, NodeMgr* node_mgr, std::vector<std::uint16_t> nodes):
    type(std::move(type)),
    name(std::move(name)),
    node_mgr(node_mgr),
    dependent_on_nodes(std::move(nodes)) {
}

Dev::~Dev() {
    SPDLOG_TRACE("~Dev() {}", fmt::ptr(this));
}

void Dev::cpu_usage_begin() {
    getrusage_thread(&usage_start);
    total_start = std::chrono::high_resolution_clock::now();
}

void Dev::cpu_usage_end() {
    struct rusage usage_end;
    auto end = std::chrono::high_resolution_clock::now();
    getrusage_thread(&usage_end);

    user_cpu_time += double(usage_end.ru_utime.tv_sec - usage_start.ru_utime.tv_sec) + (usage_end.ru_utime.tv_usec - usage_start.ru_utime.tv_usec) / 1e6;
    system_cpu_time += double(usage_end.ru_stime.tv_sec - usage_start.ru_stime.tv_sec) + (usage_end.ru_stime.tv_usec - usage_start.ru_stime.tv_usec) / 1e6;

    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - total_start);
    total_time += double(total_duration.count()) / 1000.0;
}

void Dev::emit_dev_state(const nlohmann::json& dev_status) {
    node_mgr->emit_dev_state(name, dev_status);
}

const std::vector<std::uint16_t>& Dev::get_nodes_id() {
    return dependent_on_nodes;
}

nlohmann::json Dev::get_dev_description(const TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    std::vector<std::string> methods;

    methods.reserve(method_map.size());
    for (const auto& [k, v] : method_map) {
        methods.push_back(k);
    }

    return {{"dev_name", name},
            {"dev_type", type},
            {"dev_related_nodes", dependent_on_nodes},
            {"dev_methods", methods}};
}

nlohmann::json Dev::call_dev_method(const TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params) {
    if (params.param_map.count("method") == 0) {
        throw jsonrpcpp::InvalidParamsException("Missing 'method' parameter.", id);
    }

    std::string method = params.param_map.at("method").get<std::string>();

    if (method_map.count(method)) {
        try {
            cpu_usage_begin();
            auto tmp = method_map[method](params.param_map);
            cpu_usage_end();
            return std::move(tmp);
        }
        catch (std::exception& e) {
            cpu_usage_end();
            throw jsonrpcpp::InvalidParamsException("Method '" + method + "' execution error: " + e.what() + ".", id);
        }
    }
    else {
        throw jsonrpcpp::InvalidParamsException("Dev object does not provide '" + method + "' method.", id);
    }
}

void Dev::update_dev_state(std::uint16_t node_id, const ExtH9Frame& frame) {
    if (frame.type() == H9frame::Type::NODE_INFO || frame.type() == H9frame::Type::NODE_TURNED_ON) {
        uint16_t node_type = frame.data()[0] << 8 | frame.data()[1];
        uint16_t version_major = frame.data()[2] << 8 | frame.data()[3];
        uint16_t version_minor = frame.data()[4] << 8 | frame.data()[5];

        on_dev_info(node_id, node_type, version_major, version_minor, frame.data()[6]);
    }
    else if (frame.type() == H9frame::Type::REG_EXTERNALLY_CHANGED || frame.type() == H9frame::Type::REG_INTERNALLY_CHANGED || frame.type() == H9frame::Type::REG_VALUE_BROADCAST || frame.type() == H9frame::Type::REG_VALUE) {
        Node::regvalue_t value;
        try {
            node_mgr->get_reg_value_from_frame(node_id, frame, &value);
            on_dev_register_value(node_id, frame.data()[0], std::move(value));
        }
        catch (DevNodeException& e) {
            SPDLOG_ERROR("Cannot read register value: {}", e.what());
        }
    }
    else if (frame.type() == H9frame::Type::ERROR) {
        on_dev_error(node_id, frame.data()[0]);
    }
    else if (H9frame::Type::NODE_SPECIFIC_BULK0 <= frame.type() && frame.type() <= H9frame::Type::NODE_SPECIFIC_BULK7) {
        on_dev_bulk(node_id, frame.type(), frame.dlc(), frame.data());
    }
}

nlohmann::json Dev::get_dev_state(const std::map<std::string, nlohmann::json>& param_map) {
    return {};
}
