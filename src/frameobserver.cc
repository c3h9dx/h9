/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-11.
 *
 * Copyright (C) 2020 Kamil Palkowski. All rights reserved.
 */

#include "frameobserver.h"

#include "framesubject.h"
#include <spdlog/spdlog.h>

FrameObserver::FrameObserver(FrameSubject* subject, H9FrameComparator comparator):
    subject(subject) {
    subject->attach_frame_observer(this, comparator);
}

FrameObserver::~FrameObserver() {
    subject->detach_frame_observer(this);

    SPDLOG_TRACE("~FrameObserver() [FrameSubject={}, this={}]", fmt::ptr(subject), fmt::ptr(this));
}
