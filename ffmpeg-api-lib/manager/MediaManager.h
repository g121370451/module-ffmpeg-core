#pragma once
#include "StreamContext.h"
#include <unordered_map>
#include <string>
#include "SyncClock.h"
class MediaManager
{
public:
    MediaManager();
    ~MediaManager() = default;

    // 添加任务：deviceId + indexCode 构成唯一标识
    bool AddMedia(const std::string &deviceId,
                  int indexCode,
                  const std::string &url,
                  const ROIConfig &config,
                  std::unique_ptr<IEncoder> encoder);
    bool DeleteMedia(const std::string &deviceId,
                     int indexCode);
    void UpdateConfig(const std::string &devId, int idx, int x, int y, int sw, int sh);

    bool Pause(const std::string &deviceId, int indexCode);

    bool Resume(const std::string &deviceId, int indexCode);

    // 获取最新帧：A 指针数据
    EncoderOutput GetNextFrame(std::string deviceId, int indexCode);

private:
    std::unordered_map<std::string, std::shared_ptr<StreamContext>> contexts;
    std::mutex map_mtx;
    SyncClock sync_clock_;
    bool InitFilterGraph(std::shared_ptr<StreamContext> ctx, const ROIConfig &cfg, AVFrame *in_frame);
    void DecodingLoop(std::shared_ptr<StreamContext> ctx);
    std::string MakeKey(std::string devId, int idx);
};