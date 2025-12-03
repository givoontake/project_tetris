//#include "MQueue.h"
//#include "MQueue.h"
//
//void MQueue::EnQ(int user_id)
//{
//	std::lock_guard<std::mutex> lock(q_mutex);
//	m_queue.push(user_id);
//}
//
//int MQueue::DeQ()
//{
//	std::lock_guard<std::mutex> lock(q_mutex);
//	if (m_queue.empty())
//		return -1;
//
//	int value = m_queue.front();
//	m_queue.pop();
//	return value;
//}
//
//bool MQueue::IsEmpty()
//{
//	std::lock_guard<std::mutex> lock(q_mutex);
//	return m_queue.empty();
//}
//
