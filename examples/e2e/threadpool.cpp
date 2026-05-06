//
// Created by christopher.tubbs on 5/5/26.
//

#ifndef E2E_THREADPOOL
#define E2E_THREADPOOL

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>
#include <type_traits>
#include <stdexcept>

/**
 * A pool of threads for executing tasks concurrently.
 */
class ThreadPool {
public:
    explicit ThreadPool(const std::size_t num_threads) : should_stop(false) {
        if (num_threads == 0U) {
            throw std::invalid_argument("num_threads must be greater than 0");
        }

        this->workers.reserve(num_threads);

        for (std::size_t worker_index = 0U; worker_index < num_threads; ++worker_index) {
            this->workers.emplace_back(
                [this]() { this->worker_loop(); }
            );
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(this->queue_lock);
            this->should_stop = true;
        }

        this->new_task_notification.notify_all();

        for (std::thread& worker_thread : this->workers) {
            if (worker_thread.joinable()) {
                worker_thread.join();
            }
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * Submit a callable and its arguments to the thread pool.
     *
     * This is very simple - avoid the heavy use of move only arguments and references
     *
     * Args:
     *  callable: Function, lambda, functor, or other callable object
     *  arguments: Arguments to pass to the callable
     *
     * Returns:
     *  A std::future containing the callable result
     *
     * Throws:
     *  A std::runtime_error: If submission occurs after shutdown has started
     **/
    template<typename CallableType, typename... ArgumentTypes>
    auto submit(
        CallableType&& callable,
        ArgumentTypes&&... arguments
    ) -> std::future<std::invoke_result_t<CallableType, ArgumentTypes...>> {
        using ResultType = std::invoke_result_t<CallableType, ArgumentTypes...>;

        // This will create the container for a function call that will happen eventually.
        // Binding the callable and arguments helps with type safety. Also helps pass it around
        auto packaged_task_pointer = std::make_shared<std::packaged_task<ResultType()>>(
            std::bind(
                std::forward<CallableType>(callable),
                std::forward<ArgumentTypes>(arguments)...
            )
        );

        std::future<ResultType> task_future = packaged_task_pointer->get_future();

        {
            std::lock_guard lock(this->queue_lock);

            if (this->should_stop) {
                throw std::runtime_error("Cannot submit task to a shutdown ThreadPool");
            }

            this->tasks.emplace(
                [packaged_task_pointer]() {
                    (*packaged_task_pointer)();
                }
            );
        }

        this->new_task_notification.notify_one();

        return task_future;
    }
private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock lock(this->queue_lock);

                this->new_task_notification.wait(
                    lock,
                    [this]() {
                        return this->should_stop || !this->tasks.empty();
                    }
                );

                if (this->should_stop && this->tasks.empty()) {
                    return;
                }

                task = std::move(this->tasks.front());
                this->tasks.pop();
            }

            task();
        }
    }

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_lock;
    std::condition_variable new_task_notification;
    bool should_stop;
};


#endif
