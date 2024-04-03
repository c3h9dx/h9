/*
 * Created by crowx on 29/10/2023.
 *
 */

#pragma once

#include <nlohmann/json.hpp>

class TCPClientThread;
class NodeDevMgr;

class DevStatusObserver {
  private:
    TCPClientThread* client;
    NodeDevMgr* mgr;
  public:
    DevStatusObserver(TCPClientThread* tcp_client_thread, NodeDevMgr* mgr);
    void detach();
    ~DevStatusObserver();
    void on_dev_state_update(const nlohmann::json& dev_status);
};
