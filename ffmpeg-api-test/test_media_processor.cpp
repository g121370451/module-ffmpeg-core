#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include "../ffmpeg-api-lib/utils/MediaProcessor.h" // 引用你的静态库头文件
#include <filesystem>
#include <MediaManager.h>
namespace fs = std::filesystem;
std::string GetTestAssetPath(const std::string& relative_path) {
    // fs::current_path() 获取的是进程启动时的当前工作目录
    // fs::absolute 会将其转换为绝对路径
    fs::path p = fs::absolute(fs::path(relative_path));
    
    // 打印一下，方便在测试失败时排查路径是否正确
    std::string absolute_str = p.string();
    spdlog::debug("Resolving path: {} -> {}", relative_path, absolute_str);
    
    return absolute_str;
}
// 场景：验证常规视频文件的元数据提取
TEST(MediaProcessorTest, IdentifyVideoCorrectly)
{
    std::string videoPath = GetTestAssetPath("test.mp4");
    MediaInfo info = MediaProcessor::GetMediaInfo(videoPath);
    nlohmann::json j = info;
    spdlog::info("Media Metadata:\n{}", j.dump(4));
    ASSERT_TRUE(info.valid) << "文件加载失败: " << videoPath;
    EXPECT_EQ(info.type, "video");
    EXPECT_GT(info.width, 0);
    EXPECT_GT(info.height, 0);
}

TEST(MediaProcessorTest, IdentifyGifCorrectly)
{
    std::string gifPath = GetTestAssetPath("test.gif");
    auto info = MediaProcessor::GetMediaInfo(gifPath);
    nlohmann::json j = info;
    spdlog::info("Media Metadata:\n{}", j.dump(4));
    ASSERT_TRUE(info.valid) << "文件加载失败: " << gifPath;
    EXPECT_EQ(info.type, "gif");
    EXPECT_GT(info.width, 0);
    EXPECT_GT(info.height, 0);
}

TEST(MediaProcessorTest, IdentifyImgCorrectly)
{
    std::string jpgPath = GetTestAssetPath("test.jpg");
    auto info = MediaProcessor::GetMediaInfo(jpgPath);
    nlohmann::json j = info;
    spdlog::info("Media Metadata:\n{}", j.dump(4));
    ASSERT_TRUE(info.valid) << "文件加载失败: " << jpgPath;
    EXPECT_EQ(info.type, "img");
    EXPECT_GT(info.width, 0);
    EXPECT_GT(info.height, 0);
}

// 场景：验证 SAR 和 DAR 的数学计算
// 假设视频是 1920x1080，SAR 是 1:1，则 DAR 应该是 16:9
TEST(MediaProcessorTest, CalculateAspectRatio)
{
    std::string videoPath = GetTestAssetPath("test.mp4");
    MediaInfo info = MediaProcessor::GetMediaInfo(videoPath);
    // 使用 LaTeX 逻辑公式推导：$DAR = \frac{W}{H} \times SAR$
    EXPECT_STREQ(info.sar.c_str(), "1:1");
}

TEST(MediaManagerTest, FixedOutputDynamicROITest) {
    MediaManager manager;
    std::string devId = "device_A";
    int index = 1001;
    std::string videoPath = GetTestAssetPath("test.mp4");
    // 初始配置：从 (0,0) 开始截取 640x480，输出固定为 320x240
    ROIConfig initConfig(0, 0, 640, 480, 320, 240);
    auto encoder = std::make_unique<MjpegEncoder>();
    
    ASSERT_TRUE(manager.AddMedia(devId, index, videoPath, initConfig, std::move(encoder)));

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 获取第一帧
    auto f1 = manager.GetNextFrame(devId, index);
    ASSERT_TRUE(f1.success);
    EXPECT_EQ(f1.width, 320);
    EXPECT_EQ(f1.height, 240);

    // 动态修改截取区域到 (100, 100)，截取大小改为 300x300
    // 注意：输出 320x240 不应该改变
    manager.UpdateConfig(devId, index, 100, 100, 300, 300);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    auto f2 = manager.GetNextFrame(devId, index);
    ASSERT_TRUE(f2.success);
    EXPECT_EQ(f2.width, 320);  // 依然是 320
    EXPECT_EQ(f2.height, 240); // 依然是 240
    
    // 检查 BufferAB 的稳定性
    EXPECT_EQ(f1.data.size() > 0, true);
    EXPECT_EQ(f2.data.size() > 0, true);
}