#ifndef SHAREDCPP_WS_VIDEO_EDITOR_TIME_MESSAGE_CENTER_H
#define SHAREDCPP_WS_VIDEO_EDITOR_TIME_MESSAGE_CENTER_H

#include <mutex>
#include <thread>
#include "av_utils.h"
#include "base/blocking_queue.h"
#include "platform_logger.h"

namespace whensunset {
    namespace wsvideoeditor {

        class TimeMessageCenter {
        public:
            explicit TimeMessageCenter(int capacity = 2)
                    : message_queue_(capacity) {
                capacity_ = capacity;
                message_consumer_thread_ = std::thread(&TimeMessageCenter::MsgConsumerThread, this);
            }

            ~TimeMessageCenter() {
                {
                    std::lock_guard<std::mutex> lk(mutex_);
                    released_ = true;
                }
                cond_.notify_all();
                if (message_consumer_thread_.joinable()) {
                    message_consumer_thread_.join();
                }
            }

            void SetProcessFunction(std::function<void(double)> msg_process_function) {
                std::lock_guard<std::mutex> lk(mutex_);
                message_process_function_ = msg_process_function;
            }

            void AddMessage(double pts) {
                {
                    std::unique_lock<std::mutex> lk(mutex_);
                    if (message_queue_.Size() == capacity_) {
                        message_queue_.PopFront();
                    }
                    message_queue_.PushBack(pts);
                }
                cond_.notify_all();
            }


        private:
            int capacity_;
            std::mutex mutex_;
            std::thread message_consumer_thread_;
            std::condition_variable cond_;
            bool released_ = false;
            std::function<void(double)> message_process_function_;

            base::BlockingQueue<double> message_queue_;

            void MsgConsumerThread() {
                SetCurrentThreadName("EditorMessageConsumer");
                while (true) {
                    double pts = 0.0;
                    {
                        std::unique_lock<std::mutex> lk(mutex_);
                        cond_.wait(lk, [&] { return !message_queue_.Empty() || released_; });
                        if (released_) {
                            break;
                        }
                        pts = message_queue_.PopFront();
                    }
                    message_process_function_(pts);
                }
            }
        };
    }
}
#endif
