#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "DBResult.h"

class RankingManager
{
	static constexpr int MAX_RANKING_COUNT = 10;

	std::vector<RankingInfo> rankings_;
	mutable std::mutex ranking_mutex_;

	static bool CompareRankingInfo(const RankingInfo& lhs, const RankingInfo& rhs);
	void SortAndTrim();

public:
	void InitRanking(std::vector<RankingInfo>& initial_rankings);
	void UpdateRanking(int player_id, const std::string& nickname, int score);
	std::vector<RankingInfo> GetRankings() const;
};

