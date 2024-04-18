/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-22.
 *
 * Copyright (C) 2020-2023 Kamil Palkowski. All rights reserved.
 */

#include "epoll.h"

#include <sys/eventfd.h>
#include <system_error>
#include <unistd.h>
#include <spdlog/spdlog.h>

Epoll::Epoll():
    epoll(-1),
    event_fd(-1) {
    epoll = epoll_create1(0);
    if (epoll == -1) {
        throw std::system_error(errno, std::generic_category(), __FILE__ + std::string(":") + std::to_string(__LINE__));
    }

    event_fd = eventfd(0, EFD_NONBLOCK);
    struct epoll_event event = {0};
    event.data.fd = event_fd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll, EPOLL_CTL_ADD, event_fd, &event) == -1) {
        throw std::system_error(errno, std::generic_category(), __FILE__ + std::string(":") + std::to_string(__LINE__));
    }
}

Epoll::~Epoll() {
    if (epoll > -1) {
        close(epoll);
    }
    if (event_fd > -1) {
        close(event_fd);
    }
}

void Epoll::attach_socket(int fd) {
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = fd;

    if (epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &event)) {
        throw std::system_error(errno, std::generic_category(), __FILE__ + std::string(":") + std::to_string(__LINE__));
    }
}

int Epoll::wait() {
    int ret;
    do {
        ret = epoll_wait(epoll, tevent, event_queue_size, -1);
        if (ret < 0 && errno == EINTR) {
            SPDLOG_WARN("epoll_wait: Interrupted system call");
        }
    } while (ret < 0 && errno == EINTR);

    if (ret == -1) {
        throw std::system_error(errno, std::generic_category(), __FILE__ + std::string(":") + std::to_string(__LINE__));
    }
    return ret;
}

void Epoll::trigger_async_event() {
    if (eventfd_write(event_fd, 1) < 0) {
        throw std::system_error(errno, std::generic_category(), __FILE__ + std::string(":") + std::to_string(__LINE__));
    }
}

bool Epoll::is_socket_event(int number_of_events, int fd) {
    for (int i = 0; i < number_of_events; ++i) {
        if (fd == (int)tevent[i].data.fd)
            return true;
    }
    return false;
}

bool Epoll::is_async_event(int number_of_events) {
    for (int i = 0; i < number_of_events; ++i) {
        if (event_fd == (int)tevent[i].data.fd)
            return true;
    }
    return false;
}
