#pragma once
#include <queue>
#include <mutex>

template <typename T>
class MQueue
{
private:
    std::queue<T> m_queue;
    std::mutex q_mutex;

public:
    // Enqueue
    void EnQ(const T& value)
    {
        std::lock_guard<std::mutex> lock(q_mutex);
        m_queue.push(value);
    }

    // Dequeue (큐가 비었으면 기본값 반환)
    T DeQ()
    {
        std::lock_guard<std::mutex> lock(q_mutex);
        if (m_queue.empty())
            return T();  // 기본값(T()) 반환

        T value = m_queue.front();
        m_queue.pop();
        return value;
    }

    // Is queue empty?
    bool IsEmpty()
    {
        std::lock_guard<std::mutex> lock(q_mutex);
        return m_queue.empty();
    }

    void Swap(MQueue<T>& other)
    {
        std::lock_guard<std::mutex> lock(q_mutex);
		std::lock_guard<std::mutex> other_lock(other.q_mutex);
        std::swap(m_queue, other.m_queue);
    }

};

