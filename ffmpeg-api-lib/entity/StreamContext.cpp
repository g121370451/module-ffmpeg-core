#include "StreamContext.h"

void FrameBuffer::Update(EncoderOutput &&newFrame)
{
    std::lock_guard<std::mutex> lock(mtx);
    bufferB = std::move(newFrame);
    std::swap(bufferA, bufferB);
}

EncoderOutput FrameBuffer::Get()
{
    std::lock_guard<std::mutex> lock(mtx);
    return bufferA; // 返回拷贝给 Node.js，确保线程安全
}

void StreamContext::ReleaseFilter()
{
    if (filter_graph)
    {
        avfilter_graph_free(&filter_graph);
        filter_graph = nullptr;
        buffersink_ctx = nullptr;
        buffersrc_ctx = nullptr;
    }
}

StreamContext::~StreamContext()
{
    stop_flag = true;
    if (worker.joinable())
        worker.join();
    ReleaseFilter();
    if (dec_ctx)
    {
        avcodec_free_context(&dec_ctx);
        dec_ctx = nullptr;
    }
    if (fmt_ctx)
    {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
    }
}

ROIConfig::ROIConfig()
{
}

ROIConfig::ROIConfig(int x, int y, int sw, int sh, int ow, int oh)
{
    this->srcX = x;
    this->srcY = y;
    this->srcW = sw;
    this->srcH = sh;
    this->outW = ow;
    this->outH = oh;
}
