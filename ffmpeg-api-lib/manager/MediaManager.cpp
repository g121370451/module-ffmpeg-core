#include "MediaManager.h"
#include <spdlog/spdlog.h>
#include <magic_enum/magic_enum.hpp>

MediaManager::MediaManager() : sync_clock_()
{
}

bool MediaManager::AddMedia(const std::string &deviceId, int indexCode, const std::string &url, const ROIConfig &config, std::unique_ptr<IEncoder> encoder)
{
    auto key = MakeKey(deviceId, indexCode);
    auto ctx = std::make_shared<StreamContext>();

    if (avformat_open_input(&ctx->fmt_ctx, url.c_str(), nullptr, nullptr) < 0)
        return false;
    if (avformat_find_stream_info(ctx->fmt_ctx, nullptr) < 0)
        return false;

    const AVCodec *decoder = nullptr;
    ctx->video_idx = av_find_best_stream(ctx->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (ctx->video_idx < 0)
        return false;

    auto v_stream = ctx->fmt_ctx->streams[ctx->video_idx];

    bool durationIsZero = (ctx->fmt_ctx->duration <= 0 || ctx->fmt_ctx->duration == AV_NOPTS_VALUE);
    // - 明确标注只有 1 帧
    bool frameCountIsOne = (v_stream->nb_frames == 1);
    // - 或者是 image2 这种图片序列解复用器（通常用于处理 .jpg, .png 等）
    std::string formatName = ctx->fmt_ctx->iformat->name;
    bool isImageFormat = (formatName.find("image2") != std::string::npos || formatName.find("mjpeg") != std::string::npos);

    if (durationIsZero || frameCountIsOne || isImageFormat)
    {
        ctx->is_static = true;
        spdlog::info("[{}] Identified as STATIC image (Format: {}, Frames: {})",
                     deviceId, formatName, v_stream->nb_frames);
    }

    int rawW = v_stream->codecpar->width;
    int rawH = v_stream->codecpar->height;

    if (config.srcX + config.srcW > rawW || config.srcY + config.srcH > rawH)
    {
        spdlog::error("[{}:{}] ROI out of bounds! Media: {}x{}, ROI: {}x{}+{}+{}",
                      deviceId, indexCode, rawW, rawH, config.srcW, config.srcH, config.srcX, config.srcY);
        return false;
    }

    ctx->dec_ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(ctx->dec_ctx, ctx->fmt_ctx->streams[ctx->video_idx]->codecpar);
    if (avcodec_open2(ctx->dec_ctx, decoder, nullptr) < 0)
        return false;

    // 4. 配置编码器
    ctx->config = config;
    ctx->encoder = std::move(encoder);
    if (!ctx->encoder->Open(config.outW, config.outH))
        return false;

    ctx->worker = std::thread(&MediaManager::DecodingLoop, this, ctx);
    ctx->totalTime = ctx->fmt_ctx->duration;
    this->sync_clock_.markCurrentTime(ctx->totalTime);
    std::lock_guard<std::mutex> lock(map_mtx);
    contexts[key] = ctx;
    return true;
}

bool MediaManager::DeleteMedia(const std::string &deviceId, int indexCode)
{
    auto key = MakeKey(deviceId, indexCode);

    std::lock_guard<std::mutex> lock(map_mtx);

    if (contexts.find(key) == contexts.end())
    {
        spdlog::warn("[{}] DeleteMedia failed: Key not found", key);
        return true;
    }

    this->contexts.erase(key);

    spdlog::info("[{}] Media context deleted successfully.", key);
    return true;
}

EncoderOutput MediaManager::GetNextFrame(std::string deviceId, int indexCode)
{
    auto key = MakeKey(deviceId, indexCode);
    std::shared_ptr<StreamContext> ctx;
    {
        std::lock_guard<std::mutex> lock(map_mtx);
        if (contexts.find(key) == contexts.end())
            return {};
        ctx = contexts[key];
    }

    // 尝试获取最新数据
    {
        std::lock_guard<std::mutex> lk(ctx->sync_mtx);
        if (ctx->b_frame_busy)
        {
            std::swap(ctx->frame_buffer.bufferA, ctx->frame_buffer.bufferB);
            ctx->b_frame_busy = false;
            // 唤醒解码线程开始工作
            ctx->cv_decode.notify_one();
        }
    }
    return ctx->frame_buffer.bufferA;
}

bool MediaManager::InitFilterGraph(std::shared_ptr<StreamContext> ctx, const ROIConfig &cfg, AVFrame *in_frame)
{
    ctx->ReleaseFilter(); // 销毁旧的，准备重建
    ctx->filter_graph = avfilter_graph_alloc();

    char args[512];
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();

    // 1. 输入端配置：动态适配各种输入格式 (in_frame->format)
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d:colorspace=%d:range=%d",
             in_frame->width, in_frame->height, in_frame->format,
             ctx->fmt_ctx->streams[ctx->video_idx]->time_base.num,
             ctx->fmt_ctx->streams[ctx->video_idx]->time_base.den,
             in_frame->sample_aspect_ratio.num,
             in_frame->sample_aspect_ratio.den,
             in_frame->colorspace,
             in_frame->color_range);
    // spdlog::info("Creating filter with args: {}", args);
    int ret = avfilter_graph_create_filter(&ctx->buffersrc_ctx, buffersrc, "in", args, nullptr, ctx->filter_graph);
    if (ret < 0)
        return false;

    // 2. 输出端配置
    ret = avfilter_graph_create_filter(&ctx->buffersink_ctx, buffersink, "out", nullptr, nullptr, ctx->filter_graph);
    if (ret < 0)
        return false;

    // 3. 滤镜描述符：crop -> scale -> format(强制转为 yuvj420p 适配 MJPEG)
    // 注意：这里直接使用最新的 cfg 参数
    std::string filters_descr = "crop=" + std::to_string(cfg.srcW) + ":" + std::to_string(cfg.srcH) +
                                ":" + std::to_string(cfg.srcX) + ":" + std::to_string(cfg.srcY) +
                                ",scale=" + std::to_string(cfg.outW) + ":" + std::to_string(cfg.outH) +
                                ",format=yuv420p";

    outputs->name = av_strdup("in");
    outputs->filter_ctx = ctx->buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = ctx->buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    ret = avfilter_graph_parse_ptr(ctx->filter_graph, filters_descr.c_str(), &inputs, &outputs, nullptr);
    if (ret < 0)
        return false;

    ret = avfilter_graph_config(ctx->filter_graph, nullptr);

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return ret >= 0;
}

