#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(int n_threads)
    {
        workers.reserve(n_threads);
        for (int i = 0; i < n_threads; ++i)
            workers.emplace_back([this] { worker_loop(); });
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stopping = true;
        }
        cv_task.notify_all();
        for (auto& t : workers) t.join();
    }

    void submit(std::function<void()> f)
    {
        {
            std::lock_guard<std::mutex> lock(mtx);
            ++pending;
            tasks.push_back(std::move(f));
        }
        cv_task.notify_one();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv_done.wait(lock, [this] { return pending == 0; });
    }

private:
    void worker_loop()
    {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv_task.wait(lock, [this] { return !tasks.empty() || stopping; });
                if (stopping && tasks.empty()) return;
                task = std::move(tasks.front());
                tasks.pop_front();
            }
            task();
            {
                std::lock_guard<std::mutex> lock(mtx);
                if (--pending == 0) cv_done.notify_all();
            }
        }
    }

    std::vector<std::thread>          workers;
    std::deque<std::function<void()>> tasks;
    std::mutex                        mtx;
    std::condition_variable           cv_task;
    std::condition_variable           cv_done;
    int                               pending  = 0;
    bool                              stopping = false;
};
