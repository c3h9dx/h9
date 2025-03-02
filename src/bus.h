/*
 * H9 project
 *
 * Created by crowx on 2023-09-06.
 *
 * Copyright (C) 2023 Kamil Palkowski. All rights reserved.
 */

#pragma once

#include "config.h"

#include <atomic>
#include <list>
#include <map>
#include <queue>
#include <spdlog/spdlog.h>
#include <thread>

#include "bus_driver.h"
#include "frameobserver.h"
#include "framesubject.h"
#include "h9framecomparator.h"
#include "metrics_collector.h"
#if (defined(__unix__) && defined(BSD)) || defined(__APPLE__) && defined(__MACH__)
#include "kqueue.h"
#elif defined(__linux__)
#include "epoll.h"
#endif

class Bus: public FrameSubject {
  private:
#if (defined(__unix__) && defined(BSD)) || (defined(__APPLE__) && defined(__MACH__))
    using IOEventQueue = KQueue;
#else
    using IOEventQueue = Epoll;
#endif

    struct BusFrameLess {
        bool operator()(std::shared_ptr<BusFrame> x, std::shared_ptr<BusFrame> y) const {
            return x->priority() > y->priority(); // HIGH = 0, LOW = 1
        }
    };

    std::shared_ptr<spdlog::logger> logger;
    std::shared_ptr<spdlog::logger> frames_logger;
    std::shared_ptr<spdlog::logger> frames_recv_file_logger;
    std::shared_ptr<spdlog::logger> frames_sent_file_logger;

    std::uint8_t next_seqnum;
    std::uint16_t _bus_id;

    bool _forwarding;

    std::atomic_bool run;
    std::map<int, BusDriver*> bus;

    IOEventQueue event_notificator;
    std::thread recv_thread_desc;

    std::priority_queue<std::shared_ptr<BusFrame>, std::vector<std::shared_ptr<BusFrame>>, BusFrameLess> forward_queue;
    std::priority_queue<std::shared_ptr<BusFrame>, std::vector<std::shared_ptr<BusFrame>>, BusFrameLess> send_queue;
    std::mutex send_queue_mtx;

    MetricsCollector::counter_t& sent_frames_counter;
    MetricsCollector::counter_t& received_frames_counter;
    MetricsCollector::counter_t& forward_frames_counter;
    // MetricsCollector::counter_t& size_of_send_queue;

    constexpr static int number_of_frame_types = 1 << H9frame::H9FRAME_TYPE_BIT_LENGTH;
    MetricsCollector::counter_t* sent_frames_counter_by_type[number_of_frame_types];
    MetricsCollector::counter_t* received_frames_counter_by_type[number_of_frame_types];
    bool recv_thread_send();
    bool recv_thread_forward();
    void recv_thread();
  public:
    Bus();
    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;
    ~Bus();

    void bus_default_source_id(std::uint16_t bus_id);
    void forwarding(bool v);

    void add_driver(BusDriver* bus_driver);
    int endpoint_count();
    void activate();

    /// Send frame to the bus.
    /// @param[in] frame to sent
    /// @param[in] raw is true Bus don't set source_id and seqnum
    /// @return seqnum of the sent frame
    int send_frame(ExtH9Frame frame, bool raw = false);
    std::future<SendFrameResult> send_frame_noblock(ExtH9Frame frame, bool raw = false);
};
