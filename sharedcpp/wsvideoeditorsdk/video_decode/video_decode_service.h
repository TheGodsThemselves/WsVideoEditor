#ifndef SHAREDCPP_WS_VIDEO_EDITOR_VIDEO_DECODE_SERVICE_H
#define SHAREDCPP_WS_VIDEO_EDITOR_VIDEO_DECODE_SERVICE_H

#include <mutex>
#include <thread>
#include <memory>
#include <vector>
#include <unordered_map>
#include <deque>
#include <wsvideoeditorsdk/prebuilt_protobuf/ws_video_editor_sdk.pb.h>
#include "base/blocking_queue.h"
#include "video_decode_context.h"
#include "av_utils.h"
#include "preview_timeline.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
};

namespace whensunset {
    namespace wsvideoeditor {

        class VideoDecodeService {
        public:
            VideoDecodeService(int buffer_capacity = 10) : decoded_unit_queue_(buffer_capacity) {
                LOGI("VideoDecodeService buffer_capacity:%d", buffer_capacity);
            }

            virtual ~VideoDecodeService() {
                {
                    std::lock_guard<std::mutex> lk(member_param_mutex_);
                    released_ = true;
                }
                Stop();
                LOGI("~VideoDecodeService");
            }

            /**
             * 设置 @project 只在需要初始化的时候调用
             * @param project
             * @param render_pos
             */
            void SetProject(const model::EditorProject &project, double render_pos);

            /**
             * 开始解码
             */
            void Start();

            /**
             * 更新 @project 在解码过程中如果 @project 改变了，可以调用
             * @param project
             */
            void UpdateProject(const model::EditorProject &project);

            DecodedFramesUnit GetRenderFrameAtPtsOrNull(double pts_sec, int get_frame_flags = 0);

            /**
             * 更变解码的位置
             * @param render_pos
             */
            void ResetDecodePosition(double render_pos);

            /**
             * 暂停解码
             */
            void Stop();

            inline bool ended() {
                return ended_;
            }

            inline bool stopped() {
                return stopped_;
            }

            inline int GetBufferedFrameCount() {
                return decoded_unit_queue_.Size();
            }

        private:
            DecodedFramesUnit GetRenderFrameAtPtsInternal(double pts_sec, int get_frame_flags = 0);

            /**
             * 开始运行解码线程
             */
            void DecodeThreadMain();

            /**
             * 解码到最后一帧的回调
             */
            void DecodeEofHandle();

            /**
             * 读取一帧
             */
            UniqueAVFramePtr ReadOneFrame(VideoDecodeContext *ctx, int *ret);

            int SeekTo(VideoDecodeContext *ctx, double render_pos);

            int OpenMediaAsset(VideoDecodeContext &ctx, model::MediaAsset *trackAsset);

            inline bool HasMediaAsset() { return project_.media_asset_size() > 0; }

            /**
             * 控制所有成员变量不可并发读写的锁
             */
            std::mutex member_param_mutex_;

            /**
             * @Start() 和 @Stop() 这两个方法不可并发进入的锁
             */
            std::mutex start_stop_mutex_;

            /**
             * 判断 @DecodeThreadMain() 中的 thread 是否继续运行的条件变量锁
             */
            std::condition_variable decode_thread_waiting_cv_;

            /**
             * 是否解码已经停止，调用 @Stop()/@Start() 后分别为 true/false
             */
            bool stopped_ = true;

            /**
             * 是否 @VideoDecodeService 对象已经销毁了
             */
            bool released_ = false;

            /**
             * 是否视频已经解码到了最后一帧
             */
            bool ended_ = false;

            /**
             * 是否 @project 变化了 @SetProject()、@UpdateProject() 调用后设置为 true
             */
            bool project_changed_ = false;

            /**
             * 是否 render pos 已经改变了，-1 表示没有改变
             */
            double changed_render_pos_ = -1;

            /**
             * 帧队列
             */
            base::BlockingQueue<DecodedFramesUnit> decoded_unit_queue_;

            model::EditorProject project_;

            std::mutex pop_frame_mutex_;

            std::thread decode_thread_;
        };

        std::unique_ptr<VideoDecodeService> VideoDecodeServiceCreate(int buffer_capacity = 5);
    }
}
#endif
