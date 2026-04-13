#include "MediaProcessor.h"
#include <iostream>
#include <spdlog/spdlog.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/rational.h>
#include <libavutil/opt.h>
}

MediaInfo MediaProcessor::GetMediaInfo(const std::string& filePath) {
    MediaInfo info;
    AVFormatContext* fmt_ctx = nullptr;

    if (avformat_open_input(&fmt_ctx, filePath.c_str(), nullptr, nullptr) < 0) {
        return info;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) >= 0) {
        // 处理 duration，AV_NOPTS_VALUE 和负值视为无效
        if (fmt_ctx->duration != AV_NOPTS_VALUE && fmt_ctx->duration > 0) {
            info.duration = static_cast<double>(fmt_ctx->duration) / AV_TIME_BASE;
        } else {
            info.duration = 0.0;
        }

        // 识别类型逻辑
        std::string formatName = fmt_ctx->iformat->name;
        spdlog::info("Format: {}, Duration: {}s", formatName, info.duration);
        if (formatName.find("image2") != std::string::npos
            || formatName.find("_pipe") != std::string::npos
            || formatName.find("mjpeg") != std::string::npos) {
            info.type = "img";
        } else if (formatName.find("gif") != std::string::npos) {
            info.type = "gif";
        } else {
            info.type = "video";
        }

        for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
            AVCodecParameters* params = fmt_ctx->streams[i]->codecpar;
            if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
                info.width = params->width;
                info.height = params->height;

                // 获取 SAR (Sample Aspect Ratio)
                AVRational sar = fmt_ctx->streams[i]->sample_aspect_ratio;
                if (sar.num == 0) sar = {1, 1}; // 默认 1:1
                info.sar = std::to_string(sar.num) + ":" + std::to_string(sar.den);

                // 计算 DAR (Display Aspect Ratio) = (width / height) * SAR
                AVRational dar;
                av_reduce(&dar.num, &dar.den,
                          params->width * sar.num,
                          params->height * sar.den,
                          1024 * 1024);
                info.dar = std::to_string(dar.num) + ":" + std::to_string(dar.den);

                info.valid = true;
                break;
            }
        }
    }

    avformat_close_input(&fmt_ctx);
    return info;
}

