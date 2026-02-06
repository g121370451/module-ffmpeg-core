#pragma once
#include <iostream>
#include <Windows.h>
#include <spdlog/spdlog.h>
class TimerSleep{
    // Timer sleepHelper 这种方法精度巨高 但是CPU占用量巨大 15%左右
    // 使用while不断获取时间 精度略次之 大概偏差5%左右,CPU占用量9% 左右
    // 自带的sleep函数和thread的forsleep 一样误差大 synchapi.h sleep函数
    // 最终使用 windows 消息机制配合WaitableTimer
    static int usleep(HANDLE timer, LONGLONG d);

public:
    static int sleep_for_ms(LONGLONG time);
};
