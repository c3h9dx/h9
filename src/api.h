/*
 * H9 project
 *
 * Created by crowx on 2023-09-11.
 *
 * Copyright (C) 2023 Kamil Palkowski. All rights reserved.
 */

#pragma once

#include <jsonrpcpp/jsonrpcpp.hpp>
#include <map>
#include <nlohmann/json.hpp>

class Bus;
class NodeMgr;
class TCPServer;
class TCPClientThread;

class API {
  public:
    constexpr static int EXECUTION_TIMEOUT = -10;
    constexpr static int NODE_IS_NOT_EXIST = -11;
    constexpr static int DEV_IS_NOT_EXIST = -12;
    constexpr static int MALFORMED_FRAME = -13;
    constexpr static int FRAME_SIZE_MISMATCH = -14;
    constexpr static int REGISTER_IS_NOT_EXIST = -15;
    constexpr static int REGISTER_IS_NOT_WRITABLE = -16;
    constexpr static int REGISTER_IS_NOT_READABLE = -17;
    constexpr static int UNSUPPORTED_REGISTER_DATA_CONVERSION = -18;

    /*
      -1000 - -1255 - reserved for error origin directly from node (CAN error frame)
    */

    //"Internal error", -32603
    //"Invalid params", -32602
    //"Method not found", -32601
    //"Invalid request", -32600
    //"Parse error", -32700
  private:
    using api_method = nlohmann::json (API::*)(TCPClientThread* client_thread, const jsonrpcpp::Id&, const jsonrpcpp::Parameter&);

    Bus* const bus;
    NodeMgr* const node_dev_mgr;
    TCPServer* tcp_server;

    std::map<std::string, api_method> api_methods;

    nlohmann::json get_version(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json get_methods_list(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json subscribe(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json get_tcp_clients(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json send_frame(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json get_stats(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json authenticate(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json reload_nodes_description(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json get_nodes_list(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json get_node_info(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json discover_nodes(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json node_reset(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json get_registers_list(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json get_register_value(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json set_register_value(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json set_register_bit(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json clear_register_bit(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json toggle_register_bit(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json get_devs_list(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json get_dev_description(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json get_dev_status(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
    nlohmann::json dev_method_call(TCPClientThread* client_thread, const jsonrpcpp::Id& id, const jsonrpcpp::Parameter& params);
  public:
    API(Bus* bus, NodeMgr* dev_mgr);
    void set_tcp_server(TCPServer* tcp_server);
    jsonrpcpp::Response call(TCPClientThread* client_thread, const jsonrpcpp::request_ptr& request);
};
