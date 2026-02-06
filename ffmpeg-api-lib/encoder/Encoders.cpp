#include "Encoders.h"
// #include <spdlog/spdlog.h>

MjpegEncoder::~MjpegEncoder()
{
    Close();
}

bool MjpegEncoder::Open(int w, int h)
{
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!codec)
        return false;

    enc_ctx = avcodec_alloc_context3(codec);
    enc_ctx->width = w;
    enc_ctx->height = h;
    enc_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    enc_ctx->time_base = {1, 25};

    return avcodec_open2(enc_ctx, codec, nullptr) >= 0;
}

AVCodecContext *MjpegEncoder::GetCodecContext()
{
    return enc_ctx;
}

void MjpegEncoder::Encode(AVFrame *frame, EncoderOutput &out)
{
    if (!enc_ctx || avcodec_send_frame(enc_ctx, frame) < 0)
        return;

    AVPacket *pkt = av_packet_alloc();
    if (avcodec_receive_packet(enc_ctx, pkt) == 0)
    {
        out.data.assign(pkt->data, pkt->data + pkt->size);
        out.width = enc_ctx->width;
        out.height = enc_ctx->height;
        // spdlog::info("Encoded MJPEG frame: {} bytes, {}x{}, timestamp: {} ms", pkt->size, out.width, out.height, pkt->pts);
        out.timestamp = pkt->pts;
        out.success = true;
    }
    av_packet_free(&pkt);
}

void MjpegEncoder::Close()
{
    if (enc_ctx)
    {
        avcodec_free_context(&enc_ctx);
        enc_ctx = nullptr;
    }
}

bool MjpegEncoder::Reset(int w, int h)
{
    if (enc_ctx && enc_ctx->width == w && enc_ctx->height == h)
        return true;
    Close();
    return Open(w, h);
}
