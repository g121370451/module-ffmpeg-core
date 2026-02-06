#pragma once
#include "Encoders.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

struct ROIConfig
{
    int srcX, srcY;
    int srcW, srcH;
    int outW, outH;
    ROIConfig();
    ROIConfig(int x, int y, int sw, int sh, int ow, int oh);
};

struct FrameBuffer
{
    EncoderOutput bufferA; // 供外部读取 (Latest)
    EncoderOutput bufferB; // 供内部写入 (Working)
    std::mutex mtx;

    void Update(EncoderOutput &&newFrame);

    EncoderOutput Get();
};

struct StreamContext
{
    // FFmpeg 原始上下文
    AVFormatContext *fmt_ctx = nullptr;
    AVCodecContext *dec_ctx = nullptr;
    int video_idx = -1;
    // int64_t pts_offset = 0;
    // int64_t last_pts = 0;

    // 状态与线程
    std::unique_ptr<IEncoder> encoder;
    FrameBuffer frame_buffer;

    // 事件通知核心
    std::mutex sync_mtx;
    std::condition_variable cv_decode; // 用于通知解码线程：B 帧已空，可以继续
    std::atomic<bool> b_frame_busy{false}; // B 帧占用标志
    
    // 静态资源控制
    std::atomic<bool> is_static{false};
    std::atomic<bool> static_decoded{false};

    // 动态配置锁
    ROIConfig config;
    std::mutex config_mtx;
    std::atomic<bool> config_changed{false};

    std::atomic<bool> stop_flag{false};
    std::thread worker;

    int64_t totalTime;

    // 暂停控制
    std::atomic<bool> is_paused{false};
    std::mutex pause_mtx;
    std::condition_variable cv_pause;

    // Filter
    AVFilterGraph* filter_graph = nullptr;
    AVFilterContext* buffersrc_ctx = nullptr;
    AVFilterContext* buffersink_ctx = nullptr;

    void ReleaseFilter();

    ~StreamContext();
};