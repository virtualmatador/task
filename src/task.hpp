#ifndef TASK_H
#define TASK_H

#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <tuple>

#include <boost/asio.hpp>

template<class I, class O>
class task
{
private:
    boost::asio::io_context& the_io_;
    std::function<void()> worker_;
    std::size_t index_;
    std::size_t progress_;
    std::map<std::size_t, std::tuple<
        I, std::function<void(O&&)>, boost::asio::deadline_timer>> tasks_;
    std::mutex tasks_lock_;

public:
    task(boost::asio::io_context& the_io, std::function<void()>&& worker) :
        the_io_{ the_io },
        worker_{ std::move(worker) },
        index_{ 0 },
        progress_{ 0 }
    {
    }

    ~task()
    {
    }

    std::size_t push(I&& input, std::size_t timeout_seconds,
        std::function<void(O&&)>&& handler)
    {
        std::unique_lock auto_lock{ tasks_lock_ };
        auto id = ++index_;
        boost::asio::deadline_timer expired{
            the_io_, boost::posix_time::seconds(timeout_seconds) };
        expired.async_wait([id, this]
            (const boost::system::error_code& ec)
        {
            if (!ec)
            {
                respond(id, O());
            }
        });
        bool trigger_worker = tasks_.empty();
        tasks_.insert({ id, std::make_tuple(
            std::move(input), std::move(handler), std::move(expired)) });
        auto_lock.unlock();
        if (trigger_worker)
        {
            worker_();
        }
        return id;
    }

    std::optional<std::pair<std::size_t, I>> front()
    {
        std::unique_lock auto_lock{ tasks_lock_ };
        auto task = tasks_.lower_bound(progress_);
        if (task != tasks_.end())
        {
            progress_ = task->first;
            return { { progress_, std::get<0>(task->second) } };
        }
        return {};
    }

    void step()
    {
        std::unique_lock auto_lock{ tasks_lock_ };
        progress_ += 1;
    }

    void cancel(std::size_t& id)
    {
        if (id)
        {
            std::unique_lock auto_lock{ tasks_lock_ };
            auto task = tasks_.find(id);
            if (task != tasks_.end())
            {
                tasks_.erase(task);
            }
            id = 0;
        }
    }

    void respond(std::size_t id, O&& payload)
    {
        std::unique_lock auto_lock{ tasks_lock_ };
        auto task = tasks_.find(id);
        if (task != tasks_.end())
        {
            auto handler = std::move(std::get<1>(task->second));
            tasks_.erase(task);
            auto_lock.unlock();
            try
            {
                handler(std::move(payload));
            }
            catch(const std::exception& e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }
    }
};

#endif // TASK_H
