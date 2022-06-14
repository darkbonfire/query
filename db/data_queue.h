#ifndef _DATA_QUEUE_H
#define _DATA_QUEUE_H

#include <linux/prctl.h>
#include <sys/prctl.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>


template <typename T>
class DataQueue
{
public:
    explicit DataQueue()
        : m_pop_left(0)
        , m_res_count(0)
        , m_queue_size(0)
        , m_max(0)
    {
    }

    bool Pop(T& data, bool is_sync = true)
    {
        bool wait = false;
        if (!m_queue_size)
        {
            if (IsEmpty())
            {
                return false;
            }
            wait = true;
        }

        if (wait && !is_sync)
        {
            return false;
        }

        {
            std::unique_lock<std::mutex> unique_lock(m_mut);
            if (wait)
            {
                m_cond.wait(unique_lock, [&] { return !m_queue.empty() || IsEmpty(); });
                if (IsEmpty())
                {
                    return false;
                }
            }

            data = m_queue.front();
            m_queue.pop();
            m_queue_size--;
            m_pop_left--;
        }
        return true;
    }

    void Push(const T& res)
    {
        std::lock_guard<std::mutex> lk(m_mut);
        m_res_count--;
        m_queue_size++;
        m_queue.push(res);
        m_cond.notify_one();
    }

    bool IsEmpty() const { return m_res_count <= 0 && m_queue_size <= 0; }

    void SetMax(int64_t max)
    {
        m_max = max;
        m_res_count = max;
        m_pop_left = max;
    }

    void SetEmpty()
    {
        m_res_count = 0;
        m_queue_size = 0;
        m_pop_left = 0;
        m_cond.notify_all();
    }

private:
    std::mutex m_mut;
    std::atomic<int64_t> m_pop_left;
    std::atomic<int64_t> m_res_count;
    std::atomic<int64_t> m_queue_size;
    std::condition_variable m_cond;
    std::queue<T> m_queue;
    int64_t m_max;
};

#endif  // DB_TEST_DATA_QUEUE_H
