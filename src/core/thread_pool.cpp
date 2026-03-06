#include "core/thread_pool.h"

#include <stdexcept>

namespace lbl {

// 创建线程池并启动 worker_count 个工作线程。
ThreadPool::ThreadPool(int worker_count) {
  if (worker_count <= 0) {
    throw std::invalid_argument("worker_count must be > 0");
  }

  workers_.reserve(static_cast<std::size_t>(worker_count));
  for (int i = 0; i < worker_count; ++i) {
    workers_.emplace_back([this]() { workerLoop(); });
  }
}

// 析构时确保线程池停止，避免线程泄漏。
ThreadPool::~ThreadPool() {
  stop();
}

// 投递一个任务到队列。
void ThreadPool::enqueue(std::function<void()> job) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) {
      return;
    }
    jobs_.push(std::move(job));
  }
  cv_.notify_one();
}

// 停止线程池并等待线程退出。
void ThreadPool::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) {
      return;
    }
    stopping_ = true;
  }

  cv_.notify_all();
  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  workers_.clear();
}

// 工作线程循环：等待任务、执行任务。
void ThreadPool::workerLoop() {
  while (true) {
    std::function<void()> job;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]() { return stopping_ || !jobs_.empty(); });
      if (stopping_ && jobs_.empty()) {
        return;
      }
      job = std::move(jobs_.front());
      jobs_.pop();
    }

    try {
      job();
    } catch (...) {
      // Avoid crashing worker thread on unexpected request-level exceptions.
    }
  }
}

}  // namespace lbl
