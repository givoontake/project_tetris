#pragma once
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

class LatencyRecorder
{
	std::string output_directory_;
	std::chrono::steady_clock::time_point finish_time_;
	std::vector<long long> latency_samples_;
	std::map<int, std::size_t> server_error_counts_;
	std::mutex state_mutex_;
	std::mutex samples_mutex_;
	std::mutex server_error_mutex_;
	std::atomic<bool> is_started_{ false };
	std::atomic<bool> is_finishing_{ false };
	std::atomic<bool> is_finished_{ false };

public:
	LatencyRecorder(std::string output_directory);

	bool TryStart();
	void Record(long long latency_ms);
	void RecordServerError(int error_code);
	bool TryFinish();
	bool IsStarted() const { return is_started_.load(); }
	bool IsFinished() const { return is_finished_.load(); }

private:
	void SaveResult();
	long long CalculatePercentile(const std::vector<long long>& sorted_samples, double percentile) const;
};
