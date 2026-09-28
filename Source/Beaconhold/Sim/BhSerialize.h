// Beaconhold simulation core - save/restore a mission session (suspend & resume on mobile).
#pragma once

#include "BhSession.h"

#include <cstdint>
#include <string>
#include <vector>

namespace bh
{
// Bump whenever the saved layout changes; saves of another version are refused (a suspended
// mission from an older build is dropped, never misread).
constexpr uint32_t SessionSaveVersion = 2;

// Serializes everything needed to continue the mission exactly where it was.
void SaveSession(const Session& S, std::vector<uint8_t>& OutBytes);

// Restores a session saved by SaveSession. On failure the session is left unstarted.
bool LoadSession(Session& S, const std::vector<uint8_t>& Bytes, std::string& OutError);

// Stable checksum of the simulation state (tests / debugging).
uint64_t HashWorld(const World& W);

} // namespace bh
