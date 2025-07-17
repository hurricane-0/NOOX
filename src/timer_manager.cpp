#include "timer_manager.h"

// 静态实例，用于让静态 onTimer 回调访问成员函数
static TimerManager* _instance = nullptr;

TimerManager::TimerManager() {
    _instance = this;
}

void TimerManager::begin() {
    // 定时器本身无需特殊初始化，
    // 配置在 setTimer 中完成。
    Serial.println("TimerManager initialized.");
}

void IRAM_ATTR TimerManager::onTimer() {
    // 定时器中断回调
    if (_instance && _instance->_callback) {
        _instance->_callback();
        _instance->_timerRunning = false; // 单次触发后停止定时器
    }
}

void TimerManager::setTimer(long delayMs, TimerCallback callback) {
    // 设置定时器延迟和回调
    _delayMs = delayMs;
    _callback = callback;

    if (timer) {
        timerEnd(timer); // 停止并释放已有定时器
    }

    // 配置定时器 0，80MHz 时钟，分频 80（1us 计数）
    // 80MHz / 80 = 1MHz（1 tick = 1 微秒）
    timer = timerBegin(0, 80, true); // 定时器 0，分频 80，向上计数
    timerAttachInterrupt(timer, &TimerManager::onTimer, true); // 绑定中断，边沿触发
    timerAlarmWrite(timer, _delayMs * 1000, false); // 设置定时器报警（微秒），不自动重载
    Serial.printf("Timer set for %ld ms.\n", _delayMs);
}

void TimerManager::startTimer() {
    // 启动定时器
    if (timer) {
        timerAlarmEnable(timer);
        _timerRunning = true;
        Serial.println("Timer started.");
    } else {
        Serial.println("Timer not set. Call setTimer() first.");
    }
}

void TimerManager::stopTimer() {
    // 停止定时器
    if (timer) {
        timerAlarmDisable(timer);
        _timerRunning = false;
        Serial.println("Timer stopped.");
    }
}

bool TimerManager::isTimerRunning() {
    // 查询定时器是否正在运行
    return _timerRunning;
}
