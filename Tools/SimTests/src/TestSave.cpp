// Suspend-save robustness: a phone can kill the app while it writes, storage can rot, and a save
// from another build can linger. A damaged save must be refused, never misread or crashed on.
// Run these under AddressSanitizer/UBSan too (Tools/SimTests/build-asan) to catch silent reads
// out of bounds that a normal build survives by luck.
#include "TestFramework.h"

#include "Bot.h"

#include "BhHud.h"
#include "BhPainter.h"
#include "BhRender.h"
#include "BhSerialize.h"
#include "BhSession.h"
#include "BhVisuals.h"

#include <cstring>

using namespace bh;

namespace
{
// A save with plenty going on: armies on the move, queues, projectiles, enemy picks.
const std::vector<uint8_t>& RichSave()
{
	static std::vector<uint8_t> Bytes;
	if (Bytes.empty())
	{
		Session S;
		SessionConfig C;
		C.MissionIndex = 2;
		C.bTutorial = false;
		std::string Err;
		S.Start(C, Err);
		bht::BotConfig Bot;
		Bot.TargetWorkers = 16;
		Bot.AttackSupply = 12.f;
		bht::PlayMission(S, Bot, 420.f);
		SaveSession(S, Bytes);
	}
	return Bytes;
}

uint32_t NextRandom(uint32_t& State)
{
	State ^= State << 13;
	State ^= State >> 17;
	State ^= State << 5;
	return State;
}

// What the game does with a resumed session every frame: simulate, build the HUD, pose every
// entity, paint the minimap; and once: build the ground and the trees.
void ExerciseSession(Session& S, float Seconds)
{
	HudModel Hud;
	std::vector<GroundCell> Cells;
	std::vector<Rgb> Colors;
	BuildGround(S.GetWorld().GetMap(), 2, Cells, Colors);
	std::vector<TreeInstance> Trees;
	BuildTreeInstances(S.GetWorld().GetMap(), Trees);
	ImageRGBA Minimap;
	PaintMinimap(S.GetWorld().GetMap(), 2, Minimap);
	for (float T = 0.f; T < Seconds; T += 0.25f)
	{
		S.Update(0.25f, nullptr);
		BuildHudModel(S, Hud);
		for (const Entity& E : S.GetWorld().GetEntities())
		{
			AnimInput In;
			In.Type = E.Type;
			In.Owner = E.Owner;
			In.Act = E.Act;
			In.Buff = E.Buff;
			In.Carry = E.CarryAmount > 0 ? E.CarryType : Resource::None;
			In.bConstructed = E.bConstructed;
			In.BuildProgress = E.BuildProgress;
			In.Time = T;
			In.Seed = E.Id;
			EvaluateEntityPose(In);
		}
		Vec2 P;
		float H = 0.f;
		FindTutorialMarker(S, P, H);
	}
	const Vec2 View[4] = {Vec2(0.f, 0.f), Vec2(8.f, 0.f), Vec2(8.f, 6.f), Vec2(0.f, 6.f)};
	ImageRGBA Overlay;
	PaintMinimapOverlay(S, Minimap, 2, View, true, {}, Overlay);
}
} // namespace

BH_TEST(Save_TruncatedSavesAreRefused)
{
	// The app killed halfway through writing leaves a short file.
	const std::vector<uint8_t>& Bytes = RichSave();
	BH_EXPECT(Bytes.size() > 1000);
	int Loaded = 0;
	for (size_t Cut = 0; Cut < Bytes.size(); Cut += Cut < 64 ? 1 : 211)
	{
		const std::vector<uint8_t> Part(Bytes.begin(), Bytes.begin() + static_cast<std::ptrdiff_t>(Cut));
		Session S;
		std::string Err;
		if (LoadSession(S, Part, Err))
		{
			++Loaded;
		}
		BH_EXPECT(!S.IsStarted() || Err.empty());
	}
	BH_EXPECT_MSG(Loaded == 0, "%d truncated saves loaded", Loaded);
	Session S;
	std::string Err;
	BH_EXPECT_MSG(LoadSession(S, Bytes, Err), "the intact save did not load: %s", Err.c_str());
}

