// Beaconhold — audio mixer and procedural sound wave.
#include "Game/BhAudio.h"

#include "Game/BhCommon.h"

#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "Sound/SoundBase.h"

#include <cmath>
#include <vector>

namespace
{
constexpr float AudioPi = 3.14159265f;

// Transparent below 0.8, then rounds peaks off instead of clipping when many cues overlap.
float AudioSoftClip(float X)
{
	const float A = FMath::Abs(X);
	if (A <= 0.8f)
	{
		return X;
	}
	const float Shaped = 0.8f + 0.2f * std::tanh((A - 0.8f) / 0.2f);
	return X < 0.f ? -Shaped : Shaped;
}

void AudioCopy(const std::vector<int16_t>& Src, TArray<int16>& Dst)
{
	Dst.SetNumUninitialized(static_cast<int32>(Src.size()));
	if (!Src.empty())
	{
		FMemory::Memcpy(Dst.GetData(), Src.data(), Src.size() * sizeof(int16));
	}
}
} // namespace

void FBhMixer::Synthesize()
{
	const double Start = FPlatformTime::Seconds();
	std::vector<int16_t> Pcm;
	for (int32 I = 0; I < bh::NumSfx; ++I)
	{
		bh::SynthSfx(static_cast<bh::Sfx>(I), SampleRate, Pcm);
		AudioCopy(Pcm, Cues[I]);
	}
	bEffectsReady.store(true, std::memory_order_release);
	for (int32 T = 0; T < 2; ++T)
	{
		bh::SynthMusic(SampleRate, 32.f, T == 1, Pcm);
		AudioCopy(Pcm, Music[T]);
	}
	bMusicReady.store(true, std::memory_order_release);
	UE_LOG(LogBeaconhold, Log, TEXT("Beaconhold: audio synthesized in %.0f ms"), (FPlatformTime::Seconds() - Start) * 1000.0);
}

bool FBhMixer::Play(bh::Sfx Cue, float Volume, float Pan)
{
	const int32 Index = static_cast<int32>(Cue);
	if (!AreEffectsReady() || Index < 0 || Index >= bh::NumSfx || Volume <= 0.001f)
	{
		return false;
	}
	const bh::SfxInfo& Info = bh::GetSfxInfo(Cue);
	const double Now = FPlatformTime::Seconds();
	if (Now - LastPlayed[Index] < static_cast<double>(Info.MinInterval))
	{
		return false;
	}
	LastPlayed[Index] = Now;
	const float Gain = FMath::Clamp(Volume * Info.Volume, 0.f, 2.f);
	// Equal-power panning.
	const float Angle = (FMath::Clamp(Pan, -1.f, 1.f) + 1.f) * 0.25f * AudioPi;

	FScopeLock Guard(&Lock);
	int32 Slot = -1;
	int32 Oldest = -1;
	int32 OldestPosition = -1;
	for (int32 V = 0; V < MaxVoices; ++V)
	{
		if (Voices[V].Cue < 0)
		{
			Slot = V;
			break;
		}
		if (Voices[V].Position > OldestPosition)
		{
			OldestPosition = Voices[V].Position;
			Oldest = V;
		}
	}
	if (Slot < 0)
	{
		Slot = Oldest; // steal the voice that has played the longest
	}
	if (Slot < 0)
	{
		return false;
	}
	FVoice& Voice = Voices[Slot];
	Voice.Cue = Index;
	Voice.Position = 0;
	Voice.GainL = Gain * std::cos(Angle);
	Voice.GainR = Gain * std::sin(Angle);
	return true;
}

void FBhMixer::SetMusic(int32 Track)
{
	FScopeLock Guard(&Lock);
	MusicTrack = FMath::Clamp(Track, -1, 1);
}

void FBhMixer::SetVolumes(float Master, float MusicVol, float Effects)
{
	FScopeLock Guard(&Lock);
	MasterVolume = FMath::Clamp(Master, 0.f, 2.f);
	MusicVolume = FMath::Clamp(MusicVol, 0.f, 1.f);
	EffectsVolume = FMath::Clamp(Effects, 0.f, 1.f);
}

void FBhMixer::Render(int16* Out, int32 Frames)
{
	if (Out == nullptr || Frames <= 0)
	{
		return;
	}
	if (!AreEffectsReady())
	{
		FMemory::Memzero(Out, static_cast<SIZE_T>(Frames) * 2 * sizeof(int16));
		return;
	}
	const bool bMusic = bMusicReady.load(std::memory_order_acquire);
	const float FadeStep = 1.f / (static_cast<float>(SampleRate) * 2.5f);

	FScopeLock Guard(&Lock);
	for (int32 F = 0; F < Frames; ++F)
	{
		float L = 0.f;
		float R = 0.f;
		for (FVoice& Voice : Voices)
		{
			if (Voice.Cue < 0)
			{
				continue;
			}
			const TArray<int16>& Pcm = Cues[Voice.Cue];
			if (Voice.Position >= Pcm.Num())
			{
				Voice.Cue = -1;
				continue;
			}
			const float S = static_cast<float>(Pcm[Voice.Position++]) / 32768.f;
			L += S * Voice.GainL;
			R += S * Voice.GainR;
		}
		L *= EffectsVolume;
		R *= EffectsVolume;
		if (bMusic)
		{
			for (int32 T = 0; T < 2; ++T)
			{
				const TArray<int16>& Loop = Music[T];
				if (Loop.Num() == 0)
				{
					continue;
				}
				const float Target = MusicTrack == T ? 1.f : 0.f;
				MusicLevel[T] += FMath::Clamp(Target - MusicLevel[T], -FadeStep, FadeStep);
				if (MusicLevel[T] > 0.f)
				{
					const float S = static_cast<float>(Loop[MusicPosition[T]]) / 32768.f * MusicLevel[T] * MusicVolume * 0.8f;
					L += S;
					R += S;
				}
				// Both loops keep running so a cross-fade stays in time.
				MusicPosition[T] = (MusicPosition[T] + 1) % Loop.Num();
			}
		}
		L = AudioSoftClip(L * MasterVolume);
		R = AudioSoftClip(R * MasterVolume);
		Out[F * 2] = static_cast<int16>(FMath::Clamp(L, -1.f, 1.f) * 32767.f);
		Out[F * 2 + 1] = static_cast<int16>(FMath::Clamp(R, -1.f, 1.f) * 32767.f);
	}
}

// ============================================================================ Procedural wave

UBhSynthWave::UBhSynthWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetSampleRate(FBhMixer::SampleRate);
	NumChannels = 2;
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = true;
	SoundGroup = SOUNDGROUP_Default;
	VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
}

int32 UBhSynthWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	const int32 Frames = FMath::Max(0, NumSamples / 2);
	OutAudio.SetNumUninitialized(Frames * 2 * static_cast<int32>(sizeof(int16)));
	int16* Pcm = reinterpret_cast<int16*>(OutAudio.GetData());
	if (Mixer.IsValid())
	{
		Mixer->Render(Pcm, Frames);
	}
	else if (Frames > 0)
	{
		FMemory::Memzero(Pcm, static_cast<SIZE_T>(Frames) * 2 * sizeof(int16));
	}
	return Frames * 2;
}
