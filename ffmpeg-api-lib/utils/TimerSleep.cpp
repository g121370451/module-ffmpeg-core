#include "TimerSleep.h"

int TimerSleep::usleep(HANDLE timer, LONGLONG d) {
    LARGE_INTEGER liDueTime;
    DWORD ret;
    LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
    LARGE_INTEGER Frequency;

    QueryPerformanceFrequency(&Frequency);
    QueryPerformanceCounter(&StartingTime);

    liDueTime.QuadPart = d;
    liDueTime.QuadPart = liDueTime.QuadPart * 10 * 1000;    // us into 100 of ns units
    liDueTime.QuadPart = -liDueTime.QuadPart;    // negative for relative dure time

    if (!SetWaitableTimer(timer, &liDueTime, 0, NULL, NULL, 0)) {
        spdlog::error("SetWaitableTimer failed: errno={}", GetLastError());
        return 1;
    }

    ret = WaitForSingleObject(timer, INFINITE);
    if (ret != WAIT_OBJECT_0) {
        spdlog::error("WaitForSingleObject failed: ret={} error={}", ret, GetLastError());
        return 1;
    }

    QueryPerformanceCounter(&EndingTime);
    ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
    ElapsedMicroseconds.QuadPart *= 1000;
    ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

    spdlog::debug("delay is {} ms - slept for {} ms", d, ElapsedMicroseconds.QuadPart);
    return 0;
}

int TimerSleep::sleep_for_ms(LONGLONG time) {
    HANDLE timer;

    timer = CreateWaitableTimerEx(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (timer == NULL) {
        spdlog::error("CreateWaitableTimerEx failed: errno={}", GetLastError());
        return 1;
    }
    TimerSleep::usleep(timer, time);
    CloseHandle(timer);
    return 0;
}