void MediaManager::DecodingLoop(std::shared_ptr<StreamContext> ctx)
{
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *yuvFrame = av_frame_alloc();

    yuvFrame->format = AV_PIX_FMT_YUV420P;
    yuvFrame->width = ctx->config.outW;
    yuvFrame->height = ctx->config.outH;
    av_frame_get_buffer(yuvFrame, 0);

    // 缓存当前正在使用的配置
    ROIConfig curCfg;
    {
        std::lock_guard<std::mutex> lk(ctx->config_mtx);
        curCfg = ctx->config;
    }

    while (!ctx->stop_flag)
    {

        // 停止等待逻辑
        {
            std::unique_lock<std::mutex> lk(ctx->pause_mtx);
            // 如果处于 paused 状态且没有停止，则在此无限等待
            ctx->cv_pause.wait(lk, [&]
                               { return !ctx->is_paused || ctx->stop_flag; });
        }

        if (ctx->stop_flag)
            break;
        {
            std::unique_lock<std::mutex> lk(ctx->sync_mtx);
            // spdlog::info("frame busy status {}",ctx->b_frame_busy.load());
            ctx->cv_decode.wait(lk, [&]
                                { return !ctx->b_frame_busy || ctx->stop_flag; });
        }

        if (ctx->stop_flag)
            break;

        // 如果是图片且已解析过，线程进入无限休眠（除非 stop）
        if (ctx->is_static && ctx->static_decoded)
        {
            std::unique_lock<std::mutex> lk(ctx->sync_mtx);
            ctx->cv_decode.wait(lk, [&]
                                { return ctx->stop_flag.load(); });
            break;
        }
        if (av_read_frame(ctx->fmt_ctx, pkt) >= 0)
        {
            if (pkt->stream_index == ctx->video_idx)
            {
                if (avcodec_send_packet(ctx->dec_ctx, pkt) == 0)
                {
                    while (avcodec_receive_frame(ctx->dec_ctx, frame) == 0)
                    {
                        double timestamp = static_cast<double>(frame->pts) * av_q2d(ctx->fmt_ctx->streams[ctx->video_idx]->time_base);
                        // spdlog::info("current time is {}", timestamp);
                        bool currentFrameSend = this->sync_clock_.syncControl(timestamp * 1000);
                        if (!currentFrameSend)
                        {
                            continue;
                        }
                        // frame->pts += ctx->pts_offset;
                        // ctx->last_pts = frame->pts;
                        // 检查配置动态更新
                        if (ctx->config_changed || !ctx->filter_graph)
                        {
                            // spdlog::info("Re-initializing filter graph with new configuration.");
                            std::lock_guard<std::mutex> lk(ctx->config_mtx);
                            curCfg = ctx->config;
                            if (!InitFilterGraph(ctx, curCfg, frame))
                            {
                                spdlog::error("Failed to re-init filter graph");
                                continue;
                            }
                            ctx->config_changed = false;
                        }
                        // spdlog::info("filter process");
                        if (av_buffersrc_add_frame_flags(ctx->buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0)
                        {
                            while (av_buffersink_get_frame(ctx->buffersink_ctx, yuvFrame) == 0)
                            {
                                EncoderOutput out;
                                ctx->encoder->Encode(yuvFrame, out);
                                // 更新 B 帧并设为忙碌
                                {
                                    std::lock_guard<std::mutex> lk(ctx->sync_mtx);
                                    ctx->frame_buffer.bufferB = std::move(out);
                                    ctx->b_frame_busy = true;
                                }
                                if (ctx->is_static)
                                    ctx->static_decoded = true;
                                av_frame_unref(yuvFrame);
                            }
                        }
                    }
                }
            }
            av_packet_unref(pkt);
        }
        else
        {
            // spdlog::info("Stream ended, seeking back to start.");
            // ctx->pts_offset = ctx->last_pts + 1;
            avformat_seek_file(ctx->fmt_ctx, -1, INT64_MIN, 0, INT64_MAX, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(ctx->dec_ctx);
            ctx->encoder->Close();
            ctx->encoder->Open(ctx->config.outW, ctx->config.outH);
            ctx->ReleaseFilter();
            this->sync_clock_.markCurrentTime(ctx->totalTime);
        }
    }

    av_frame_free(&yuvFrame);
    av_frame_free(&frame);
    av_packet_free(&pkt);
}

void MediaManager::UpdateConfig(const std::string &devId, int idx, int x, int y, int sw, int sh)
{
    auto key = devId + "_" + std::to_string(idx);
    std::lock_guard<std::mutex> lock(map_mtx);
    if (contexts.find(key) != contexts.end())
    {
        auto &ctx = contexts[key];
        std::lock_guard<std::mutex> cfg_lock(ctx->config_mtx);
        ctx->config = {x, y, sw, sh, ctx->config.outW, ctx->config.outH};
        ctx->config_changed = true;
    }
}

bool MediaManager::Pause(const std::string &deviceId, int indexCode)
{
    auto key = MakeKey(deviceId, indexCode);

    std::lock_guard<std::mutex> lock(map_mtx);

    if (contexts.find(key) == contexts.end())
    {
        spdlog::warn("[{}] Pause failed: Key not found", key);
        return false;
    }

    auto ctx = this->contexts[key];

    // 1. 设置状态
    {
        std::lock_guard<std::mutex> lk(ctx->pause_mtx);
        ctx->is_paused = true;
    }

    // 2. 通知时钟记录暂停时间
    // 注意：要在解码线程挂起前或者同时记录，最好由主控线程立即记录
    this->sync_clock_.pause();
    return true;
}

bool MediaManager::Resume(const std::string &deviceId, int indexCode)
{
    auto key = MakeKey(deviceId, indexCode);

    std::lock_guard<std::mutex> lock(map_mtx);

    if (contexts.find(key) == contexts.end())
    {
        spdlog::warn("[{}] Resume failed: Key not found", key);
        return false;
    }

    auto ctx = this->contexts[key];

    // 1. 修正时钟（要在唤醒线程之前做）
    this->sync_clock_.resume();

    // 2. 唤醒解码线程
    {
        std::lock_guard<std::mutex> lk(ctx->pause_mtx);
        ctx->is_paused = false;
    }
    ctx->cv_pause.notify_all();
    return true;
}

std::string MediaManager::MakeKey(std::string devId, int idx)
{
    return devId + "_" + std::to_string(idx);
}
