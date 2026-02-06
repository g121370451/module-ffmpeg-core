#include "MediaProcessor.h"
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/rational.h>
}

MediaInfo MediaProcessor::GetMediaInfo(const std::string& filePath) {
    MediaInfo info;
    AVFormatContext* fmt_ctx = nullptr;

    if (avformat_open_input(&fmt_ctx, filePath.c_str(), nullptr, nullptr) < 0) {
        return info;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) >= 0) {
        info.duration = static_cast<double>(fmt_ctx->duration) / AV_TIME_BASE;
        
        // 识别类型逻辑
        std::string formatName = fmt_ctx->iformat->name;
        if (formatName.find("image2") != std::string::npos || formatName.find("png") != std::string::npos) {
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