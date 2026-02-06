#pragma once
#include <vector>
#include <cstdint>
#include <memory>

extern "C"
{
#include <libavcodec/avcodec.h>
}
struct EncoderOutput
{
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    int64_t timestamp = 0;
    bool success = false;
};
// 编码器接口规范
class IEncoder
{
public:
    virtual ~IEncoder() = default;
    virtual bool Open(int w, int h) = 0;
    virtual void Encode(AVFrame *frame, EncoderOutput &out) = 0;
    virtual void Close() = 0;
    virtual bool Reset(int w, int h) = 0;
    virtual AVCodecContext* GetCodecContext() { return nullptr; }
};

// MJPEG 具体实现
class MjpegEncoder : public IEncoder
{
private:
    AVCodecContext *enc_ctx = nullptr;

public:
    ~MjpegEncoder();

    bool Open(int w, int h) override;

    AVCodecContext* GetCodecContext() override;

    void Encode(AVFrame *frame, EncoderOutput &out) override;

    void Close() override;
    
    bool Reset(int w, int h) override;
};