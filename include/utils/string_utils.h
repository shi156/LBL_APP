#pragma once

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace lbl::utils {

// 将字符串转为小写。
inline std::string toLower(std::string input) {
  std::transform(input.begin(), input.end(), input.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return input;
}

// 去除字符串首尾空白字符。
inline std::string trim(std::string s) {
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

// 对 JSON 字符串内容进行转义，避免响应体格式错误。
inline std::string jsonEscape(const std::string& input) {
  std::ostringstream out;
  for (const auto c : input) {
    switch (c) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\b':
        out << "\\b";
        break;
      case '\f':
        out << "\\f";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(static_cast<unsigned char>(c));
        } else {
          out << c;
        }
        break;
    }
  }
  return out.str();
}

// 将字节数组编码为十六进制字符串。
inline std::string bytesToHex(const unsigned char* bytes, std::size_t len) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < len; ++i) {
    out << std::setw(2) << static_cast<int>(bytes[i]);
  }
  return out.str();
}

// 将十六进制字符串解码为字节数组。
inline std::vector<unsigned char> hexToBytes(const std::string& hex) {
  if (hex.size() % 2 != 0) {
    return {};
  }

  std::vector<unsigned char> result;
  result.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    try {
      const auto byte = static_cast<unsigned char>(std::stoul(hex.substr(i, 2), nullptr, 16));
      result.push_back(byte);
    } catch (...) {
      return {};
    }
  }
  return result;
}

// 从简单 JSON（固定 key/value 字符串）中提取字段。
inline std::optional<std::string> extractJsonString(const std::string& json,
                                                    const std::string& key) {
  const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
  std::smatch match;
  if (!std::regex_search(json, match, pattern) || match.size() < 2) {
    return std::nullopt;
  }
  return match[1].str();
}

// 判断 value 是否以 prefix 开头。
inline bool startsWith(const std::string& value, const std::string& prefix) {
  return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace lbl::utils