BH_TEST(Save_MenuOnlyOffersRestorableSaves)
{
	// The menu's quick check agrees with a full load: whole saves pass; cut, damaged, and other
	// versions' saves are dropped before "Continue" is shown.
	const std::vector<uint8_t>& Bytes = RichSave();
	std::string Err;
	BH_EXPECT(IsSaveIntact(Bytes.data(), Bytes.size(), Err));
	BH_EXPECT(!IsSaveIntact(nullptr, 0, Err));
	BH_EXPECT(!IsSaveIntact(Bytes.data(), Bytes.size() - 1, Err));
	std::vector<uint8_t> Old = Bytes;
	const uint32_t OldVersion = SessionSaveVersion - 1;
	std::memcpy(Old.data() + 4, &OldVersion, sizeof(OldVersion));
	BH_EXPECT(!IsSaveIntact(Old.data(), Old.size(), Err));
	BH_EXPECT_MSG(Err.find("version") != std::string::npos, "unexpected reason: %s", Err.c_str());
	std::vector<uint8_t> Rot = Bytes;
	Rot[Rot.size() / 2] ^= 0x10;
	BH_EXPECT(!IsSaveIntact(Rot.data(), Rot.size(), Err));
	Session S;
	BH_EXPECT(!LoadSession(S, Rot, Err));
}

BH_TEST(Save_DamagedSavesAreRefused)
{
	// A few bytes changed anywhere (storage rot, a partial overwrite) are detected.
	const std::vector<uint8_t>& Bytes = RichSave();
	uint32_t Rng = 12345u;
	int Loaded = 0;
	for (int I = 0; I < 200; ++I)
	{
		std::vector<uint8_t> Copy = Bytes;
		const int Flips = 1 + static_cast<int>(NextRandom(Rng) % 4u);
		for (int F = 0; F < Flips; ++F)
		{
			const size_t At = NextRandom(Rng) % Copy.size();
			Copy[At] = static_cast<uint8_t>(Copy[At] ^ (1u + NextRandom(Rng) % 255u));
		}
		Session S;
		std::string Err;
		Loaded += LoadSession(S, Copy, Err) ? 1 : 0;
	}
	BH_EXPECT_MSG(Loaded == 0, "%d of 200 damaged saves loaded", Loaded);
}

BH_TEST(Save_CorruptContentIsRefusedOrHarmless)
{
	// Past the checksum (a save written by a buggy build, or edited by hand), content that could
	// index out of bounds or poison the simulation is refused; anything accepted must play on.
	const std::vector<uint8_t>& Bytes = RichSave();
	uint32_t Rng = 777u;
	int Loaded = 0;
	const int Rounds = 240;
	for (int I = 0; I < Rounds; ++I)
	{
		std::vector<uint8_t> Copy = Bytes;
		const int Edits = 1 + static_cast<int>(NextRandom(Rng) % 3u);
		for (int F = 0; F < Edits; ++F)
		{
			const size_t At = SaveHeaderBytes + NextRandom(Rng) % (Copy.size() - SaveHeaderBytes);
			switch (NextRandom(Rng) % 4u)
			{
			case 0: // one random byte
				Copy[At] = static_cast<uint8_t>(NextRandom(Rng));
				break;
			case 1: // a float or int word set to an extreme
			{
				const uint32_t Extremes[] = {0xFFFFFFFFu, 0x7F800000u, 0x7FC00000u, 0x80000000u, 0x4F000000u, 0x00FFFFFFu};
				const uint32_t V = Extremes[NextRandom(Rng) % 6u];
				if (At + 4 <= Copy.size())
				{
					std::memcpy(Copy.data() + At, &V, 4);
				}
				break;
			}
			case 2: // a small integer (enum values, indices)
				Copy[At] = static_cast<uint8_t>(NextRandom(Rng) % 40u);
				break;
			default: // a byte cleared
				Copy[At] = 0;
				break;
			}
		}
		ResealSave(Copy);
		Session S;
		std::string Err;
		if (LoadSession(S, Copy, Err))
		{
			++Loaded;
			ExerciseSession(S, 3.f);
		}
	}
	// Most random edits land on harmless values (a timer, a colour of the ground), so a good
	// share must still load: the check refuses what is dangerous, not everything.
	BH_EXPECT_MSG(Loaded > Rounds / 4, "only %d of %d edited saves loaded", Loaded, Rounds);
}

BH_TEST(Save_ResumedSessionPlaysOn)
{
	// The intact rich save resumes and keeps playing through the presentation paths.
	Session S;
	std::string Err;
	BH_EXPECT(LoadSession(S, RichSave(), Err));
	ExerciseSession(S, 20.f);
	BH_EXPECT(S.GetMission().Outcome == MissionOutcome::InProgress || S.GetMission().Elapsed > 0.f);
}
