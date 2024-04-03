/*
 * H9 project
 *
 * Created by crowx on 2023-09-12.
 *
 * Copyright (C) 2023 Kamil Palkowski. All rights reserved.
 */

#include "client_frame_obs.h"

#include <jsonrpcpp/jsonrpcpp.hpp>

#include "tcpclientthread.h"

void ClientFrameObs::on_frame_recv(const ExtH9Frame& frame) {
    jsonrpcpp::Notification n("on_frame", nlohmann::json({{"frame", frame}}));
    client->send_msg(std::make_shared<jsonrpcpp::Notification>(std::move(n)));
}

void ClientFrameObs::on_frame_send(const ExtH9Frame& frame) {
    jsonrpcpp::Notification n("on_frame", nlohmann::json({{"frame", frame}}));
    client->send_msg(std::make_shared<jsonrpcpp::Notification>(std::move(n)));
}

ClientFrameObs::ClientFrameObs(TCPClientThread* tcp_client_thread, FrameSubject* subject, H9FrameComparator comparator):
    FrameObserver(subject, comparator),
    client(tcp_client_thread) {
    SPDLOG_TRACE("FrameObserver(TCPClientThread={}, FrameSubject={}) [this={}]", fmt::ptr(tcp_client_thread), fmt::ptr(subject), fmt::ptr(this));
}