CropResult MediaProcessor::CropMedia(
    const std::string &inputPath,
    const std::string &outputPath,
    int srcX, int srcY, int srcW, int srcH,
    int outW, int outH,
    int quality,
    double startTime,
    double endTime)
{
    CropResult result;

    // 参数校验
    if (inputPath.empty()) {
        result.error = "inputPath cannot be empty";
        return result;
    }
    if (outputPath.empty()) {
        result.error = "outputPath cannot be empty";
        return result;
    }
    if (quality < 1 || quality > 100) {
        result.error = "quality must be between 1 and 100, got " + std::to_string(quality);
        return result;
    }
    if (srcX < 0 || srcY < 0) {
        result.error = "srcX and srcY must be >= 0, got srcX=" + std::to_string(srcX) + " srcY=" + std::to_string(srcY);
        return result;
    }
    if (startTime > 0 && endTime > 0 && startTime >= endTime) {
        result.error = "startTime must be less than endTime, got start=" + std::to_string(startTime) + " end=" + std::to_string(endTime);
        return result;
    }

    AVFormatContext *ifmt_ctx = nullptr;
    AVCodecContext *dec_ctx = nullptr;
    AVFormatContext *ofmt_ctx = nullptr;
    AVCodecContext *enc_ctx = nullptr;
    AVFilterGraph *filter_graph = nullptr;
    AVFilterContext *buffersrc_ctx = nullptr;
    AVFilterContext *buffersink_ctx = nullptr;
    AVPacket *pkt = nullptr;
    AVFrame *frame = nullptr;
    AVFrame *filt_frame = nullptr;
    int video_idx = -1;
    int ret = 0;
    int64_t startPts = INT64_MIN;
    int64_t endPts = INT64_MAX;

    // 1. 打开输入
    if (avformat_open_input(&ifmt_ctx, inputPath.c_str(), nullptr, nullptr) < 0) {
        result.error = "Failed to open input: " + inputPath;
        goto cleanup;
    }
    if (avformat_find_stream_info(ifmt_ctx, nullptr) < 0) {
        result.error = "Failed to find stream info";
        goto cleanup;
    }

    // 2. 找视频流并打开解码器
    {
        const AVCodec *decoder = nullptr;
        video_idx = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
        if (video_idx < 0) {
            result.error = "No video stream found";
            goto cleanup;
        }
        dec_ctx = avcodec_alloc_context3(decoder);
        avcodec_parameters_to_context(dec_ctx, ifmt_ctx->streams[video_idx]->codecpar);
        if (avcodec_open2(dec_ctx, decoder, nullptr) < 0) {
            result.error = "Failed to open decoder";
            goto cleanup;
        }
    }

    // 处理默认参数：srcW/srcH=0 表示不裁剪，outW/outH=0 表示不缩放
    if (srcW <= 0) srcW = dec_ctx->width;
    if (srcH <= 0) srcH = dec_ctx->height;
    if (outW <= 0) outW = srcW;
    if (outH <= 0) outH = srcH;

    // ROI 边界校验
    if (srcX + srcW > dec_ctx->width || srcY + srcH > dec_ctx->height) {
        result.error = "ROI out of bounds: media=" + std::to_string(dec_ctx->width) + "x" + std::to_string(dec_ctx->height) +
            ", crop=" + std::to_string(srcW) + "x" + std::to_string(srcH) + "+" + std::to_string(srcX) + "+" + std::to_string(srcY);
        goto cleanup;
    }

    // 时间范围处理
    {
        bool isVideo = (ifmt_ctx->duration > 0 && ifmt_ctx->duration != AV_NOPTS_VALUE);
        double durationSec = isVideo ? static_cast<double>(ifmt_ctx->duration) / AV_TIME_BASE : 0.0;

        if (isVideo && endTime > 0 && endTime > durationSec) {
            result.error = "endTime exceeds media duration: endTime=" + std::to_string(endTime) + "s, duration=" + std::to_string(durationSec) + "s";
            goto cleanup;
        }
        if (isVideo && startTime > 0 && startTime >= durationSec) {
            result.error = "startTime exceeds media duration: startTime=" + std::to_string(startTime) + "s, duration=" + std::to_string(durationSec) + "s";
            goto cleanup;
        }

        if (isVideo && startTime > 0) {
            int64_t seekTarget = static_cast<int64_t>(startTime * AV_TIME_BASE);
            avformat_seek_file(ifmt_ctx, -1, INT64_MIN, seekTarget, seekTarget, 0);
            avcodec_flush_buffers(dec_ctx);
        }

        AVRational tb = ifmt_ctx->streams[video_idx]->time_base;
        startPts = (isVideo && startTime > 0) ? static_cast<int64_t>(startTime / av_q2d(tb)) : INT64_MIN;
        endPts = (isVideo && endTime > 0) ? static_cast<int64_t>(endTime / av_q2d(tb)) : INT64_MAX;
    }

    // 3. 打开输出
    {
        avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, outputPath.c_str());
        if (!ofmt_ctx) {
            result.error = "Failed to create output context";
            goto cleanup;
        }

        enum AVCodecID codec_id = av_guess_codec(ofmt_ctx->oformat, nullptr, outputPath.c_str(), nullptr, AVMEDIA_TYPE_VIDEO);
        const AVCodec *encoder = avcodec_find_encoder(codec_id);
        if (!encoder) {
            result.error = "No suitable encoder for output format";
            goto cleanup;
        }

        AVStream *out_stream = avformat_new_stream(ofmt_ctx, nullptr);
        if (!out_stream) {
            result.error = "Failed to create output stream";
            goto cleanup;
        }

        enc_ctx = avcodec_alloc_context3(encoder);
        enc_ctx->width = outW;
        enc_ctx->height = outH;
        enc_ctx->time_base = ifmt_ctx->streams[video_idx]->time_base;
        enc_ctx->framerate = av_guess_frame_rate(ifmt_ctx, ifmt_ctx->streams[video_idx], nullptr);

        if (encoder->pix_fmts) {
            enc_ctx->pix_fmt = encoder->pix_fmts[0];
        } else {
            enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        }

        // 质量映射
        if (codec_id == AV_CODEC_ID_MJPEG) {
            int qscale = 1 + (100 - quality) * 30 / 99;
            enc_ctx->flags |= AV_CODEC_FLAG_QSCALE;
            enc_ctx->global_quality = FF_QP2LAMBDA * qscale;
        } else if (codec_id == AV_CODEC_ID_H264 || codec_id == AV_CODEC_ID_HEVC) {
            int crf = (100 - quality) * 51 / 99;
            av_opt_set_int(enc_ctx->priv_data, "crf", crf, 0);
        } else if (codec_id == AV_CODEC_ID_WEBP) {
            av_opt_set_int(enc_ctx->priv_data, "quality", quality, 0);
        } else if (codec_id != AV_CODEC_ID_PNG) {
            int qscale = 1 + (100 - quality) * 30 / 99;
            enc_ctx->flags |= AV_CODEC_FLAG_QSCALE;
            enc_ctx->global_quality = FF_QP2LAMBDA * qscale;
        }

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2(enc_ctx, encoder, nullptr) < 0) {
            result.error = "Failed to open encoder";
            goto cleanup;
        }
        avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
        out_stream->time_base = enc_ctx->time_base;

        if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&ofmt_ctx->pb, outputPath.c_str(), AVIO_FLAG_WRITE) < 0) {
                result.error = "Failed to open output file";
                goto cleanup;
            }
        }
        if (avformat_write_header(ofmt_ctx, nullptr) < 0) {
            result.error = "Failed to write output header";
            goto cleanup;
        }
    }

    // 4. 构建 filter graph（延迟到第一帧解码后）
    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    filt_frame = av_frame_alloc();

    // 帧处理循环
    while (av_read_frame(ifmt_ctx, pkt) >= 0) {
        if (pkt->stream_index != video_idx) {
            av_packet_unref(pkt);
            continue;
        }
        ret = avcodec_send_packet(dec_ctx, pkt);
        av_packet_unref(pkt);
        if (ret < 0) continue;

        while (avcodec_receive_frame(dec_ctx, frame) == 0) {
            // 时间过滤
            if (frame->pts != AV_NOPTS_VALUE) {
                if (frame->pts < startPts) {
                    av_frame_unref(frame);
                    continue;
                }
                if (frame->pts > endPts) {
                    av_frame_unref(frame);
                    goto flush;
                }
            }
            // 延迟初始化 filter graph
            if (!filter_graph) {
                filter_graph = avfilter_graph_alloc();
                const AVFilter *buffersrc = avfilter_get_by_name("buffer");
                const AVFilter *buffersink = avfilter_get_by_name("buffersink");
                AVFilterInOut *outputs = avfilter_inout_alloc();
                AVFilterInOut *inputs = avfilter_inout_alloc();

                char args[512];
                snprintf(args, sizeof(args),
                    "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                    frame->width, frame->height, frame->format,
                    ifmt_ctx->streams[video_idx]->time_base.num,
                    ifmt_ctx->streams[video_idx]->time_base.den,
                    frame->sample_aspect_ratio.num ? frame->sample_aspect_ratio.num : 1,
                    frame->sample_aspect_ratio.den ? frame->sample_aspect_ratio.den : 1);

                ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, nullptr, filter_graph);
                if (ret < 0) {
                    result.error = "Failed to create buffersrc filter";
                    avfilter_inout_free(&inputs);
                    avfilter_inout_free(&outputs);
                    av_frame_unref(frame);
                    goto cleanup;
                }
                ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", nullptr, nullptr, filter_graph);
                if (ret < 0) {
                    result.error = "Failed to create buffersink filter";
                    avfilter_inout_free(&inputs);
                    avfilter_inout_free(&outputs);
                    av_frame_unref(frame);
                    goto cleanup;
                }

                std::string filters_descr = "crop=" + std::to_string(srcW) + ":" + std::to_string(srcH) +
                    ":" + std::to_string(srcX) + ":" + std::to_string(srcY) +
                    ",scale=" + std::to_string(outW) + ":" + std::to_string(outH) +
                    ",format=" + av_get_pix_fmt_name(enc_ctx->pix_fmt);

                outputs->name = av_strdup("in");
                outputs->filter_ctx = buffersrc_ctx;
                outputs->pad_idx = 0;
                outputs->next = nullptr;
                inputs->name = av_strdup("out");
                inputs->filter_ctx = buffersink_ctx;
                inputs->pad_idx = 0;
                inputs->next = nullptr;

                ret = avfilter_graph_parse_ptr(filter_graph, filters_descr.c_str(), &inputs, &outputs, nullptr);
                if (ret < 0) {
                    result.error = "Failed to parse filter graph: " + filters_descr;
                    avfilter_inout_free(&inputs);
                    avfilter_inout_free(&outputs);
                    av_frame_unref(frame);
                    goto cleanup;
                }
                ret = avfilter_graph_config(filter_graph, nullptr);
                avfilter_inout_free(&inputs);
                avfilter_inout_free(&outputs);
                if (ret < 0) {
                    result.error = "Failed to configure filter graph";
                    av_frame_unref(frame);
                    goto cleanup;
                }
            }

            // 送入 filter → 编码 → 写入
            av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
            while (av_buffersink_get_frame(buffersink_ctx, filt_frame) == 0) {
                avcodec_send_frame(enc_ctx, filt_frame);
                while (avcodec_receive_packet(enc_ctx, pkt) == 0) {
                    av_interleaved_write_frame(ofmt_ctx, pkt);
                    av_packet_unref(pkt);
                }
                av_frame_unref(filt_frame);
            }
            av_frame_unref(frame);
        }
    }

    // Flush 解码器和编码器
