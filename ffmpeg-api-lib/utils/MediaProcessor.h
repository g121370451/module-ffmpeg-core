#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
struct MediaInfo
{
    std::string type; // "video", "img", "gif", "unknown"
    double duration;  // 秒
    int width;
    int height;
    std::string sar; // 样本宽高比 "1:1"
    std::string dar; // 显示宽高比 "16:9"
    bool valid = false;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MediaInfo, type, duration, width, height, sar, dar, valid)
};

struct CropResult
{
    bool success = false;
    std::string error;
};

class MediaProcessor
{
public:
    static MediaInfo GetMediaInfo(const std::string &filePath);
    static CropResult CropMedia(
        const std::string &inputPath,
        const std::string &outputPath,
        int srcX, int srcY, int srcW, int srcH,
        int outW, int outH,
        int quality,        // 1-100, 100=最高质量
        double startTime,   // 秒，仅视频生效，<=0 表示从头开始
        double endTime      // 秒，仅视频生效，<=0 表示到结尾
    );
};