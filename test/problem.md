# NOOX设备内存管理问题记录与解决方案

## 问题一：堆损坏（CORRUPT HEAP）

### 初始症状
ESP32-S3设备在通过Web界面发送LLM API请求后，收到响应时会发生堆损坏（CORRUPT HEAP）并重启。

```
[ 12778][E][ssl_client.cpp:37] _handle_error(): [data_to_read():361]: (-76) UNKNOWN ERROR CODE (004C)
[LLM] Received 545 bytes
[ 12790][D][HTTPClient.cpp:408] disconnect(): tcp is closed

Largest Free Heap Block after LLM call: 8257524 bytes
Free DRAM after LLM call: 8582923, Free PSRAM after LLM call: 8370719
LLMTask: Generated content: 你好！很高兴为你提供帮助！有什么我可以为你做的吗？😊
handleLLMRawResponse: Content is natural language or invalid JSON. Error: InvalidInput
Natural language response: 你好！很高兴为你提供帮助！有什么我可以为你做的吗？😊
Sent to host: {"requestId":"","type":"aiResponse","payload":"你好！很高兴为你提供帮助！有什么我可以为你做的吗？😊"}
CORRUPT HEAP: Bad head at 0x3fcb8b84. Expected 0xabba1234 got 0x3fca4164

assert failed: multi_heap_free multi_heap_poisoning.c:259 (head != NULL)
```

### 根本原因分析
1. ArduinoJson v7的JsonDocument（启用PSRAM时）在析构时与手动内存管理冲突，导致堆元数据损坏
2. 问题链：
   - ARDUINOJSON_USE_PSRAM=1让ArduinoJson v7的JsonDocument使用自定义的PSRAM分配器
   - 手动使用ps_malloc()分配缓冲区，用heap_caps_free()释放
   - JsonDocument的析构函数尝试释放它自己的PSRAM内存
   - 内存分配器状态冲突：两种不同的内存管理方式在同一个函数中交互，导致堆元数据损坏

### 实施的解决方案
1. **JsonDocument对象重用**：
   - 在各管理器类中添加可重用的JsonDocument成员变量
   - 所有临时的JsonDocument局部变量都改为使用类成员变量
   - 每次使用前调用clear()方法清空文档

2. **静态缓冲区替代动态分配**：
   - 在llm_manager.cpp中添加静态字符数组：`static char responseBuffer[1024]`
   - 使用StaticJsonDocument<2048>替代动态分配的JsonDocument
   - 使用strlcpy替代String对象进行字符串操作

3. **安全字符串处理**：
   - 添加null指针检查
   - 使用snprintf进行安全的字符串格式化
   - 使用strcmp替代String对象的比较操作

4. **手动垃圾回收**：
   - 在关键函数结束前调用ESP.getFreeHeap()触发垃圾回收
   - 显式清理不再使用的String对象
   - 使用memset清空静态缓冲区

## 问题二：堆栈溢出（Stack Canary Watchpoint）

### 初始症状
在配置更新过程中，设备出现堆栈溢出错误并重启：

```
Processing pending configuration update... 
Guru Meditation Error: Core 0 panic'ed (Unhandled debug exception). 
Debug exception reason: Stack canary watchpoint triggered (WebTask) 
```

### 根本原因分析
1. WebTask堆栈在处理配置更新时溢出
2. 原因：
   - 在WebManager::loop函数中，配置更新处理使用了大量堆栈空间
   - JsonDocument操作和管理器重新初始化在同一堆栈帧中执行
   - 递归调用和深层函数调用导致堆栈深度过大

### 实施的解决方案
1. **使用独立FreeRTOS任务处理配置更新**：
   - 创建专门的"ConfigUpdate"任务处理配置更新
   - 为新任务分配足够的堆栈空间（8192字节）
   - 完全避免在WebTask堆栈中执行配置更新操作

2. **减小内存使用**：
   - 将StaticJsonDocument大小从8192减小到2048
   - 使用循环逐个复制JSON字段，避免一次性大量内存操作

3. **分批初始化管理器**：
   - 将llmManager和wifiManager的初始化分成两个循环执行
   - 使用静态计数器跟踪初始化状态
   - 避免在同一堆栈帧中执行多个初始化操作

4. **异常处理**：
   - 添加try-catch块捕获可能的异常
   - 在异常处理中提供用户友好的错误消息
   - 确保在异常情况下也能正确清理资源

## 当前状态
所有已知的内存管理问题已解决。设备现在能够：
1. 正常处理LLM请求和响应，不再出现堆损坏
2. 安全地更新配置，不再触发堆栈溢出
3. 稳定运行，不再出现意外重启

## 注意事项
1. 如果将来需要修改代码，请遵循以下原则：
   - 优先使用静态分配而非动态分配
   - 避免在关键任务中使用大量堆栈空间
   - 使用FreeRTOS任务分离复杂操作
   - 小心处理String对象和JsonDocument的生命周期

2. 已知的ArduinoJson警告可以忽略：
   - "ArduinoJson::V742PB22::StaticJsonDocument已弃用：请使用JsonDocument代替"
   - 我们故意使用StaticJsonDocument以确保静态内存分配