// Beaconhold simulation core — campaign progress and permanent Warden Boons.
#pragma once

#include "BhTypes.h"

namespace bh
{
constexpr int MaxCampaignMissions = 8;

struct SessionModifiers
{
	float HpMult = 1.f;
	float DamageMult = 1.f;
	float GatherMult = 1.f;
	float BuildTimeMult = 1.f;
	float BuildingHpMult = 1.f;
	int BonusResources = 0;
};

struct CampaignProgress
{
	int BestStars[MaxCampaignMissions] = {};
	bool Completed[MaxCampaignMissions] = {};
	bool CompletedHard[MaxCampaignMissions] = {};
	int BoonRanks[NumBoons] = {};

	// Renown = best stars across missions + 1 per mission beaten on Hard.
	int TotalRenown() const;
	int SpentRenown() const;
	int AvailableRenown() const { return TotalRenown() - SpentRenown(); }

	bool IsUnlocked(int Mission) const;
	bool CanRaiseBoon(Boon B) const;
	bool RaiseBoon(Boon B);
	void ResetBoons();
	// Returns true if this improved the record.
	bool RecordVictory(int Mission, int Stars, Difficulty Diff);
};

// Renown cost to buy the next rank of a boon (rank 0 -> 1 costs 1, 1 -> 2 costs 2, 2 -> 3 costs 3).
int BoonRankCost(int CurrentRank);
SessionModifiers ComputeModifiers(const int (&BoonRanks)[NumBoons]);

} // namespace bh
