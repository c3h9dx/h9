/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-11.
 *
 * Copyright (C) 2020-2023 Kamil Palkowski. All rights reserved.
 */

#pragma once

#include "config.h"

#include "ext_h9frame.h"
#include "h9framecomparator.h"

class FrameSubject;

class FrameObserver {
  private:
    FrameSubject* subject;
  protected:
    friend class FrameSubject;

    FrameObserver(FrameSubject* subject, H9FrameComparator comparator);
    FrameObserver(const FrameObserver&) = delete;
    ~FrameObserver();

    virtual void on_frame_recv(const ExtH9Frame& frame) = 0;
    virtual void on_frame_send(const ExtH9Frame& frame) = 0;

  public:
    void detach();
};