flush:
    avcodec_send_packet(dec_ctx, nullptr);
    while (avcodec_receive_frame(dec_ctx, frame) == 0) {
        if (filter_graph) {
            av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
            while (av_buffersink_get_frame(buffersink_ctx, filt_frame) == 0) {
                avcodec_send_frame(enc_ctx, filt_frame);
                while (avcodec_receive_packet(enc_ctx, pkt) == 0) {
                    av_interleaved_write_frame(ofmt_ctx, pkt);
                    av_packet_unref(pkt);
                }
                av_frame_unref(filt_frame);
            }
        }
        av_frame_unref(frame);
    }

    // Flush 编码器
    avcodec_send_frame(enc_ctx, nullptr);
    while (avcodec_receive_packet(enc_ctx, pkt) == 0) {
        av_interleaved_write_frame(ofmt_ctx, pkt);
        av_packet_unref(pkt);
    }

    av_write_trailer(ofmt_ctx);
    result.success = true;

cleanup:
    if (filt_frame) av_frame_free(&filt_frame);
    if (frame) av_frame_free(&frame);
    if (pkt) av_packet_free(&pkt);
    if (filter_graph) avfilter_graph_free(&filter_graph);
    if (enc_ctx) avcodec_free_context(&enc_ctx);
    if (dec_ctx) avcodec_free_context(&dec_ctx);
    if (ofmt_ctx) {
        if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&ofmt_ctx->pb);
        avformat_free_context(ofmt_ctx);
    }
    if (ifmt_ctx) avformat_close_input(&ifmt_ctx);

    if (!result.success)
        spdlog::error("CropMedia failed: {}", result.error);

    return result;
}
