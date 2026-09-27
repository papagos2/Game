// Beaconhold simulation core - grid A* pathfinding.
#include "BhPath.h"

#include <algorithm>

namespace bh
{
namespace
{
struct PathHeapGreater
{
	template <typename T>
	bool operator()(const T& A, const T& B) const
	{
		return A.F > B.F;
	}
};

constexpr float PathDiagonalCost = 1.41421356f;
constexpr float PathUnitClearance = 0.28f;
} // namespace

void PathFinder::Prepare(int NumTiles)
{
	if (static_cast<int>(GScore.size()) != NumTiles)
	{
		GScore.assign(static_cast<size_t>(NumTiles), 0.f);
		Parent.assign(static_cast<size_t>(NumTiles), -1);
		OpenStamp.assign(static_cast<size_t>(NumTiles), 0u);
		ClosedStamp.assign(static_cast<size_t>(NumTiles), 0u);
		Stamp = 0;
	}
	++Stamp;
	if (Stamp == 0)
	{
		std::fill(OpenStamp.begin(), OpenStamp.end(), 0u);
		std::fill(ClosedStamp.begin(), ClosedStamp.end(), 0u);
		Stamp = 1;
	}
	Heap.clear();
}

float PathFinder::Heuristic(int X, int Y, const TileRect& Goal)
{
	const int Dx = MaxI(MaxI(Goal.X0 - X, 0), X - (Goal.X1 - 1));
	const int Dy = MaxI(MaxI(Goal.Y0 - Y, 0), Y - (Goal.Y1 - 1));
	const int Lo = MinI(Dx, Dy);
	const int Hi = MaxI(Dx, Dy);
	return static_cast<float>(Hi - Lo) + PathDiagonalCost * static_cast<float>(Lo);
}

bool PathFinder::LineWalkable(const GameMap& Map, const Vec2& A, const Vec2& B, float Clearance)
{
	const Vec2 Delta = B - A;
	const float Len = Delta.Length();
	if (Len < 1e-4f)
	{
		return Map.IsWalkablePos(B);
	}
	const Vec2 Dir = Delta / Len;
	const Vec2 Side(-Dir.Y * Clearance, Dir.X * Clearance);
	const Tile StartTile = Tile::FromPos(A);
	const int Steps = static_cast<int>(Len / 0.2f) + 1;
	for (int I = 1; I <= Steps; ++I)
	{
		const float T = MinF(static_cast<float>(I) * 0.2f, Len);
		const Vec2 P = A + Dir * T;
		const Vec2 Samples[3] = {P, P + Side, P - Side};
		for (const Vec2& S : Samples)
		{
			const Tile St = Tile::FromPos(S);
			if (St == StartTile)
			{
				continue; // allow leaving a blocked start tile
			}
			if (!Map.IsWalkable(St))
			{
				return false;
			}
		}
	}
	return true;
}

bool PathFinder::FindPath(const GameMap& Map, const Vec2& Start, const TileRect& Goal, const Vec2& GoalPoint,
	std::vector<Vec2>& OutPath, bool& bOutReachedGoal)
{
	OutPath.clear();
	bOutReachedGoal = false;
	LastExpansions = 0;

	const int W = Map.GetWidth();
	const int H = Map.GetHeight();
	Prepare(W * H);

	Tile StartTile = Tile::FromPos(Start);
	StartTile.X = ClampI(StartTile.X, 0, W - 1);
	StartTile.Y = ClampI(StartTile.Y, 0, H - 1);
	const int StartIndex = StartTile.Y * W + StartTile.X;

	auto IsGoalTile = [&](int X, int Y) -> bool
	{
		return Goal.Contains(Tile(X, Y)) && (Map.IsWalkable(X, Y) || (X == StartTile.X && Y == StartTile.Y));
	};

	auto ClampIntoTile = [](const Tile& T, const Vec2& P) -> Vec2
	{
		const float Margin = 0.2f;
		return Vec2(ClampF(P.X, static_cast<float>(T.X) + Margin, static_cast<float>(T.X + 1) - Margin),
			ClampF(P.Y, static_cast<float>(T.Y) + Margin, static_cast<float>(T.Y + 1) - Margin));
	};

	if (IsGoalTile(StartTile.X, StartTile.Y))
	{
		bOutReachedGoal = true;
		const Vec2 Final = ClampIntoTile(StartTile, GoalPoint);
		if (Vec2::DistSq(Final, Start) > 0.01f)
		{
			OutPath.push_back(Final);
		}
		return true;
	}

	GScore[static_cast<size_t>(StartIndex)] = 0.f;
	Parent[static_cast<size_t>(StartIndex)] = -1;
	OpenStamp[static_cast<size_t>(StartIndex)] = Stamp;
	Heap.push_back({Heuristic(StartTile.X, StartTile.Y, Goal), StartIndex});

	int BestIndex = StartIndex;
	float BestH = Heuristic(StartTile.X, StartTile.Y, Goal);
	float BestG = 0.f;
	int FoundIndex = -1;

	static const int NX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
	static const int NY[8] = {0, 0, 1, -1, 1, -1, 1, -1};

	const int MaxExpansions = W * H;
	while (!Heap.empty())
	{
		std::pop_heap(Heap.begin(), Heap.end(), PathHeapGreater());
		const HeapNode Node = Heap.back();
		Heap.pop_back();
		const int Cur = Node.Index;
		if (ClosedStamp[static_cast<size_t>(Cur)] == Stamp)
		{
			continue;
		}
		ClosedStamp[static_cast<size_t>(Cur)] = Stamp;
		++LastExpansions;

		const int CX = Cur % W;
		const int CY = Cur / W;
		if (IsGoalTile(CX, CY))
		{
			FoundIndex = Cur;
			break;
		}
		const float CurG = GScore[static_cast<size_t>(Cur)];
		const float CurH = Heuristic(CX, CY, Goal);
		if (CurH < BestH || (CurH == BestH && CurG < BestG))
		{
			BestH = CurH;
			BestG = CurG;
			BestIndex = Cur;
		}
		if (LastExpansions >= MaxExpansions)
		{
			break;
		}

		for (int Dir = 0; Dir < 8; ++Dir)
		{
			const int NXp = CX + NX[Dir];
			const int NYp = CY + NY[Dir];
			if (!Map.InBounds(NXp, NYp) || !Map.IsWalkable(NXp, NYp))
			{
				continue;
			}
			const bool bDiagonal = Dir >= 4;
			if (bDiagonal && (!Map.IsWalkable(CX + NX[Dir], CY) || !Map.IsWalkable(CX, CY + NY[Dir])))
			{
				continue; // no corner cutting
			}
			const int NIndex = NYp * W + NXp;
			if (ClosedStamp[static_cast<size_t>(NIndex)] == Stamp)
			{
				continue;
			}
			const float NewG = CurG + (bDiagonal ? PathDiagonalCost : 1.f);
			if (OpenStamp[static_cast<size_t>(NIndex)] == Stamp && NewG >= GScore[static_cast<size_t>(NIndex)])
			{
				continue;
			}
			OpenStamp[static_cast<size_t>(NIndex)] = Stamp;
			GScore[static_cast<size_t>(NIndex)] = NewG;
			Parent[static_cast<size_t>(NIndex)] = Cur;
			Heap.push_back({NewG + Heuristic(NXp, NYp, Goal) * 1.001f, NIndex});
			std::push_heap(Heap.begin(), Heap.end(), PathHeapGreater());
		}
	}

	const int EndIndex = FoundIndex >= 0 ? FoundIndex : BestIndex;
	bOutReachedGoal = FoundIndex >= 0;
	if (EndIndex == StartIndex)
	{
		return bOutReachedGoal;
	}

	TilePath.clear();
	for (int I = EndIndex; I != -1 && I != StartIndex; I = Parent[static_cast<size_t>(I)])
	{
		TilePath.push_back(Tile(I % W, I / W));
	}
	std::reverse(TilePath.begin(), TilePath.end());

	// Raw waypoints: tile centres, with the final point pulled towards the requested goal point.
	std::vector<Vec2> Raw;
	Raw.reserve(TilePath.size());
	for (size_t I = 0; I < TilePath.size(); ++I)
	{
		Raw.push_back(TilePath[I].Center());
	}
	if (!Raw.empty())
	{
		Raw.back() = ClampIntoTile(TilePath.back(), bOutReachedGoal ? GoalPoint : Goal.ClosestPoint(TilePath.back().Center()));
	}

	// String pulling: skip waypoints that are directly visible.
	Vec2 Anchor = Start;
	size_t I = 0;
	while (I < Raw.size())
	{
		size_t Farthest = I;
		for (size_t J = Raw.size(); J-- > I + 1;)
		{
			if (LineWalkable(Map, Anchor, Raw[J], PathUnitClearance))
			{
				Farthest = J;
				break;
			}
		}
		OutPath.push_back(Raw[Farthest]);
		Anchor = Raw[Farthest];
		I = Farthest + 1;
	}
	return true;
}

} // namespace bh
