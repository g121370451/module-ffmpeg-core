#include <SyncClock.h>
#include <TimerSleep.h>

int64_t AllowOffestTime = 40;

void SyncClock::markCurrentTime(int64_t totalTime) {
    this->startTime = av_gettime() / 1000;
    this->totalTime = totalTime;
}

bool SyncClock::syncControl(int64_t pts) {
    // 获取现在的时刻的偏移
    int64_t base = this->getSyncDrift();
    // 获取理论要睡的时间 pts为视频的偏移
    int64_t sleepMs = pts - base;
    // spdlog::info("SleepMs is {}, pts :{} , base :{}", sleepMs, pts, base);
    if(sleepMs > totalTime){
        startTime = av_gettime() / 1000 - pts;
    }
    if (sleepMs <= 0 && AllowOffestTime + sleepMs >= 0) {
        return true;
    }
    if (sleepMs >= 0) {
        //睡眠一段时间
        TimerSleep::sleep_for_ms(sleepMs);
        return true;
    }
    return false;
}

void SyncClock::pause()
{
    this->pauseAt = av_gettime() / 1000;
}

void SyncClock::resume()
{
    if (this->pauseAt > 0) {
        this->startTime += av_gettime() / 1000 - this->pauseAt;
        this->pauseAt = 0;
    }
}

int64_t SyncClock::getSyncDrift() const {
    int64_t cur = av_gettime() / 1000;
    return cur - this->startTime;
}