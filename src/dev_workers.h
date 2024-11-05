/*
 * Created by crowx on 04/11/2024.
 *
 */

#pragma once

#include "ext_h9frame.h"
class Dev;

class DevWorkers {
  public:
    void dev_init(Dev* dev);
    void update_dev_state(Dev* dev, std::uint16_t node_id, const ExtH9Frame& frame);
};
