/**
 * @file timer_manager.h
 * @brief ESP32硬件定时器管理类
 *
 * TimerManager 用于管理ESP32的硬件定时器，支持设置定时回调、启动/停止定时器等功能。
 * 主要接口：
 *   - setTimer(long delayMs, TimerCallback callback): 设置定时器延时和回调函数
 *   - startTimer(): 启动定时器
 *   - stopTimer(): 停止定时器
 *   - isTimerRunning(): 查询定时器是否正在运行
 *
 * 用法示例：
 *   TimerManager tm;
 *   tm.begin();
 *   tm.setTimer(1000, myCallback);
 *   tm.startTimer();
 */

#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <Arduino.h>

// Define a callback type for the timer
typedef void (*TimerCallback)();

class TimerManager {
public:
    TimerManager();
    void begin();
    void setTimer(long delayMs, TimerCallback callback);
    void startTimer();
    void stopTimer();
    bool isTimerRunning();

private:
    hw_timer_t * timer = NULL; // ESP32 hardware timer
    TimerCallback _callback = nullptr;
    long _delayMs = 0;
    volatile bool _timerRunning = false;

    static void IRAM_ATTR onTimer(); // Static callback for hardware timer
};

#endif // TIMER_MANAGER_H
