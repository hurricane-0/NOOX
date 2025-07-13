#include "timer_manager.h"

// Static instance to allow the static onTimer callback to access member functions
static TimerManager* _instance = nullptr;

TimerManager::TimerManager() {
    _instance = this;
}

void TimerManager::begin() {
    // No specific initialization needed here for the timer itself,
    // as it's configured in setTimer.
    Serial.println("TimerManager initialized.");
}

void IRAM_ATTR TimerManager::onTimer() {
    if (_instance && _instance->_callback) {
        _instance->_callback();
        _instance->_timerRunning = false; // Stop timer after single shot
    }
}

void TimerManager::setTimer(long delayMs, TimerCallback callback) {
    _delayMs = delayMs;
    _callback = callback;

    if (timer) {
        timerEnd(timer); // Stop and free existing timer if any
    }

    // Configure timer 0, 80MHz clock, prescaler 80 (1us tick)
    // 80MHz / 80 = 1MHz (1 tick = 1 microsecond)
    timer = timerBegin(0, 80, true); // timer 0, prescaler 80, count up
    timerAttachInterrupt(timer, &TimerManager::onTimer, true); // attach interrupt, edge triggered
    timerAlarmWrite(timer, _delayMs * 1000, false); // set alarm in microseconds, not auto-reload
    Serial.printf("Timer set for %ld ms.\n", _delayMs);
}

void TimerManager::startTimer() {
    if (timer) {
        timerAlarmEnable(timer);
        _timerRunning = true;
        Serial.println("Timer started.");
    } else {
        Serial.println("Timer not set. Call setTimer() first.");
    }
}

void TimerManager::stopTimer() {
    if (timer) {
        timerAlarmDisable(timer);
        _timerRunning = false;
        Serial.println("Timer stopped.");
    }
}

bool TimerManager::isTimerRunning() {
    return _timerRunning;
}
