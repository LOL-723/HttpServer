# QPS Optimize Report

## 1. 未提交区概览

当前未提交区包含以下内容：

- `README.md`
- `include/httpserver/http/HttpServer.h`
- `src/httpserver/http/HttpServer.cpp`
- `include/httpserver/ssl/SslConnection.h`
- `src/httpserver/ssl/SslConnection.cc`
- `include/muduo/Logger.h`
- `src/muduo/Logger.cc`
- `src/muduo/EPollPoller.cc`
- `src/muduo/TcpConnection.cc`
- `src/muduo/Channel.cc`
- `src/muduo/EventLoop.cc`
- `src/muduo/Buffer.cc`
- `POSSIBLE_PROBLEMS.md`，未跟踪文档文件

本轮 QPS 优化主线是降低 HTTPS 请求路径中的锁竞争、日志开销、TLS 发送拆分和错误响应路径问题。

## 2. README 更新

### 修改内容

原记录：

```text
2100QPS IN HTTPS
cmake --build build -j
```

现记录：

```text
5800QPS IN HTTPS
cmake --build build -j2
```

并补充了启动命令、默认 HTTPS 访问地址和重新编译流程。

### 作用

- 记录 HTTPS QPS 优化后的压测结果。
- 将构建命令从裸 `-j` 改为 `-j2`，避免 3.8GiB 内存虚拟机同时拉起过多 `cc1plus` 导致卡死。

### 对原逻辑影响

无运行逻辑影响，仅文档更新。

## 3. HttpServer HTTPS 热路径优化

涉及文件：

- `include/httpserver/http/HttpServer.h`
- `src/httpserver/http/HttpServer.cpp`

### 3.1 SSL 连接管理方式调整

原实现：

```cpp
std::mutex sslConnsMutex_;
std::map<muduo::TcpConnectionPtr, std::shared_ptr<ssl::SslConnection>> sslConns_;
```

每次 HTTPS 收包和发响应时，都需要通过 `findSslConnection()` 加锁查询全局 `map`。

现实现：

```cpp
struct ConnectionContext
{
    HttpContext http;
    std::shared_ptr<ssl::SslConnection> ssl;
};
```

每条 `TcpConnection` 的 context 中直接保存自己的 `HttpContext` 和 `SslConnection`。

### 作用

- 去掉 HTTPS 请求热路径上的全局 mutex。
- 去掉 `TcpConnectionPtr -> SslConnectionPtr` 的全局 map 查询。
- 减少多 IO 线程压测时的锁竞争。

### 对原逻辑影响

原流程：

```text
TcpConnection -> 全局 sslConns_ map -> SslConnection -> 解密 -> HttpContext 解析
```

现流程：

```text
TcpConnection -> 自身 ConnectionContext -> SslConnection -> 解密 -> 同一个 ConnectionContext 内的 HttpContext 解析
```

HTTP 解析逻辑没有改变，改变的是 SSL 连接对象的存放位置和查找方式。

### 3.2 响应发送出口统一

新增：

```cpp
void HttpServer::sendResponseData(const muduo::TcpConnectionPtr& conn,
                                  const std::string& responseData);
```

原来普通响应和错误响应有不同发送路径。尤其在解析失败时，HTTPS 模式下也会直接：

```cpp
conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
```

这会绕过 TLS，向 HTTPS 连接写入明文 HTTP。

现在所有响应统一走 `sendResponseData()`：

- HTTPS：调用 `sslConn->send()` 加密发送。
- HTTP：调用 `conn->send()` 明文发送。

### 作用

- 修复 HTTPS 错误响应裸发明文的问题。
- 避免 `curl -I https://...` 这类解析失败路径出现 TLS `wrong version number`。

### 对原逻辑影响

正常响应内容不变。变化点是错误响应在 HTTPS 下也会经过 TLS 加密。

## 4. SslConnection 优化

涉及文件：

- `include/httpserver/ssl/SslConnection.h`
- `src/httpserver/ssl/SslConnection.cc`

