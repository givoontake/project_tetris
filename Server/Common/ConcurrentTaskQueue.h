#pragma once
#include <atomic>
#include <cstddef>
#include <utility>
#include <immintrin.h>
#include <oneapi/tbb/concurrent_queue.h>

template <typename T>
class ConcurrentTaskQueue
{
	oneapi::tbb::concurrent_queue<T> task_queue;
	std::atomic<std::size_t> task_count{ 0 };

public:
	void Enqueue(T task)
	{
		task_count.fetch_add(1);
		task_queue.push(std::move(task));
	}

	std::size_t ClaimTaskCount()
	{
		return task_count.exchange(0);
	}

	std::size_t GetTaskCount() const
	{
		return task_count.load();
	}

	T Dequeue()
	{
		T task;
		while (!task_queue.try_pop(task)) _mm_pause();
		return task;
	}
};
