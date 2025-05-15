#pragma once
#include <queue>
#include <mutex>

class MQueue
{
	std::queue<int> m_queue;
	std::mutex q_mutex;

public:
	void EnQ(int user_id);
	int DeQ();
	bool IsEmpty();
};

