/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-19.
 *
 * Copyright (C) 2020-2023 Kamil Palkowski. All rights reserved.
 */

#include "tcpclientthread.h"

#include <cassert>
#include <memory>
#include <spdlog/spdlog.h>
#include <sys/errno.h>

#include "h9d_configurator.h"
#include "dev_status_observer.h"

void TCPClientThread::thread() {
    thread_running = true;
    int read_fd = h9socket.get_socket();

    event.attach_socket(read_fd);
    // try {
    while (running) {
        int n = event.wait();
        if (event.is_socket_event(n, read_fd))
            thread_recv_msg();
        else if (event.is_async_event(n))
            thread_send_async_msg();
    }
    //}
    // catch (...) {
    //    execadapter.cleanup_connection();
    //    thread_running = false;
    //    throw;
    //}
    // execadapter.cleanup_connection();
    SPDLOG_LOGGER_TRACE(logger, "Client ({}) thread finish.", get_client_idstring());
    thread_running = false;

    if (_frame_observer) _frame_observer->detach();
    if (_dev_status_observer) _dev_status_observer->detach();

    server->cleanup_tcpclientthread(this);
}

void TCPClientThread::thread_recv_msg() {
    nlohmann::json json;

    int res = h9socket.recv_complete_msg(json);
    if (res <= 0) {
        if (res == 0 || errno == ECONNRESET) { // Connection reset by peer
            SPDLOG_LOGGER_WARN(logger, "Connection reset by peer {}.", get_client_idstring().c_str());
            running = false;
            h9socket.close();
            return;
        }
        throw std::system_error(errno, std::generic_category(), __FILE__ + std::string(":") + std::to_string(__LINE__));
    }

    if (json.is_discarded()) {
        SPDLOG_LOGGER_ERROR(logger, "Recv invalid JSON from client: {}.", get_client_idstring().c_str());

        h9socket.send(jsonrpcpp::ParseErrorException("").to_json());
        return;
    }
    else {
        SPDLOG_LOGGER_TRACE(logger, "Recv JSON from client: {}: {}.", get_client_idstring().c_str(), json.dump());
    }
    jsonrpcpp::Parser parser;
    jsonrpcpp::entity_ptr msg;
    try {
        msg = parser.parse_json(json);
    }
    catch (const jsonrpcpp::RpcException& e) {
        SPDLOG_LOGGER_ERROR(logger, "Recv invalid JSONRPC from client: {}: {}.", get_client_idstring().c_str(), e.what());
        h9socket.send(jsonrpcpp::ParseErrorException(e.what()).to_json());
        return;
    }

    if (msg && msg->is_request()) {
        jsonrpcpp::request_ptr request = std::dynamic_pointer_cast<jsonrpcpp::Request>(msg);

        SPDLOG_LOGGER_DEBUG(logger, "Recv request (id: {}) - execute method '{}' from client {}", request->id().int_id(), request->method(), get_client_idstring().c_str());

        try {
            jsonrpcpp::Response response = api->call(this, request);
            h9socket.send(response.to_json());
            SPDLOG_LOGGER_DEBUG(logger, "Sent response (id: {}) - method '{}' to client {}", response.id().int_id(), request->method(), get_client_idstring().c_str());
        }
        catch (const jsonrpcpp::RequestException& e) {
            h9socket.send(e.to_json());
            SPDLOG_LOGGER_WARN(logger, "Sent error response (id: {}) - method '{}' to client {}: {}", e.id().int_id(), request->method(), get_client_idstring().c_str(), e.what());
        }
    }
    else if (msg && msg->is_batch()) {
        jsonrpcpp::batch_ptr batch = std::dynamic_pointer_cast<jsonrpcpp::Batch>(msg);
        SPDLOG_LOGGER_DEBUG(logger, "Recv batch from client {}", get_client_idstring().c_str());

        jsonrpcpp::Batch response_batch;
        for (const auto& batch_entity : batch->entities) {
            if (batch_entity->is_request()) {
                jsonrpcpp::request_ptr request = std::dynamic_pointer_cast<jsonrpcpp::Request>(batch_entity);
                try {
                    jsonrpcpp::Response response = api->call(this, request);
                    response_batch.add(response);
                    h9socket.send(response.to_json());
                }
                catch (const jsonrpcpp::RequestException& e) {
                    response_batch.add(e);
                }
            }
        }

        if (!response_batch.entities.empty()) {
            h9socket.send(response_batch.to_json());
        }
    }
    else {
        SPDLOG_LOGGER_ERROR(logger, "Recv not supported JSON-RPC object from client: {}", get_client_idstring().c_str());
        SPDLOG_LOGGER_DEBUG(logger, "Dump not supported JSON-RPC object: {}.", json.dump());

        h9socket.send(jsonrpcpp::ParseErrorException("The JSON sent is not supported object.").to_json());
    }
}

