// Beaconhold simulation core - save/restore a mission session (suspend & resume on mobile).
#pragma once

#include "BhSession.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bh
{
// Bump whenever the saved layout changes; saves of another version are refused (a suspended
// mission from an older build is dropped, never misread).
constexpr uint32_t SessionSaveVersion = 3;

// Magic, version and a checksum of the rest: a save cut short or damaged on disk is refused.
constexpr size_t SaveHeaderBytes = 16;

// Serializes everything needed to continue the mission exactly where it was.
void SaveSession(const Session& S, std::vector<uint8_t>& OutBytes);

// Restores a session saved by SaveSession. A save that is damaged, from another version, or
// holds values that could index out of bounds or poison the simulation is refused with a reason;
// the session is then left unstarted.
bool LoadSession(Session& S, const std::vector<uint8_t>& Bytes, std::string& OutError);

// Cheap check (magic, version, checksum) that a stored save is whole and from this version, so
// the menu only offers to continue a mission that can be restored.
bool IsSaveIntact(const uint8_t* Data, size_t Size, std::string& OutError);

// Recomputes the checksum after the bytes were edited (tests and tools only).
void ResealSave(std::vector<uint8_t>& Bytes);

// Stable checksum of the simulation state (tests / debugging).
uint64_t HashWorld(const World& W);

} // namespace bh
