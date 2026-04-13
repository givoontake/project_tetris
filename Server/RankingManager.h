#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "DBResult.h"

class RankingManager
{
	static constexpr int MAX_RANKING_COUNT = 10;

	std::vector<RankingInfo> rankings;
	mutable std::mutex ranking_mutex;

	static bool CompareRankingInfo(const RankingInfo& lhs, const RankingInfo& rhs);
	void SortAndTrim();

public:
	void LoadInitialRanking(const std::vector<RankingInfo>& initial_rankings);
	void UpdateRanking(int id, const std::string& nickname, int score);
	std::vector<RankingInfo> GetRankings() const;
};

