// Beaconhold simulation core - tile map.
#include "BhMap.h"

namespace bh
{
void GameMap::Init(int InWidth, int InHeight)
{
	Width = MaxI(InWidth, 1);
	Height = MaxI(InHeight, 1);
	Tiles.assign(static_cast<size_t>(Width * Height), MapTile());
	BeaconSites.clear();
	for (std::vector<Tile>& Points : SpawnPoints)
	{
		Points.clear();
	}
	FelledTrees.clear();
	++Revision;
}

bool GameMap::IsWalkable(int X, int Y) const
{
	if (!InBounds(X, Y))
	{
		return false;
	}
	const MapTile& T = At(X, Y);
	if (T.G == Ground::Water || T.G == Ground::Rock)
	{
		return false;
	}
	return T.Tree == TreeKind::None && T.Occupant == NoEntity;
}

bool GameMap::IsBuildableGround(int X, int Y) const
{
	if (!IsWalkable(X, Y))
	{
		return false;
	}
	const Ground G = At(X, Y).G;
	return G != Ground::Blight;
}

void GameMap::SetOccupant(const TileRect& Rect, EntityId Id)
{
	for (int Y = Rect.Y0; Y < Rect.Y1; ++Y)
	{
		for (int X = Rect.X0; X < Rect.X1; ++X)
		{
			if (InBounds(X, Y))
			{
				At(X, Y).Occupant = Id;
			}
		}
	}
	++Revision;
}

int GameMap::HarvestTree(int X, int Y, int Amount, bool& bOutFelled)
{
	bOutFelled = false;
	if (!HasTree(X, Y))
	{
		return 0;
	}
	MapTile& T = At(X, Y);
	const int Taken = MinI(Amount, static_cast<int>(T.Wood));
	T.Wood = static_cast<uint16_t>(static_cast<int>(T.Wood) - Taken);
	if (T.Wood == 0)
	{
		T.Tree = TreeKind::None;
		bOutFelled = true;
		FelledTrees.push_back(Tile(X, Y));
		++Revision;
	}
	return Taken;
}

bool GameMap::HasWalkableNeighbour(int X, int Y) const
{
	for (int Dy = -1; Dy <= 1; ++Dy)
	{
		for (int Dx = -1; Dx <= 1; ++Dx)
		{
			if ((Dx != 0 || Dy != 0) && IsWalkable(X + Dx, Y + Dy))
			{
				return true;
			}
		}
	}
	return false;
}

bool GameMap::FindNearestTree(const Vec2& Pos, float MaxRadius, Tile& OutTile) const
{
	const Tile Center = Tile::FromPos(Pos);
	const int R = static_cast<int>(MaxRadius) + 1;
	float BestScore = 1e9f;
	bool bFound = false;
	for (int Y = Center.Y - R; Y <= Center.Y + R; ++Y)
	{
		for (int X = Center.X - R; X <= Center.X + R; ++X)
		{
			if (!HasTree(X, Y))
			{
				continue;
			}
			const float D = Vec2::Dist(Tile(X, Y).Center(), Pos);
			if (D > MaxRadius)
			{
				continue;
			}
			// Trees on the forest edge are reachable; interior trees are only a fallback.
			const float Score = D + (HasWalkableNeighbour(X, Y) ? 0.f : 50.f);
			if (Score < BestScore)
			{
				BestScore = Score;
				OutTile = Tile(X, Y);
				bFound = true;
			}
		}
	}
	return bFound && BestScore < 50.f;
}

bool GameMap::FindNearestWalkable(const Tile& T, int MaxRadius, Tile& OutTile) const
{
	if (IsWalkable(T))
	{
		OutTile = T;
		return true;
	}
	for (int R = 1; R <= MaxRadius; ++R)
	{
		float BestD = 1e9f;
		bool bFound = false;
		for (int Y = T.Y - R; Y <= T.Y + R; ++Y)
		{
			for (int X = T.X - R; X <= T.X + R; ++X)
			{
				if (MaxI(AbsI(X - T.X), AbsI(Y - T.Y)) != R)
				{
					continue;
				}
				if (!IsWalkable(X, Y))
				{
					continue;
				}
				const float Dx = static_cast<float>(X - T.X);
				const float Dy = static_cast<float>(Y - T.Y);
				const float D = Dx * Dx + Dy * Dy;
				if (D < BestD)
				{
					BestD = D;
					OutTile = Tile(X, Y);
					bFound = true;
				}
			}
		}
		if (bFound)
		{
			return true;
		}
	}
	return false;
}

} // namespace bh
