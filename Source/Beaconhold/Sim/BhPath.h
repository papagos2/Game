// Beaconhold simulation core — grid A* pathfinding with path smoothing.
#pragma once

#include "BhMap.h"
#include "BhMath.h"

#include <cstdint>
#include <vector>

namespace bh
{
class PathFinder
{
public:
	// Finds a path from Start to any walkable tile inside Goal.
	// OutPath receives world-space waypoints (the start position is not included).
	// If the goal cannot be reached, a path to the closest reachable tile is returned and
	// bOutReachedGoal is false. Returns false only when no movement is possible at all.
	bool FindPath(const GameMap& Map, const Vec2& Start, const TileRect& Goal, const Vec2& GoalPoint,
		std::vector<Vec2>& OutPath, bool& bOutReachedGoal);

	// True if a unit can walk in a straight line from A to B (samples along the segment).
	static bool LineWalkable(const GameMap& Map, const Vec2& A, const Vec2& B, float Clearance);

	int GetLastExpansions() const { return LastExpansions; }

private:
	struct HeapNode
	{
		float F;
		int Index;
	};

	void Prepare(int NumTiles);
	static float Heuristic(int X, int Y, const TileRect& Goal);

	std::vector<float> GScore;
	std::vector<int> Parent;
	std::vector<uint32_t> OpenStamp;
	std::vector<uint32_t> ClosedStamp;
	std::vector<HeapNode> Heap;
	std::vector<Tile> TilePath;
	uint32_t Stamp = 0;
	int LastExpansions = 0;
};

} // namespace bh
