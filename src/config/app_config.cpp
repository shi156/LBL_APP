#include "config/app_config.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace lbl {
namespace {

// 去除字符串首尾空白。
std::string trim(std::string s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

// 安全解析 int，要求整串都为数字内容。
bool parseInt(const std::string& value, int& out) {
  try {
    std::size_t idx = 0;
    out = std::stoi(value, &idx);
    return idx == value.size();
  } catch (...) {
    return false;
  }
}

// 解析端口号（1~65535）。
bool parseU16(const std::string& value, std::uint16_t& out) {
  int tmp = 0;
  if (!parseInt(value, tmp) || tmp < 1 || tmp > 65535) {
    return false;
  }
  out = static_cast<std::uint16_t>(tmp);
  return true;
}

}  // namespace

// 从配置文件加载系统配置。
bool loadAppConfig(const std::string& path, AppConfig& config, std::string& error) {
  std::ifstream in(path);
  if (!in.is_open()) {
    error = "Failed to open config file: " + path;
    return false;
  }

  std::unordered_map<std::string, std::string> values;
  std::string line;
  int line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    auto trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
      continue;
    }

    const auto eq = trimmed.find('=');
    if (eq == std::string::npos) {
      error = "Invalid config line " + std::to_string(line_no) + ": " + line;
      return false;
    }

    const auto key = trim(trimmed.substr(0, eq));
    const auto value = trim(trimmed.substr(eq + 1));
    if (key.empty()) {
      error = "Empty key at line " + std::to_string(line_no);
      return false;
    }

    values[key] = value;
  }

  auto getValue = [&values](const std::string& key) -> std::string {
    auto it = values.find(key);
    if (it == values.end()) {
      return "";
    }
    return it->second;
  };

  auto useString = [&getValue](const std::string& key, std::string& target) {
    const auto value = getValue(key);
    if (!value.empty()) {
      target = value;
    }
  };

  auto useInt = [&getValue, &error](const std::string& key, int& target) -> bool {
    const auto value = getValue(key);
    if (value.empty()) {
      return true;
    }
    int parsed = 0;
    if (!parseInt(value, parsed)) {
      error = "Invalid int for key: " + key;
      return false;
    }
    target = parsed;
    return true;
  };

  auto usePort = [&getValue, &error](const std::string& key, std::uint16_t& target) -> bool {
    const auto value = getValue(key);
    if (value.empty()) {
      return true;
    }
    std::uint16_t parsed = 0;
    if (!parseU16(value, parsed)) {
      error = "Invalid port for key: " + key;
      return false;
    }
    target = parsed;
    return true;
  };

  // 以下参数均支持通过配置覆盖默认值。
  useString("server.host", config.server.host);
  if (!usePort("server.port", config.server.port)) {
    return false;
  }
  if (!useInt("server.worker_threads", config.server.worker_threads)) {
    return false;
  }
  if (!useInt("server.epoll_max_events", config.server.epoll_max_events)) {
    return false;
  }
  if (!useInt("server.socket_backlog", config.server.socket_backlog)) {
    return false;
  }

  useString("tls.cert_file", config.tls.cert_file);
  useString("tls.key_file", config.tls.key_file);

  useString("db.host", config.database.host);
  if (!usePort("db.port", config.database.port)) {
    return false;
  }
  useString("db.user", config.database.user);
  useString("db.password", config.database.password);
  useString("db.database", config.database.database);
  useString("db.charset", config.database.charset);

  if (!useInt("auth.token_expire_seconds", config.auth.token_expire_seconds)) {
    return false;
  }
  if (!useInt("auth.pbkdf2_iterations", config.auth.pbkdf2_iterations)) {
    return false;
  }

  // 关键参数校验：防止线程数、过期时间等配置错误导致运行异常。
  if (config.server.worker_threads <= 0) {
    error = "server.worker_threads must be > 0";
    return false;
  }
  if (config.auth.token_expire_seconds <= 0) {
    error = "auth.token_expire_seconds must be > 0";
    return false;
  }
  if (config.auth.pbkdf2_iterations < 10000) {
    error = "auth.pbkdf2_iterations must be >= 10000";
    return false;
  }

  return true;
}

}  // namespace lbl
