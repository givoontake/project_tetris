#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <utility>
#include "define.h"
#include "LatencyRecorder.h"

LatencyRecorder::LatencyRecorder(std::string output_directory)
	: output_directory_(std::move(output_directory))
{
}

bool LatencyRecorder::TryStart()
{
	std::lock_guard<std::mutex> lock(state_mutex_);
	if (is_started_.load()) return false;
	finish_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(MEASUREMENT_SECONDS);
	is_started_.store(true);
	return true;
}

void LatencyRecorder::Record(long long latency_ms)
{
	if (!is_started_.load() || is_finishing_.load() || std::chrono::steady_clock::now() >= finish_time_) return;
	std::lock_guard<std::mutex> lock(samples_mutex_);
	latency_samples_.emplace_back(latency_ms);
}

void LatencyRecorder::RecordServerError(int error_code)
{
	std::lock_guard<std::mutex> lock(server_error_mutex_);
	++server_error_counts_[error_code];
}

bool LatencyRecorder::TryFinish()
{
	if (!is_started_.load() || std::chrono::steady_clock::now() < finish_time_) return false;
	bool expected = false;
	if (!is_finishing_.compare_exchange_strong(expected, true)) return false;
	SaveResult();
	is_finished_.store(true);
	return true;
}

void LatencyRecorder::SaveResult()
{
	std::vector<long long> sorted_samples;
	std::map<int, std::size_t> server_error_counts;
	{
		std::lock_guard<std::mutex> lock(samples_mutex_);
		sorted_samples = latency_samples_;
	}
	{
		std::lock_guard<std::mutex> lock(server_error_mutex_);
		server_error_counts = server_error_counts_;
	}
	std::sort(sorted_samples.begin(), sorted_samples.end());

	std::filesystem::create_directories(output_directory_);
	std::ofstream output(std::filesystem::path(output_directory_) / "latency.txt", std::ios::trunc);
	const long long total_latency = std::accumulate(sorted_samples.begin(), sorted_samples.end(), 0LL);
	const double average_latency = sorted_samples.empty() ? 0.0 : static_cast<double>(total_latency) / sorted_samples.size();
	const auto count_at_most = [&sorted_samples](long long upper_bound) {
		return std::upper_bound(sorted_samples.begin(), sorted_samples.end(), upper_bound) - sorted_samples.begin();
	};

	output << "sample_count=" << sorted_samples.size() << '\n';
	output << "average_ms=" << average_latency << '\n';
	output << "p50_ms=" << CalculatePercentile(sorted_samples, 0.50) << '\n';
	output << "p95_ms=" << CalculatePercentile(sorted_samples, 0.95) << '\n';
	output << "p99_ms=" << CalculatePercentile(sorted_samples, 0.99) << '\n';
	output << "max_ms=" << (sorted_samples.empty() ? 0 : sorted_samples.back()) << '\n';
	output << "latency_0_10_ms=" << count_at_most(10) << '\n';
	output << "latency_11_20_ms=" << count_at_most(20) - count_at_most(10) << '\n';
	output << "latency_21_30_ms=" << count_at_most(30) - count_at_most(20) << '\n';
	output << "latency_31_50_ms=" << count_at_most(50) - count_at_most(30) << '\n';
	output << "latency_51_100_ms=" << count_at_most(100) - count_at_most(50) << '\n';
	output << "latency_over_100_ms=" << sorted_samples.size() - count_at_most(100) << '\n';
	std::size_t server_error_count = 0;
	for (const auto& [error_code, error_count] : server_error_counts) server_error_count += error_count;
	output << "server_error_count=" << server_error_count << '\n';
	for (const auto& [error_code, error_count] : server_error_counts) output << "server_error_code_" << error_code << '=' << error_count << '\n';
}

long long LatencyRecorder::CalculatePercentile(const std::vector<long long>& sorted_samples, double percentile) const
{
	if (sorted_samples.empty()) return 0;
	const std::size_t index = (std::min)(sorted_samples.size() - 1, static_cast<std::size_t>(std::ceil(sorted_samples.size() * percentile)) - 1);
	return sorted_samples[index];
}
