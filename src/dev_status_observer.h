/*
 * Created by crowx on 29/10/2023.
 *
 */

#pragma once

#include <nlohmann/json.hpp>

class TCPClientThread;
class NodeMgr;

class DevStatusObserver {
  private:
    TCPClientThread* client;
    NodeMgr* mgr;
  public:
    DevStatusObserver(TCPClientThread* tcp_client_thread, NodeMgr* mgr);
    void detach();
    ~DevStatusObserver();
    void on_dev_state_update(const std::string &dev, const nlohmann::json& dev_status);
};
