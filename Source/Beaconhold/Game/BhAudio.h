// Beaconhold — audio: a small software mixer streamed through one procedural sound wave.
//
// Every sound effect and both music loops are synthesized at startup (Sim/BhSynth) on a worker
// thread, then mixed here. This keeps the game free of audio assets and gives full control over
// polyphony, anti-spam, stereo placement and music cross-fades.
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Sound/SoundWaveProcedural.h"

#include "BhSynth.h"

#include <atomic>

#include "BhAudio.generated.h"

class FBhMixer
{
public:
	static constexpr int32 SampleRate = bh::SynthSampleRate;
	static constexpr int32 MaxVoices = 24;

	// Synthesizes every cue, then both music loops. Slow: run it on a worker thread.
	void Synthesize();
	bool AreEffectsReady() const { return bEffectsReady.load(std::memory_order_acquire); }

	// Game thread. Pan is -1 (left) .. 1 (right). Returns false if the cue was suppressed.
	bool Play(bh::Sfx Cue, float Volume = 1.f, float Pan = 0.f);
	// Game thread: -1 silence, 0 calm loop, 1 battle loop (cross-faded).
	void SetMusic(int32 Track);
	void SetVolumes(float Master, float Music, float Effects);

	// Audio thread: writes Frames stereo frames of 16-bit PCM.
	void Render(int16* Out, int32 Frames);

private:
	struct FVoice
	{
		int32 Cue = -1;
		int32 Position = 0;
		float GainL = 0.f;
		float GainR = 0.f;
	};

	FCriticalSection Lock;
	TArray<int16> Cues[bh::NumSfx];
	TArray<int16> Music[2];
	FVoice Voices[MaxVoices];
	int32 MusicPosition[2] = {0, 0};
	float MusicLevel[2] = {0.f, 0.f};
	int32 MusicTrack = -1;
	float MasterVolume = 1.f;
	float MusicVolume = 0.6f;
	float EffectsVolume = 0.9f;
	double LastPlayed[bh::NumSfx] = {};
	std::atomic<bool> bEffectsReady{false};
	std::atomic<bool> bMusicReady{false};
};

UCLASS()
class UBhSynthWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UBhSynthWave(const FObjectInitializer& ObjectInitializer);

	TSharedPtr<FBhMixer, ESPMode::ThreadSafe> Mixer;

protected:
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;
};
