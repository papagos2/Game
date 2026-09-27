// Tests for procedural media: sound effects, music and icons.
#include "TestFramework.h"

#include "BhPainter.h"
#include "BhSynth.h"
#include "BhVisuals.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace bh;

namespace
{
void WriteWav(const char* Path, const std::vector<int16_t>& Pcm, int Sr)
{
	FILE* F = std::fopen(Path, "wb");
	if (F == nullptr)
	{
		return;
	}
	const uint32_t DataBytes = static_cast<uint32_t>(Pcm.size() * 2);
	const uint32_t RiffSize = 36 + DataBytes;
	const uint16_t Fmt = 1, Ch = 1, Bits = 16, Align = 2;
	const uint32_t Rate = static_cast<uint32_t>(Sr), ByteRate = Rate * 2, FmtSize = 16;
	std::fwrite("RIFF", 1, 4, F);
	std::fwrite(&RiffSize, 4, 1, F);
	std::fwrite("WAVEfmt ", 1, 8, F);
	std::fwrite(&FmtSize, 4, 1, F);
	std::fwrite(&Fmt, 2, 1, F);
	std::fwrite(&Ch, 2, 1, F);
	std::fwrite(&Rate, 4, 1, F);
	std::fwrite(&ByteRate, 4, 1, F);
	std::fwrite(&Align, 2, 1, F);
	std::fwrite(&Bits, 2, 1, F);
	std::fwrite("data", 1, 4, F);
	std::fwrite(&DataBytes, 4, 1, F);
	std::fwrite(Pcm.data(), 2, Pcm.size(), F);
	std::fclose(F);
}
} // namespace

BH_TEST(Media_AllSoundEffectsAreAudible)
{
	const char* Dump = std::getenv("BH_DUMP_AUDIO");
	for (int I = 0; I < NumSfx; ++I)
	{
		std::vector<int16_t> Pcm;
		SynthSfx(static_cast<Sfx>(I), SynthSampleRate, Pcm);
		BH_EXPECT_MSG(!Pcm.empty(), "sfx %s empty", GetSfxInfo(static_cast<Sfx>(I)).Name);
		if (Pcm.empty())
		{
			continue;
		}
		int Peak = 0;
		double Energy = 0.0;
		for (int16_t V : Pcm)
		{
			Peak = MaxI(Peak, V < 0 ? -V : V);
			Energy += static_cast<double>(V) * static_cast<double>(V);
		}
		const double Rms = std::sqrt(Energy / static_cast<double>(Pcm.size()));
		const float Seconds = static_cast<float>(Pcm.size()) / static_cast<float>(SynthSampleRate);
		BH_EXPECT_MSG(Peak > 8000 && Peak <= 32767, "sfx %s peak %d", GetSfxInfo(static_cast<Sfx>(I)).Name, Peak);
		BH_EXPECT_MSG(Rms > 300.0, "sfx %s too quiet (rms %.0f)", GetSfxInfo(static_cast<Sfx>(I)).Name, Rms);
		BH_EXPECT_MSG(Seconds > 0.04f && Seconds < 3.5f, "sfx %s length %.2fs", GetSfxInfo(static_cast<Sfx>(I)).Name, Seconds);
		// Starts and ends quietly (no clicks).
		BH_EXPECT_MSG(std::abs(static_cast<int>(Pcm.back())) < 3000, "sfx %s ends with a click", GetSfxInfo(static_cast<Sfx>(I)).Name);
		if (Dump != nullptr)
		{
			const std::string Path = std::string(Dump) + "/" + GetSfxInfo(static_cast<Sfx>(I)).Name + ".wav";
			WriteWav(Path.c_str(), Pcm, SynthSampleRate);
		}
	}
}

BH_TEST(Media_MusicLoopsSeamlessly)
{
	for (int Battle = 0; Battle < 2; ++Battle)
	{
		std::vector<int16_t> Pcm;
		SynthMusic(SynthSampleRate, 32.f, Battle == 1, Pcm);
		BH_EXPECT(Pcm.size() >= static_cast<size_t>(SynthSampleRate * 31));
		// The seam must not jump: compare the last and first samples.
		const int Jump = std::abs(static_cast<int>(Pcm.back()) - static_cast<int>(Pcm.front()));
		BH_EXPECT_MSG(Jump < 2500, "loop seam jump %d", Jump);
		double Energy = 0.0;
		for (int16_t V : Pcm)
		{
			Energy += static_cast<double>(V) * static_cast<double>(V);
		}
		BH_EXPECT(std::sqrt(Energy / static_cast<double>(Pcm.size())) > 800.0);
		if (const char* Dump = std::getenv("BH_DUMP_AUDIO"))
		{
			WriteWav((std::string(Dump) + (Battle ? "/MusicBattle.wav" : "/Music.wav")).c_str(), Pcm, SynthSampleRate);
		}
	}
}

BH_TEST(Media_EventsMapToSounds)
{
	GameEvent E;
	E.Type = EventType::Hit;
	E.Sub = static_cast<int>(Archetype::Ranger);
	Sfx S = Sfx::UiTap;
	BH_EXPECT(SfxForEvent(E, S) && S == Sfx::ArrowHit);
	E.Sub = static_cast<int>(Archetype::Shieldbearer);
	BH_EXPECT(SfxForEvent(E, S) && S == Sfx::SwordHit);
	E.Type = EventType::BuildingCompleted;
	E.Owner = Team::Player;
	E.Arch = Archetype::Beacon;
	BH_EXPECT(SfxForEvent(E, S) && S == Sfx::BeaconLit);
	E.Type = EventType::EntitySpawned;
	BH_EXPECT(!SfxForEvent(E, S));
}

BH_TEST(Media_IconsAndDecalsHaveContent)
{
	for (int I = 1; I < NumIcons; ++I)
	{
		ImageRGBA Img;
		PaintIcon(static_cast<Icon>(I), 64, Img);
		int Opaque = 0;
		for (size_t K = 3; K < Img.Px.size(); K += 4)
		{
			Opaque += Img.Px[K] > 128 ? 1 : 0;
		}
		BH_EXPECT_MSG(Opaque > 200, "icon %d nearly empty (%d px)", I, Opaque);
	}
	for (int D = 0; D < static_cast<int>(DecalTex::Count); ++D)
	{
		ImageRGBA Img;
		PaintDecal(static_cast<DecalTex>(D), 64, Img);
		int Visible = 0;
		for (size_t K = 3; K < Img.Px.size(); K += 4)
		{
			Visible += Img.Px[K] > 10 ? 1 : 0;
		}
		BH_EXPECT_MSG(Visible > 100, "decal %d empty", D);
	}
	GameMap Map;
	Map.Init(10, 8);
	ImageRGBA Mini;
	PaintMinimap(Map, 2, Mini);
	BH_EXPECT(Mini.W == 20 && Mini.H == 16);
	for (int A = 0; A < NumArchetypes; ++A)
	{
		BH_EXPECT(!GetModel(static_cast<Archetype>(A)).Parts.empty());
	}
}
