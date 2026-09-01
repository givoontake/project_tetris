#include "RankingManager.h"

#include <algorithm>

bool RankingManager::CompareRankingInfo(const RankingInfo& lhs, const RankingInfo& rhs)
{
	if (lhs.score != rhs.score) return lhs.score > rhs.score;
	return lhs.id < rhs.id;
}

void RankingManager::SortAndTrim()
{
	std::sort(rankings.begin(), rankings.end(), CompareRankingInfo); // 오름차순 정렬
	if (rankings.size() > MAX_RANKING_COUNT) rankings.resize(MAX_RANKING_COUNT); // 상위 10개만 남기기
}

void RankingManager::InitRanking(std::vector<RankingInfo>& loaded_rankings)
{
	std::lock_guard<std::mutex> lock(ranking_mutex);
	rankings = std::move(loaded_rankings);
	SortAndTrim();
}

void RankingManager::UpdateRanking(int id, const std::string& nickname, int score)
{
	if (score <= 0) return;

	std::lock_guard<std::mutex> lock(ranking_mutex);

	auto it = std::find_if(rankings.begin(), rankings.end(),
		[id](const RankingInfo& info) {
			return info.id == id;
		});

	if (it != rankings.end()) {
		it->nickname = nickname;
		if (score > it->score) it->score = score;
		SortAndTrim();
		return;
	}

	if (rankings.size() < MAX_RANKING_COUNT || score > rankings.back().score) {
		rankings.push_back(RankingInfo{ id, nickname, score });
		SortAndTrim();
	}
}

std::vector<RankingInfo> RankingManager::GetRankings() const
{
	std::lock_guard<std::mutex> lock(ranking_mutex);
	return rankings;
}