void TCPClientThread::thread_send_async_msg() {
    bool queue_empty = false;
    do {
        async_msg_queue_mtx.lock();
        SPDLOG_LOGGER_TRACE(logger,"thread_send_async_msg, queue size: {}", async_msg_queue.size());

        assert(!async_msg_queue.empty());

        jsonrpcpp::entity_ptr msg = async_msg_queue.front();
        async_msg_queue.pop();

        if (async_msg_queue.empty())
            queue_empty = true;
        async_msg_queue_mtx.unlock();

        send(std::move(msg));
    } while (!queue_empty);
}

void TCPClientThread::send(jsonrpcpp::entity_ptr msg) {
    int ret = h9socket.send(msg->to_json());
    if (ret <= 0) {
        if (ret == 0) {
            running = false;
            return;
        }
        throw std::system_error(errno, std::generic_category(), __FILE__ + std::string(":") + std::to_string(__LINE__));
    }
}

TCPClientThread::TCPClientThread(int sockfd, API* api, TCPServer* server):
    connection_time(std::time(nullptr)),
    h9socket(sockfd),
    api(api),
    server(server),
    _frame_observer(nullptr),
    _dev_status_observer(nullptr) {
    logger = spdlog::get(H9dConfigurator::tcp_logger_name);

    running = true;
    client_thread_desc = std::thread([this]() {
        this->thread();
    });
}

TCPClientThread::~TCPClientThread() {
    SPDLOG_LOGGER_DEBUG(logger, "Cleaning client ({}) data... [this={}]", get_client_idstring(), fmt::ptr(this));
    delete _frame_observer;
    delete _dev_status_observer;

    close_connection();
    if (client_thread_desc.joinable())
        client_thread_desc.join();
    h9socket.close();
    SPDLOG_LOGGER_TRACE(logger, "~TCPClientThread() [this={}]", fmt::ptr(this));
}

void TCPClientThread::set_frame_observer(ClientFrameObs* frame_observer) {
    delete _frame_observer;
    _frame_observer = frame_observer;
}

bool TCPClientThread::is_frame_observer_set() const {
    return _frame_observer != nullptr;
}

void TCPClientThread::set_dev_status_observer(DevStatusObserver* dev_status_observer) {
    delete _dev_status_observer;
    _dev_status_observer = dev_status_observer;
}

bool TCPClientThread::is_dev_status_observer_set() const {
    return _dev_status_observer != nullptr;
}

std::string TCPClientThread::get_remote_address() const noexcept {
    return h9socket.get_remote_address();
}

std::string TCPClientThread::get_remote_port() const noexcept {
    return h9socket.get_remote_port();
}

std::string TCPClientThread::entity() const noexcept {
    return _entity;
}

void TCPClientThread::entity(const std::string& entity) noexcept {
    _entity = entity;
}

bool TCPClientThread::authenticated() const noexcept {
    return _authenticated;
}

void TCPClientThread::authenticate(const std::string& entity) noexcept {
    this->entity(entity);
    _authenticated = true;
}

std::string TCPClientThread::get_client_idstring() const noexcept {
    return entity() + "@" + get_remote_address() + ":" + get_remote_port();
}

bool TCPClientThread::is_running() {
    return thread_running;
}

void TCPClientThread::close_connection() {
    running = false;
    h9socket.shutdown_read();
}

void TCPClientThread::send_msg(jsonrpcpp::entity_ptr msg) {
    async_msg_queue_mtx.lock();
    async_msg_queue.push(std::move(msg));
    if (async_msg_queue.size() == 1) {
        event.trigger_async_event();
    }
    async_msg_queue_mtx.unlock();
}
