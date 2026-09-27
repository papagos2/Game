// Beaconhold simulation core — campaign progress and boons.
#include "BhProgress.h"

#include "BhData.h"
#include "BhMath.h"

namespace bh
{
int BoonRankCost(int CurrentRank)
{
	return CurrentRank + 1;
}

int CampaignProgress::TotalRenown() const
{
	int Total = 0;
	for (int I = 0; I < MaxCampaignMissions; ++I)
	{
		Total += ClampI(BestStars[I], 0, 3);
		Total += CompletedHard[I] ? 1 : 0;
	}
	return Total;
}

int CampaignProgress::SpentRenown() const
{
	int Spent = 0;
	for (int B = 0; B < NumBoons; ++B)
	{
		for (int R = 0; R < ClampI(BoonRanks[B], 0, MaxBoonRank); ++R)
		{
			Spent += BoonRankCost(R);
		}
	}
	return Spent;
}

bool CampaignProgress::IsUnlocked(int Mission) const
{
	if (Mission <= 0)
	{
		return true;
	}
	if (Mission >= MaxCampaignMissions)
	{
		return false;
	}
	return Completed[Mission - 1];
}

bool CampaignProgress::CanRaiseBoon(Boon B) const
{
	const int Index = static_cast<int>(B);
	if (Index < 0 || Index >= NumBoons || BoonRanks[Index] >= MaxBoonRank)
	{
		return false;
	}
	return AvailableRenown() >= BoonRankCost(BoonRanks[Index]);
}

bool CampaignProgress::RaiseBoon(Boon B)
{
	if (!CanRaiseBoon(B))
	{
		return false;
	}
	++BoonRanks[static_cast<int>(B)];
	return true;
}

void CampaignProgress::ResetBoons()
{
	for (int& Rank : BoonRanks)
	{
		Rank = 0;
	}
}

bool CampaignProgress::RecordVictory(int Mission, int Stars, Difficulty Diff)
{
	if (Mission < 0 || Mission >= MaxCampaignMissions)
	{
		return false;
	}
	bool bImproved = !Completed[Mission];
	Completed[Mission] = true;
	if (Stars > BestStars[Mission])
	{
		BestStars[Mission] = ClampI(Stars, 0, 3);
		bImproved = true;
	}
	if (Diff == Difficulty::Hard && !CompletedHard[Mission])
	{
		CompletedHard[Mission] = true;
		bImproved = true;
	}
	return bImproved;
}

SessionModifiers ComputeModifiers(const int (&BoonRanks)[NumBoons])
{
	SessionModifiers M;
	auto Rank = [&BoonRanks](Boon B) { return static_cast<float>(ClampI(BoonRanks[static_cast<int>(B)], 0, MaxBoonRank)); };
	M.HpMult = 1.f + GetBoonDef(Boon::Hardy).PerRank * Rank(Boon::Hardy);
	M.DamageMult = 1.f + GetBoonDef(Boon::Keen).PerRank * Rank(Boon::Keen);
	M.GatherMult = 1.f + GetBoonDef(Boon::Swift).PerRank * Rank(Boon::Swift);
	M.BuildTimeMult = 1.f - GetBoonDef(Boon::Stonewright).PerRank * Rank(Boon::Stonewright);
	M.BuildingHpMult = 1.f + 0.10f * Rank(Boon::Stonewright);
	M.BonusResources = static_cast<int>(GetBoonDef(Boon::Coffers).PerRank * Rank(Boon::Coffers));
	return M;
}

} // namespace bh
