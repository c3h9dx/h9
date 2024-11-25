/*
 * Created by crowx on 04/11/2024.
 *
 */

#pragma once

#include "ext_h9frame.h"
#include <queue>
#include <mutex>
#include <semaphore>
#include <thread>


class Dev;

class DevWorkers {
    struct Task {
        enum {
            UPDATE_TASK,
            PERIODIC_TASK
        } task_type;
        Dev* dev;
        std::uint16_t node_id;
        ExtH9Frame frame;
    };

    bool _run;

    std::counting_semaphore<USHRT_MAX> sema{0};

    std::mutex task_queue_mtx;
    std::queue<Task> task_queue;

    void worker();
  public:
    DevWorkers();
    void create_devs_workers(int workers);
    void dev_periodic_task(Dev* dev);
    void update_dev_state(Dev* dev, std::uint16_t node_id, const ExtH9Frame& frame);
};
