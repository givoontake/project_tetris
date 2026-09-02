#include "RankingManager.h"

#include <algorithm>

bool RankingManager::CompareRankingInfo(const RankingInfo& lhs, const RankingInfo& rhs)
{
	if (lhs.score != rhs.score) return lhs.score > rhs.score;
	return lhs.player_id < rhs.player_id;
}

void RankingManager::SortAndTrim()
{
	std::sort(rankings_.begin(), rankings_.end(), CompareRankingInfo); // 오름차순 정렬
	if (rankings_.size() > MAX_RANKING_COUNT) rankings_.resize(MAX_RANKING_COUNT); // 상위 10개만 남기기
}

void RankingManager::InitRanking(std::vector<RankingInfo>& initial_rankings)
{
	std::lock_guard<std::mutex> lock(ranking_mutex_);
	rankings_ = std::move(initial_rankings);
	SortAndTrim();
}

void RankingManager::UpdateRanking(int player_id, const std::string& nickname, int score)
{
	if (score <= 0) return;

	std::lock_guard<std::mutex> lock(ranking_mutex_);

	auto it = std::find_if(rankings_.begin(), rankings_.end(),
		[player_id](const RankingInfo& info) {
			return info.player_id == player_id;
		});

	if (it != rankings_.end()) {
		it->nickname = nickname;
		if (score > it->score) it->score = score;
		SortAndTrim();
		return;
	}

	if (rankings_.size() < MAX_RANKING_COUNT || score > rankings_.back().score) {
		rankings_.push_back(RankingInfo{ player_id, nickname, score });
		SortAndTrim();
	}
}

std::vector<RankingInfo> RankingManager::GetRankings() const
{
	std::lock_guard<std::mutex> lock(ranking_mutex_);
	return rankings_;
}
