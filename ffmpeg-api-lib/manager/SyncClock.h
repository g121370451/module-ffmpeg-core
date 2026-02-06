#pragma once
/**
 * 同步时钟 用于控制每一个解析器解析时刻的类
 * public:
 *      1. 标记起始时间
 *      2. 传入目标时间，判断需要睡眠多久 进行睡眠操作
 * private:
 *      1. 获取当前的偏移时间
*/
extern "C"
{
#include <libavutil/time.h>
}
extern int64_t AllowOffestTime;
class SyncClock{
public:
    void markCurrentTime(int64_t totalTime);
    bool syncControl(int64_t pts);
    void pause();
    void resume();
private:
    int64_t getSyncDrift() const;
    int64_t startTime;
    int64_t totalTime;
    int64_t pauseAt = 0;
};