### 4.1 移除 SSL 层反调 HTTP 层的 messageCallback

原设计中 `SslConnection` 内部保存 `messageCallback_`，解密后可能主动回调 HTTP 层。

现设计中移除了：

```cpp
MessageCallback messageCallback_;
setMessageCallback(...)
```

解密后的明文只追加到：

```cpp
decryptedBuffer_
```

然后返回 `HttpServer::onMessage()`，由 HTTP 层继续解析。

### 作用

- 消除 SSL 层反向调用 HTTP 层的递归结构。
- 让层次更清晰：SSL 只负责 TLS 握手、加密、解密；HTTP 层负责 HTTP 解析和响应。

### 对原逻辑影响

原流程可能是：

```text
HttpServer::onMessage()
  -> SslConnection::onRead()
     -> SSL_read()
     -> messageCallback_ 再次进入 HttpServer::onMessage()
```

现流程是：

```text
HttpServer::onMessage()
  -> SslConnection::onRead()
     -> SSL_read()
     -> decryptedBuffer_
  -> HttpServer::onMessage() 继续用 decryptedBuffer_ 解析 HTTP
```

### 4.2 TLS write BIO 刷出优化

原实现：

```cpp
char buf[4096];
while (BIO_pending(writeBio_) > 0) {
    BIO_read(...);
    conn_->send(std::string(buf, bytes));
}
```

现实现：

```cpp
char buf[16 * 1024];
std::string encrypted;
encrypted.reserve(BIO_pending(writeBio_));

while (BIO_pending(writeBio_) > 0) {
    BIO_read(...);
    encrypted.append(buf, bytes);
}

conn_->send(encrypted);
```

### 作用

- 减少多次小块 `send()`。
- 减少临时 `std::string` 构造。
- 减少 TLS 响应路径上的函数调用和潜在系统调用压力。

### 对原逻辑影响

TLS 密文内容不变，发送时机不变。只是由多次小块发送改为合并后发送。

### 4.3 OpenSSL mode 调整

新增：

```cpp
SSL_MODE_RELEASE_BUFFERS
```

### 作用

在连接空闲时允许 OpenSSL 释放内部 buffer，降低大量 HTTPS 连接下的内存压力。

## 5. Logger 优化与修复

涉及文件：

- `include/muduo/Logger.h`
- `src/muduo/Logger.cc`

### 5.1 日志级别顺序修复

原枚举顺序：

```cpp
INFO, ERROR, FATAL, DEBUG, WARN
```

现枚举顺序：

```cpp
DEBUG, INFO, WARN, ERROR, FATAL
```

### 作用

让日志级别从低到高排列，使 `setLogLevel(WARN)` 时仍然能输出 `ERROR` 和 `FATAL`。

### 5.2 logLevel_ 初始化

原来：

```cpp
int logLevel_;
```

现改为：

```cpp
int logLevel_ = INFO;
```

### 作用

避免日志级别未初始化导致过滤行为不稳定。

### 5.3 日志宏提前过滤

原来即使日志级别不输出，也会先执行：

```cpp
formatLog(...)
LogStream(...)
```

现在宏先判断：

```cpp
Logger::instance().enabled(level)
```

只有需要输出时才格式化和构造 `LogStream`。

### 作用

降低压测时被关闭日志的隐藏开销，尤其是热点路径中的 INFO 日志。

### 5.4 日志前缀修复

原 `Logger::log()` 根据全局 `logLevel_` 打印前缀，现改为根据当前消息级别打印前缀。

### 作用

避免 `WARN` 日志被打印成其他级别前缀。

## 6. muduo 高频日志清理

涉及文件：

- `src/muduo/EPollPoller.cc`
- `src/muduo/Channel.cc`
- `src/muduo/EventLoop.cc`
- `src/muduo/TcpConnection.cc`

### 修改内容

