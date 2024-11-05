/*
 * H9 project
 *
 * Created by SQ8KFH on 2020-11-08.
 *
 * Copyright (C) 2020-2024 Kamil Palkowski. All rights reserved.
 */

#ifndef H9_RAW_NODE_H
#define H9_RAW_NODE_H

#include "config.h"

#include <chrono>
#include <future>
#include <queue>
#include <set>
#include <spdlog/spdlog.h>
#include <tuple>

#include "frameobserver.h"

class NodeMgr;
class Bus;

class RawNode {
  private:
    class FramePromise {
        RawNode* const node;
        bool comparator_has_seqnum;
        H9FrameComparator comparator;
        std::queue<ExtH9Frame> frame_storage;

        std::promise<ExtH9Frame> promise;

      public:
        FramePromise(RawNode* node, H9FrameComparator comparator):
            node(node),
            comparator_has_seqnum(false),
            comparator(comparator) {}

        /// @return Return true on full match - FramePromise can be deleted
        bool on_frame(const ExtH9Frame& frame) {
            if (comparator == frame) {
                if (comparator_has_seqnum) {
                    try {
                        promise.set_value(frame);
                    } catch (std::future_error& e) {
                        SPDLOG_CRITICAL("{}", e.what());
                        throw;
                    }
                    return true;
                } else {
                    frame_storage.push(frame);
                }
            }
            return false;
        }

        void set_comparator_seqnum(uint8_t seqnum) {
            node->frame_promise_set_mtx.lock();
            comparator.set_seqnum(seqnum);
            comparator_has_seqnum = true;
            while (!frame_storage.empty()) {
                if (on_frame(frame_storage.front())) {
                    node->frame_promise_set.erase(this);
                    node->frame_promise_set_mtx.unlock();
                    return;
                }
                frame_storage.pop();
            }
            node->frame_promise_set_mtx.unlock();
        }

        std::future<ExtH9Frame> get_future() {
            return promise.get_future();
        }
    };

    Bus* const bus;

    std::mutex frame_promise_set_mtx;
    std::set<FramePromise*> frame_promise_set;

    FramePromise* create_frame_promise(H9FrameComparator comparator);
    void destroy_frame_promise(FramePromise* frame_promise);

    ssize_t bit_operation(const std::string& origin, H9frame::Type type, std::uint8_t reg, std::uint8_t bit, std::size_t length = 0, std::uint8_t* reg_after_set = nullptr);

  protected:
    NodeMgr* const node_mgr;

    const std::uint16_t _node_id;
    RawNode(NodeMgr* node_mgr, Bus* bus, std::uint16_t node_id) noexcept;

    friend NodeMgr;

  public:
    constexpr static ssize_t TIMEOUT_ERROR = -1000;
    constexpr static ssize_t MALFORMED_FRAME_ERROR = -1001;

    ~RawNode() = default;

    void on_frame_recv(const ExtH9Frame& frame);

    std::uint16_t node_id() const noexcept;

    ssize_t reset(const std::string& origin);

    int32_t get_node_type(const std::string& origin) noexcept;
    int64_t get_node_version(const std::string& origin, std::uint16_t* major = nullptr, std::uint16_t* minor = nullptr, std::uint16_t* patch = nullptr) noexcept;
    int32_t get_mcu_type(const std::string& origin) noexcept;

    void firmware_update(const std::string& origin, void (*progress_callback)(int percentage));

    ssize_t set_bit(const std::string& origin, std::uint8_t reg, std::uint8_t bit, std::size_t length = 0, std::uint8_t* reg_after_set = nullptr);
    ssize_t clear_bit(const std::string& origin, std::uint8_t reg, std::uint8_t bit, std::size_t length = 0, std::uint8_t* reg_after_set = nullptr);
    ssize_t toggle_bit(const std::string& origin, std::uint8_t reg, std::uint8_t bit, std::size_t length = 0, std::uint8_t* reg_after_set = nullptr);

    ssize_t set_reg(const std::string& origin, std::uint8_t reg, std::size_t length, const std::uint8_t* reg_val, std::uint8_t* reg_after_set = nullptr, ssize_t reg_after_set_length = -1);
    ssize_t set_reg(const std::string& origin, std::uint8_t reg, std::uint8_t reg_val, std::uint8_t* reg_after_set = nullptr);
    ssize_t set_reg(const std::string& origin, std::uint8_t reg, std::uint16_t reg_val, std::uint16_t* reg_after_set = nullptr);
    ssize_t set_reg(const std::string& origin, std::uint8_t reg, std::uint32_t reg_val, std::uint32_t* reg_after_set = nullptr);

    /// Read registry from the node
    /// @param[in] origin client idstring
    /// @param[in] reg number to the read
    /// @param[in] nbyte size of reg_val
    /// @param[out] reg_val
    /// @retval >=0  Number of read byte
    /// @retval TIMEOUT_ERROR on timeout
    /// @retval -H9Frame::Error on node error (received ERROR frame)
    ssize_t get_reg(const std::string& origin, std::uint8_t reg, std::size_t length, std::uint8_t* reg_val);
    ssize_t get_reg(const std::string& origin, std::uint8_t reg, std::uint8_t* reg_val);
    ssize_t get_reg(const std::string& origin, std::uint8_t reg, std::uint16_t* reg_val);
    ssize_t get_reg(const std::string& origin, std::uint8_t reg, std::uint32_t* reg_val);
};

#endif // H9_RAW_NODE_H
