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

void FrameObserver::detach() {
    subject->detach_frame_observer(this);
    SPDLOG_TRACE("FrameObserver::detach(this={}) [FrameSubject={}]", fmt::ptr(this), fmt::ptr(subject));
    subject = nullptr;
}

FrameObserver::~FrameObserver() {
    assert(subject == nullptr);
    SPDLOG_TRACE("~FrameObserver(this={})", fmt::ptr(this));
}
