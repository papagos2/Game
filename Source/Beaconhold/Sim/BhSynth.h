// Beaconhold simulation core - procedural sound effects and ambient music.
//
// All audio is synthesized at startup (16-bit mono PCM), so the game ships without sound
// assets. Real recordings can replace any cue later via the audio settings in the editor.
#pragma once

#include "BhWorld.h"

#include <cstdint>
#include <vector>

namespace bh
{
enum class Sfx : uint8_t
{
	UiTap,
	UiConfirm,
	UiError,
	Select,
	Acknowledge,
	SwordHit,
	ArrowShoot,
	ArrowHit,
	MagicCast,
	MagicHit,
	Chop,
	Mine,
	Hammer,
	Deliver,
	BuildComplete,
	UnitTrained,
	ResearchDone,
	UnitDie,
	GloamDie,
	BuildingCollapse,
	TowerShoot,
	Alarm,
	WaveHorn,
	BeaconLit,
	Victory,
	Defeat,
	Heal,
	Charge,
	Brace,
	Sunburst,
	StarAward,
	Count,
};
constexpr int NumSfx = static_cast<int>(Sfx::Count);

struct SfxInfo
{
	const char* Name = "";
	float Volume = 1.f;      // relative mix volume
	float MinInterval = 0.f; // seconds between repeats of the same cue (anti-spam)
	bool bPositional = false; // attenuate by distance from the camera focus
};

const SfxInfo& GetSfxInfo(Sfx S);

constexpr int SynthSampleRate = 22050;

void SynthSfx(Sfx S, int SampleRate, std::vector<int16_t>& Out);

// A seamless ambient loop (lute arpeggios over a warm drone) of about Seconds length.
void SynthMusic(int SampleRate, float Seconds, bool bBattle, std::vector<int16_t>& Out);

// Which cue (if any) a simulation event should play. Returns false for silent events.
bool SfxForEvent(const GameEvent& E, Sfx& Out);

} // namespace bh
