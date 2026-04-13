#pragma once

#include <algorithm>
#include <mutex>
#include <vector>

#include "DBResult.h"

class RankingManager
{
	static constexpr int MAX_RANKING_COUNT = 10;

	std::vector<RankingInfo> rankings;
	mutable std::mutex ranking_mutex;

	static bool CompareRankingInfo(const RankingInfo& lhs, const RankingInfo& rhs)
	{
		if (lhs.score != rhs.score) return lhs.score > rhs.score;
		return lhs.db_pk < rhs.db_pk;
	}

	void SortAndTrim()
	{
		std::sort(rankings.begin(), rankings.end(), CompareRankingInfo);
		if (rankings.size() > MAX_RANKING_COUNT) rankings.resize(MAX_RANKING_COUNT);
	}

public:
	void LoadInitialRanking(const std::vector<RankingInfo>& initial_rankings)
	{
		std::lock_guard<std::mutex> lock(ranking_mutex);
		rankings = initial_rankings;
		rankings.erase(
			std::remove_if(rankings.begin(), rankings.end(),
				[](const RankingInfo& info) {
					return info.score <= 0;
				}),
			rankings.end());
		SortAndTrim();
	}

	void UpdateRanking(int db_pk, const std::string& nickname, int score)
	{
		if (score <= 0) return;

		std::lock_guard<std::mutex> lock(ranking_mutex);

		auto it = std::find_if(rankings.begin(), rankings.end(),
			[db_pk](const RankingInfo& info) {
				return info.db_pk == db_pk;
			});

		if (it != rankings.end()) {
			it->nickname = nickname;
			if (score > it->score) it->score = score;
			SortAndTrim();
			return;
		}

		if (rankings.size() < MAX_RANKING_COUNT || score > rankings.back().score) {
			rankings.push_back(RankingInfo{ db_pk, nickname, score });
			SortAndTrim();
		}
	}

	std::vector<RankingInfo> GetRankings() const
	{
		std::lock_guard<std::mutex> lock(ranking_mutex);
		return rankings;
	}
};
