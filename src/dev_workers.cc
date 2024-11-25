/*
 * Created by crowx on 04/11/2024.
 *
 */

#include "dev_workers.h"

#include "dev.h"

void DevWorkers::worker() {
    while (_run) {
        sema.acquire();
        task_queue_mtx.lock();
        auto task = task_queue.front();
        task_queue.pop();
        task_queue_mtx.unlock();

        task.dev->cpu_usage_begin();

        if (task.task_type == Task::PERIODIC_TASK) {
            task.dev->periodic_task();
        } else if (task.task_type == Task::UPDATE_TASK) {
            task.dev->update_dev_state(task.node_id, task.frame);
        }

        task.dev->cpu_usage_end();
    }
}

DevWorkers::DevWorkers(): _run(true) {

}

void DevWorkers::create_devs_workers(int workers) {
    for (int i = 0; i < 5; ++i) {
        new std::thread(&DevWorkers::worker, this);
    }
}

void DevWorkers::dev_periodic_task(Dev* dev) {
    task_queue_mtx.lock();
    task_queue.push({.task_type = Task::PERIODIC_TASK, .dev = dev});
    task_queue_mtx.unlock();
    sema.release();
}

void DevWorkers::update_dev_state(Dev* dev, std::uint16_t node_id, const ExtH9Frame& frame) {
    task_queue_mtx.lock();
    task_queue.push({.task_type = Task::UPDATE_TASK, .dev = dev, .node_id = node_id, .frame = frame});
    task_queue_mtx.unlock();
    sema.release();
}
