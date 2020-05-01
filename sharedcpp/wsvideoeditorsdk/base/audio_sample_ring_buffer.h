#ifndef SHAREDCPP_WS_VIDEO_EDITOR_AUDIOSAMPLERINGBUFFER_H
#define SHAREDCPP_WS_VIDEO_EDITOR_AUDIOSAMPLERINGBUFFER_H

#include <assert.h>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace whensunset {
    namespace base {

        template<typename T>
        class AudioSampleRingBuffer {
        public:

            AudioSampleRingBuffer(int capacity) : capacity_(capacity) {
                assert(capacity_ > 0);
                data_buffer_ = std::unique_ptr<T[]>(new(std::nothrow) T[capacity_]);
                ts_buffer_ = std::unique_ptr<double[]>(new(std::nothrow) double[capacity_]);
                if (!data_buffer_ || !ts_buffer_) {
                    LOGE("OOM in AudioSampleRingBuffer constructor!!!");
                    abort();  // If OOM here, the App is in a hopeless state, just abort early, do not let errors propagate
                }
                head_ = 0;
                size_ = 0;
                not_full_cond_.notify_all();
            }

            virtual ~AudioSampleRingBuffer() {
                std::unique_lock<std::mutex> lock(mutex_);
                data_buffer_.reset();
                ts_buffer_.reset();
            }

            int size() {
                std::unique_lock<std::mutex> lock(mutex_);
                return size_;
            }

            void Clear() {
                std::unique_lock<std::mutex> lock(mutex_);
                head_ = 0;
                size_ = 0;

                not_full_cond_.notify_all();
            }

            void Release() {
                std::unique_lock<std::mutex> lock(mutex_);
                head_ = 0;
                size_ = 0;
                is_released_ = true;
                not_full_cond_.notify_all();
                not_empty_cond_.notify_all();
            }

            void ReOpen() {
                std::unique_lock<std::mutex> lock(mutex_);
                head_ = 0;
                size_ = 0;
                is_released_ = false;
                not_full_cond_.notify_all();
                not_empty_cond_.notify_all();
            }

            void Put(T data[], int length, double pos) {
                assert(length % bytes_per_sample_ == 0);
                std::unique_lock<std::mutex> lock(mutex_);
                not_full_cond_.wait(lock,
                                    [&] { return size_ + length <= capacity_ || is_released_; });
                if (is_released_) {
                    return;
                }

                int tail = (head_ + size_) % capacity_;
                for (int i = 0; i < length; ++i) {
                    data_buffer_[tail % capacity_] = data[i];
                    ts_buffer_[tail % capacity_] =
                            pos + (i / bytes_per_sample_) / (double) audio_sample_rate_;
                    tail++;
                }
                size_ += length;

                not_empty_cond_.notify_all();
            }

            int Get(T data[], int length, double *pos, bool blocking) {
                assert(length % bytes_per_sample_ == 0);
                std::unique_lock<std::mutex> lock(mutex_);
                if (blocking) {
                    assert(length < capacity_);
                    not_empty_cond_.wait(lock, [&] { return size_ >= length || is_released_; });
                }
                if (is_released_) {
                    return 0;
                }
                if (size_ == 0) {
                    return 0;
                }

                *pos = ts_buffer_[head_ % capacity_];

                int get_length = length < size_ ? length : size_;
                assert(get_length % bytes_per_sample_ == 0);
                for (int i = 0; i < get_length; ++i) {
                    data[i] = data_buffer_[(head_++) % capacity_];
                }
                size_ -= get_length;

                not_full_cond_.notify_all();
                return get_length;
            }

            int capacity() {
                std::unique_lock<std::mutex> lock(mutex_);
                return capacity_;
            }

        private:

            int capacity_;
            int head_;
            int size_;
            bool is_released_ = false;
            int bytes_per_sample_ = 4;
            int audio_sample_rate_ = 44100;

            std::unique_ptr<T[]> data_buffer_;
            std::unique_ptr<double[]> ts_buffer_;

            std::mutex mutex_;
            std::condition_variable not_empty_cond_;
            std::condition_variable not_full_cond_;
        };
    }
}

#endif
