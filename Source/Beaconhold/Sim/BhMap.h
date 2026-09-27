// Beaconhold simulation core - tile map (terrain, trees, occupancy, beacon sites).
#pragma once

#include "BhMath.h"
#include "BhTypes.h"

#include <cstdint>
#include <vector>

namespace bh
{
enum class Ground : uint8_t
{
	Grass,
	Meadow, // grass with flowers (decorative)
	Dirt,
	Sand,
	Water,
	Rock,
	Blight, // Gloam-corrupted ground: walkable, cannot be built on by Wardens
	Count,
};

enum class TreeKind : uint8_t
{
	None = 0,
	Pine = 1,
	Oak = 2,
	Dead = 3, // gloam-withered tree
};

struct MapTile
{
	Ground G = Ground::Grass;
	TreeKind Tree = TreeKind::None;
	uint16_t Wood = 0;
	EntityId Occupant = NoEntity; // building or resource node standing on this tile
	int8_t BeaconSite = -1;       // index into GameMap::BeaconSites, -1 if none
};

class GameMap
{
public:
	void Init(int InWidth, int InHeight);

	int GetWidth() const { return Width; }
	int GetHeight() const { return Height; }
	bool InBounds(int X, int Y) const { return X >= 0 && Y >= 0 && X < Width && Y < Height; }
	bool InBounds(const Tile& T) const { return InBounds(T.X, T.Y); }

	MapTile& At(int X, int Y) { return Tiles[static_cast<size_t>(Y * Width + X)]; }
	const MapTile& At(int X, int Y) const { return Tiles[static_cast<size_t>(Y * Width + X)]; }
	MapTile& At(const Tile& T) { return At(T.X, T.Y); }
	const MapTile& At(const Tile& T) const { return At(T.X, T.Y); }

	bool IsWalkable(int X, int Y) const;
	bool IsWalkable(const Tile& T) const { return IsWalkable(T.X, T.Y); }
	bool IsWalkablePos(const Vec2& P) const { return IsWalkable(Tile::FromPos(P)); }
	bool HasTree(int X, int Y) const { return InBounds(X, Y) && At(X, Y).Tree != TreeKind::None; }
	bool IsBuildableGround(int X, int Y) const;

	// Marks/unmarks a building footprint as occupied.
	void SetOccupant(const TileRect& Rect, EntityId Id);

	// Removes wood from a tree. Returns the amount actually taken; fells the tree when empty.
	int HarvestTree(int X, int Y, int Amount, bool& bOutFelled);

	// Nearest tile with a tree to Pos within MaxRadius (tiles), preferring reachable edge trees.
	bool FindNearestTree(const Vec2& Pos, float MaxRadius, Tile& OutTile) const;

	// Nearest walkable tile to T (spiral search).
	bool FindNearestWalkable(const Tile& T, int MaxRadius, Tile& OutTile) const;

	// Is tile T next to (8-neighbourhood) at least one walkable tile?
	bool HasWalkableNeighbour(int X, int Y) const;

	uint32_t GetRevision() const { return Revision; }
	void BumpRevision() { ++Revision; }

	std::vector<TileRect> BeaconSites;
	std::vector<Tile> SpawnPoints[10]; // scripted wave spawn points, by digit

	// Trees felled since last cleared (consumed by the presentation layer).
	std::vector<Tile> FelledTrees;

	const std::vector<MapTile>& GetTiles() const { return Tiles; }
	std::vector<MapTile>& GetTilesMutable() { return Tiles; }

	// Replaces the whole grid (used when loading a saved session).
	bool Assign(int InWidth, int InHeight, const std::vector<MapTile>& InTiles)
	{
		if (InWidth <= 0 || InHeight <= 0 || static_cast<int>(InTiles.size()) != InWidth * InHeight)
		{
			return false;
		}
		Width = InWidth;
		Height = InHeight;
		Tiles = InTiles;
		++Revision;
		return true;
	}

private:
	int Width = 0;
	int Height = 0;
	uint32_t Revision = 1;
	std::vector<MapTile> Tiles;
};

} // namespace bh
