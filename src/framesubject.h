/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-23.
 *
 * Copyright (C) 2020-2023 Kamil Palkowski. All rights reserved.
 */

#pragma once

#include "config.h"

#include <list>
#include <map>
#include <mutex>

#include "frameobserver.h"
#include "h9framecomparator.h"

class FrameSubject {
  private:
    std::recursive_mutex frame_observers_mtx;
    std::map<H9FrameComparator, std::list<FrameObserver*>> frame_observers;

    friend FrameObserver::FrameObserver(FrameSubject* subject, H9FrameComparator comparator);
    friend void FrameObserver::detach();

    void attach_frame_observer(FrameObserver* observer, H9FrameComparator comparator);
    void detach_frame_observer(FrameObserver* observer);

  protected:
    void notify_frame_recv_observer(const ExtH9Frame& frame);
    void notify_frame_send_observer(const ExtH9Frame& frame);
};
