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

class MediaProcessor
{
public:
    static MediaInfo GetMediaInfo(const std::string &filePath);
};