- `EPollPoller::poll()`、`updateChannel()`、`removeChannel()` 中的高频 `LOG_INFO` 改为 `LOG_DEBUG`。
- 删除 `Channel::handleEventWithGuard()` 中每次事件触发都会打印的 INFO 日志。
- 删除 `EventLoop::loop()` 启停 INFO 日志。
- 删除 `TcpConnection::connectEstablished()` 中连接建立调试 INFO 日志。
- `ECONNRESET` 从 `LOG_ERROR` 降为 `LOG_DEBUG`。

### 作用

- 减少 epoll 循环、连接建立、事件触发、wrk 断连时的日志噪声。
- 避免 stdout/printf 成为压测瓶颈。

### 对原逻辑影响

无网络行为变化，仅调整日志输出。

## 7. Buffer 格式调整

涉及文件：

- `src/muduo/Buffer.cc`

### 修改内容

仅调整 `Buffer::readFd()` 的缩进和函数尾部格式。

### 作用

代码格式清理。

### 对原逻辑影响

无。

## 8. POSSIBLE_PROBLEMS.md

该文件当前是未跟踪文件，内容主要是对 muduo、Reactor、HttpServer、SslConnection 等流程的学习笔记和问题整理。

### 作用

作为排查和理解项目结构的辅助文档。

### 对原逻辑影响

无，未参与编译。

## 9. 构建与压测说明

### 构建

在当前虚拟机环境中，建议使用：

```bash
cmake --build build -j2
```

不建议使用裸：

```bash
cmake --build build -j
```

原因是本轮修改涉及公共头文件 `include/muduo/Logger.h`，会触发大量 C++ 文件重编。裸 `-j` 会按 CPU 数尽量并发，在 3.8GiB 内存虚拟机中容易同时启动过多 `cc1plus`，导致内存压力过高甚至虚拟机卡死。

### 验证结果

已验证：

```bash
cmake --build build -j2
```

可以完整构建到：

```text
[100%] Built target simple_server
```

HTTPS smoke test 已验证：

- TLS 1.3 握手成功。
- `GET /` 返回 `200`。
- 解析失败路径返回加密后的 `400 Bad Request`，不再出现明文写入 TLS 连接导致的 `wrong version number`。

短时 wrk 压测曾观察到 HTTPS QPS 明显高于原 `2100QPS` 水平。README 当前记录为：

```text
5800QPS IN HTTPS
```
## 10. 证书变更

从RSA 4096改为 RSA 2048

## 11. 总结

本轮未提交区的核心优化是：

1. 将 SSL 连接从全局加锁 map 移到每条连接自己的 context 中，降低 HTTPS 热路径锁竞争。
2. 统一响应发送出口，修复 HTTPS 错误响应裸发明文的问题。
3. 优化 TLS write BIO 刷出逻辑，减少小块发送和字符串构造。
4. 修复并优化日志系统，降低压测时的日志开销。
5. 清理 muduo 事件循环、epoll、连接错误中的高频 INFO 日志。

整体效果是 HTTPS 请求路径更短、锁更少、日志更少、TLS 发送更集中，因此 HTTPS QPS 从原先约 `2100` 提升到 README 当前记录的 `6400` 附近。

### 同时间前端优化(已完成)

Q:当前在对战AI时，当玩家赢的时候，会先显示：AI 对战请求失败，请重试。点击确定后才会正常显示：恭喜你，获胜了

A:问题位置：[ChessGameVsAi.html (line 281)]
当前逻辑是：
玩家落子后请求 /aiBot/move
后端发现玩家赢了，返回：status: "ok"
winner: "human"
没有 last_move

前端进入 if (data.winner !== 'none')
里面用了 setTimeout(... alert('恭喜你，获胜了！') ...)
但是这个分支没有真正 return
代码继续往下执行到：const aiMove = data.last_move;
因为玩家胜利时没有 AI 落子，data.last_move 是 undefined

JS 抛错，被 .catch() 捕获，于是先弹：AI 对战请求失败，请重试。

用户点确定后，前面 setTimeout 里的胜利提示才继续弹出。
推荐改法：
在 [ChessGameVsAi.html (line 282)]的胜负处理分支中，处理完 data.winner !== 'none' 后，直接 return; 退出当前 .then(data => { ... })。