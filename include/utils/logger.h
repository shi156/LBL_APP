#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace lbl::logger {

// 生成本地时间字符串，格式：YYYY-MM-DD HH:MM:SS.mmm
inline std::string nowText() {
  const auto now = std::chrono::system_clock::now();
  const auto now_time_t = std::chrono::system_clock::to_time_t(now);
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

  std::tm tm_buf{};
#if defined(_WIN32)
  localtime_s(&tm_buf, &now_time_t);
#else
  localtime_r(&now_time_t, &tm_buf);
#endif

  std::ostringstream out;
  out << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3)
      << std::setfill('0') << ms.count();
  return out.str();
}

// 确保日志目录存在。
inline void ensureLogDir() {
  std::error_code ec;
  std::filesystem::create_directories("logs", ec);
}

// 写入指定日志文件，内部加锁保证多线程安全。
inline void writeLine(const std::string& file_name, const std::string& line) {
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);

  ensureLogDir();
  std::ofstream out("logs/" + file_name, std::ios::app);
  if (!out.is_open()) {
    return;
  }
  out << line << '\n';
}

// 记录通用信息日志（同时输出到控制台）。
inline void info(const std::string& message) {
  const std::string line = nowText() + " [INFO] " + message;
  writeLine("server.log", line);
  std::cout << line << std::endl;
}

// 记录错误日志（同时输出到错误日志文件）。
inline void error(const std::string& message) {
  const std::string line = nowText() + " [ERROR] " + message;
  writeLine("server.log", line);
  writeLine("error.log", line);
  std::cerr << line << std::endl;
}

// 记录访问日志。
// client_ip/method/path/status_code/cost_ms: 请求关键指标；note: 附加说明。
inline void access(const std::string& client_ip,
                   const std::string& method,
                   const std::string& path,
                   int status_code,
                   long long cost_ms,
                   const std::string& note = "") {
  std::ostringstream out;
  out << nowText() << " [ACCESS] ip=" << client_ip << " method=" << method
      << " path=" << path << " status=" << status_code << " cost_ms=" << cost_ms;
  if (!note.empty()) {
    out << " note=" << note;
  }
  writeLine("access.log", out.str());
}

}  // namespace lbl::logger
