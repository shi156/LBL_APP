#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace lbl {

class ThreadPool {
 public:
  // 创建固定大小线程池，worker_count 为工作线程数量。
  explicit ThreadPool(int worker_count);
  // 析构时自动停止线程池并回收线程。
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  // 投递任务到队列，由工作线程异步执行。
  void enqueue(std::function<void()> job);
  // 停止线程池，不再接收新任务，等待已投递任务执行完成。
  void stop();

 private:
  // 单个工作线程主循环：等待任务并执行。
  void workerLoop();

  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> jobs_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stopping_ = false;
};

}  // namespace lbl
