/*
 * Created by crowx on 29/10/2023.
 *
 */

#include "dev_status_observer.h"

#include "node_mgr.h"
#include "tcpclientthread.h"

DevStatusObserver::DevStatusObserver(TCPClientThread* tcp_client_thread, NodeMgr* mgr):
    client(tcp_client_thread),
    mgr(mgr) {
    mgr->attach_dev_state_observer("*", this);
}

void DevStatusObserver::detach() {
    mgr->detach_dev_state_observer("*", this);
    SPDLOG_TRACE("DevStatusObserver::detach(this={}) [NodeDevMgr={}]", fmt::ptr(this), fmt::ptr(mgr));
    mgr = nullptr;
}

DevStatusObserver::~DevStatusObserver() {
    assert(mgr == nullptr);
    SPDLOG_TRACE("~DevStatusObserver(this={})", fmt::ptr(this));
}

void DevStatusObserver::on_dev_state_update(const std::string &dev, const nlohmann::json& dev_status) {
    jsonrpcpp::Notification n("dev_status_update", nlohmann::json({{"dev", dev},{"status", dev_status}}));
    client->send_msg(std::make_shared<jsonrpcpp::Notification>(std::move(n)));
}
