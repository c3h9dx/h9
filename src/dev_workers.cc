/*
 * Created by crowx on 04/11/2024.
 *
 */

#include "dev_workers.h"
#include "dev.h"

void DevWorkers::dev_init(Dev* dev) {
    dev->init();
}

void DevWorkers::update_dev_state(Dev* dev, std::uint16_t node_id, const ExtH9Frame& frame) {
    dev->update_dev_state(node_id, frame);
}
