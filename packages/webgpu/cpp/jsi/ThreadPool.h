#pragma once

#include <vector>
#include <queue>
#include <thread>

namespace rnwgpu {
    class ThreadPool {
        private:
            std::vector<std::thread> workers;
            std::queue<std::function<void()>> tasks;
            std::mutex queue_mutex;
            std::condition_variable condition;
            bool stop;
        
        public:
            explicit ThreadPool(size_t threads = 1) : stop(false) {
                for(size_t i = 0; i < threads; ++i) {
                    workers.emplace_back([this] {
                        while(true) {
                            std::function<void()> task;
                            {
                                std::unique_lock<std::mutex> lock(queue_mutex);
                                condition.wait(lock, [this] { 
                                    return stop || !tasks.empty(); 
                                });
                                if(stop && tasks.empty()) {
                                    return;
                                }
                                task = std::move(tasks.front());
                                tasks.pop();
                            }
                            task();
                        }
                    });
                }
            }
        
            template<class F>
            void enqueue(F&& f) {
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    tasks.emplace(std::forward<F>(f));
                }
                condition.notify_one();
            }
        
            ~ThreadPool() {
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    stop = true;
                }
                condition.notify_all();
                for(std::thread &worker: workers) {
                    worker.join();
                }
            }
        };
        
        // Singleton thread pool instance
        class GlobalThreadPool {
        public:
            static ThreadPool& getInstance() {
                static ThreadPool instance(4); // Create a pool with 4 threads
                return instance;
            }
        };
        
} // namespace rnwgpu