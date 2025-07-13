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
