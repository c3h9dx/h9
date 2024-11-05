/*
 * H9 project
 *
 * Created by SQ8KFH on 2024-04-19.
 *
 * Copyright (C) 2024 Kamil Palkowski. All rights reserved.
 */

#include "udp_driver.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>

struct can_frame {
    std::uint32_t can_id;
    std::uint8_t can_dlc;
    std::uint8_t data[8];

};

UDPDriver::UDPDriver(const std::string& name, std::string local_port, std::string remote_addr, std::string remote_port):
    BusDriver(name, "udp"),
    remote(nullptr),
    local_port(std::move(local_port)),
    remote_addr(std::move(remote_addr)),
    remote_port(std::move(remote_port)) {
}

UDPDriver::~UDPDriver() {
    if (remote) {
        freeaddrinfo(remote);
    }
}

int UDPDriver::open() {
    addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET6 to use IPv6
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    int ret;

    if ((ret = getaddrinfo(nullptr, local_port.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        throw std::system_error(errno, std::generic_category(), __FILE__ + std::string(":") + std::to_string(__LINE__));
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != nullptr; p = p->ai_next) {
        if ((socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
            ::close(socket_fd);
            perror("listener: bind");
            continue;
        }
        break;
    }

    if (p == nullptr) {
        throw std::system_error(errno, std::generic_category(), __FILE__ + std::string(":") + std::to_string(__LINE__));
    }

    freeaddrinfo(servinfo);

    hints.ai_flags = 0;

    if ((ret = getaddrinfo(remote_addr.c_str(), remote_port.c_str(), &hints, &remote)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        throw std::system_error(errno, std::generic_category(), __FILE__ + std::string(":") + std::to_string(__LINE__));
    }

    return socket_fd;
}

int UDPDriver::recv_data(H9frame* frame) {
    sockaddr_storage remote_addr;
    socklen_t len = sizeof(remote_addr);

    can_frame can_msg;

    ssize_t ret = recvfrom(socket_fd, &can_msg, sizeof(can_msg), 0, (struct sockaddr*)&remote_addr, &len);
    if (ret == -1) {
        throw std::system_error(errno, std::generic_category(), __FILE__ + std::string(":") + std::to_string(__LINE__));
    }

    can_msg.can_id = ntohl(can_msg.can_id);

    frame->priority = H9frame::from_underlying<H9frame::Priority>((can_msg.can_id >> (H9frame::H9FRAME_TYPE_BIT_LENGTH + H9frame::H9FRAME_SEQNUM_BIT_LENGTH +
                                                                                   H9frame::H9FRAME_DESTINATION_ID_BIT_LENGTH + H9frame::H9FRAME_SOURCE_ID_BIT_LENGTH)) &
                                                               ((1 << H9frame::H9FRAME_PRIORITY_BIT_LENGTH) - 1));

    frame->type = H9frame::from_underlying<H9frame::Type>((can_msg.can_id >> (H9frame::H9FRAME_SEQNUM_BIT_LENGTH + H9frame::H9FRAME_DESTINATION_ID_BIT_LENGTH + H9frame::H9FRAME_SOURCE_ID_BIT_LENGTH)) & ((1 << H9frame::H9FRAME_TYPE_BIT_LENGTH) - 1));

    frame->seqnum = static_cast<std::uint8_t>((can_msg.can_id >> (H9frame::H9FRAME_DESTINATION_ID_BIT_LENGTH + H9frame::H9FRAME_SOURCE_ID_BIT_LENGTH)) & ((1 << H9frame::H9FRAME_SEQNUM_BIT_LENGTH) - 1));

    frame->destination_id = static_cast<std::uint16_t>((can_msg.can_id >> (H9frame::H9FRAME_SOURCE_ID_BIT_LENGTH)) & ((1 << H9frame::H9FRAME_DESTINATION_ID_BIT_LENGTH) - 1));

    frame->source_id = static_cast<std::uint16_t>((can_msg.can_id >> (0)) & ((1 << H9frame::H9FRAME_SOURCE_ID_BIT_LENGTH) - 1));

    frame->dlc = can_msg.can_dlc;
    for (int i = 0; i < 8; i++) {
        frame->data[i] = can_msg.data[i];
    }

    return ret != 0 ? RECV_FRAME : SOCKET_CLOSE;
}

int UDPDriver::send_data(std::shared_ptr<BusFrame> busframe) {
    can_frame can_msg;
    memset(&can_msg, 0, sizeof(struct can_frame));

    can_msg.can_id |= H9frame::to_underlying(busframe->priority()) & ((1 << H9frame::H9FRAME_PRIORITY_BIT_LENGTH) - 1);
    can_msg.can_id <<= H9frame::H9FRAME_TYPE_BIT_LENGTH;
    can_msg.can_id |= H9frame::to_underlying(busframe->type()) & ((1 << H9frame::H9FRAME_TYPE_BIT_LENGTH) - 1);
    can_msg.can_id <<= H9frame::H9FRAME_SEQNUM_BIT_LENGTH;
    can_msg.can_id |= busframe->seqnum() & ((1 << H9frame::H9FRAME_SEQNUM_BIT_LENGTH) - 1);
    can_msg.can_id <<= H9frame::H9FRAME_DESTINATION_ID_BIT_LENGTH;
    can_msg.can_id |= busframe->destination_id() & ((1 << H9frame::H9FRAME_DESTINATION_ID_BIT_LENGTH) - 1);
    can_msg.can_id <<= H9frame::H9FRAME_SOURCE_ID_BIT_LENGTH;
    can_msg.can_id |= busframe->source_id() & ((1 << H9frame::H9FRAME_SOURCE_ID_BIT_LENGTH) - 1);

    can_msg.can_id = htonl(can_msg.can_id);

    can_msg.can_id |= 0x80000000U; //CAN_EFF_FLAG;

    can_msg.can_dlc = busframe->dlc();
    for (int i = 0; i < 8; i++) {
        can_msg.data[i] = busframe->data()[i];
    }

    ssize_t ret = sendto(socket_fd, &can_msg, sizeof(can_msg), 0, remote->ai_addr, remote->ai_addrlen);

    if (ret == -1) {
        throw std::system_error(errno, std::generic_category(), __FILE__ + std::string(":") + std::to_string(__LINE__));
    }
    else {
        frame_sent_correctly(busframe);
    }

    return ret;
}
