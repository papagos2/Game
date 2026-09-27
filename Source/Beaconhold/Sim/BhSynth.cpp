// Beaconhold simulation core - procedural audio.
#include "BhSynth.h"

#include "BhData.h"
#include "BhMath.h"

#include <array>
#include <cmath>

namespace bh
{
namespace
{
constexpr float SynTau = 6.28318530718f;

float SynMidi(float Note)
{
	return 440.f * std::pow(2.f, (Note - 69.f) / 12.f);
}

struct SynNoise
{
	uint32_t S = 0x9E3779B9u;
	float White()
	{
		S ^= S << 13;
		S ^= S >> 17;
		S ^= S << 5;
		return static_cast<float>(S >> 8) * (2.f / 16777216.f) - 1.f;
	}
};

// RBJ biquad filter.
struct SynBiquad
{
	float B0 = 1.f, B1 = 0.f, B2 = 0.f, A1 = 0.f, A2 = 0.f;
	float Z1 = 0.f, Z2 = 0.f;

	void Set(int Type, float Sr, float F, float Q)
	{
		const float W0 = SynTau * ClampF(F, 20.f, Sr * 0.45f) / Sr;
		const float Cw = std::cos(W0);
		const float Alpha = std::sin(W0) / (2.f * MaxF(Q, 0.05f));
		float b0 = 0.f, b1 = 0.f, b2 = 0.f;
		const float a0 = 1.f + Alpha;
		switch (Type)
		{
		case 0: // low-pass
			b0 = (1.f - Cw) * 0.5f;
			b1 = 1.f - Cw;
			b2 = (1.f - Cw) * 0.5f;
			break;
		case 1: // high-pass
			b0 = (1.f + Cw) * 0.5f;
			b1 = -(1.f + Cw);
			b2 = (1.f + Cw) * 0.5f;
			break;
		default: // band-pass (constant peak gain)
			b0 = Alpha;
			b1 = 0.f;
			b2 = -Alpha;
			break;
		}
		B0 = b0 / a0;
		B1 = b1 / a0;
		B2 = b2 / a0;
		A1 = (-2.f * Cw) / a0;
		A2 = (1.f - Alpha) / a0;
	}
	float Process(float X)
	{
		const float Y = B0 * X + Z1;
		Z1 = B1 * X - A1 * Y + Z2;
		Z2 = B2 * X - A2 * Y;
		return Y;
	}
};

enum class SynWave : uint8_t
{
	Sine,
	Triangle,
	Square,
	Saw,
};

// PolyBLEP correction to band-limit the saw and square discontinuities (less aliasing).
float SynPolyBlep(float T, float Dt)
{
	if (Dt <= 0.f)
	{
		return 0.f;
	}
	if (T < Dt)
	{
		const float X = T / Dt;
		return X + X - X * X - 1.f;
	}
	if (T > 1.f - Dt)
	{
		const float X = (T - 1.f) / Dt;
		return X * X + X + X + 1.f;
	}
	return 0.f;
}

float SynOsc(SynWave W, float Phase, float Dt)
{
	const float P = Phase - std::floor(Phase);
	switch (W)
	{
	case SynWave::Sine:
		return std::sin(P * SynTau);
	case SynWave::Triangle:
		return 4.f * AbsF(P - 0.5f) - 1.f;
	case SynWave::Square:
	{
		const float Q = P + 0.5f - std::floor(P + 0.5f);
		return 0.8f * ((P < 0.5f ? 1.f : -1.f) + SynPolyBlep(P, Dt) - SynPolyBlep(Q, Dt));
	}
	case SynWave::Saw:
		return 2.f * P - 1.f - SynPolyBlep(P, Dt);
	}
	return 0.f;
}

// A float mix buffer with helpers that add voices at a start time.
class SynBuffer
{
public:
	SynBuffer(int InSr, float Seconds, bool bInLoop = false)
		: Sr(InSr), Data(static_cast<size_t>(static_cast<float>(InSr) * Seconds) + 1, 0.f), bLoop(bInLoop)
	{
	}

	int Sr;
	std::vector<float> Data;
	bool bLoop;
	SynNoise Noise;

	void Add(long Index, float V)
	{
		const long N = static_cast<long>(Data.size());
		if (bLoop)
		{
			Index %= N;
			if (Index < 0)
			{
				Index += N;
			}
		}
		if (Index >= 0 && Index < N)
		{
			Data[static_cast<size_t>(Index)] += V;
		}
	}

	// Tone with exponential frequency glide, attack and exponential decay (Tau seconds).
	void Tone(float Start, float Dur, float F0, float F1, float Amp, float Attack, float Tau, SynWave W, float Vibrato = 0.f)
	{
		const long S0 = static_cast<long>(Start * static_cast<float>(Sr));
		const long N = static_cast<long>(Dur * static_cast<float>(Sr));
		float Phase = 0.f;
		for (long I = 0; I < N; ++I)
		{
			const float T = static_cast<float>(I) / static_cast<float>(Sr);
			const float K = Dur > 0.f ? T / Dur : 0.f;
			float F = F0 * std::pow(F1 / F0, K);
			if (Vibrato > 0.f)
			{
				F *= 1.f + Vibrato * std::sin(T * SynTau * 5.5f);
			}
			Phase += F / static_cast<float>(Sr);
			const float Env = (Attack > 0.f ? MinF(1.f, T / Attack) : 1.f) * std::exp(-T / MaxF(Tau, 1e-3f)) * MinF(1.f, (Dur - T) * 60.f);
			Add(S0 + I, SynOsc(W, Phase, F / static_cast<float>(Sr)) * Amp * Env);
		}
	}

	// Filtered noise burst; the filter frequency sweeps from F0 to F1.
	void NoiseBurst(float Start, float Dur, float Amp, float Attack, float Tau, int FilterType, float F0, float F1, float Q)
	{
		const long S0 = static_cast<long>(Start * static_cast<float>(Sr));
		const long N = static_cast<long>(Dur * static_cast<float>(Sr));
		SynBiquad Filter;
		for (long I = 0; I < N; ++I)
		{
			const float T = static_cast<float>(I) / static_cast<float>(Sr);
			if (I % 32 == 0)
			{
				const float K = Dur > 0.f ? T / Dur : 0.f;
				Filter.Set(FilterType, static_cast<float>(Sr), F0 * std::pow(F1 / F0, K), Q);
			}
			const float Env = (Attack > 0.f ? MinF(1.f, T / Attack) : 1.f) * std::exp(-T / MaxF(Tau, 1e-3f)) * MinF(1.f, (Dur - T) * 60.f);
			Add(S0 + I, Filter.Process(Noise.White()) * Amp * Env);
		}
	}

	// Karplus-Strong plucked string.
	void Pluck(float Start, float Freq, float Amp, float Seconds, float Brightness)
	{
		const int Period = MaxI(2, static_cast<int>(static_cast<float>(Sr) / Freq));
		std::vector<float> Ring(static_cast<size_t>(Period));
		SynBiquad Shape;
		Shape.Set(0, static_cast<float>(Sr), 800.f + Brightness * 5000.f, 0.7f);
		for (float& V : Ring)
		{
			V = Shape.Process(Noise.White());
		}
		const long S0 = static_cast<long>(Start * static_cast<float>(Sr));
		const long N = static_cast<long>(Seconds * static_cast<float>(Sr));
		const float Decay = std::pow(0.001f, 1.f / (Seconds * Freq));
		size_t Pos = 0;
		float Prev = 0.f;
		for (long I = 0; I < N; ++I)
		{
			const float Cur = Ring[Pos];
			const float Next = (Cur + Prev) * 0.5f * Decay;
			Prev = Cur;
			Ring[Pos] = Next;
			Pos = (Pos + 1) % Ring.size();
			const float Fade = MinF(1.f, static_cast<float>(N - I) / (0.02f * static_cast<float>(Sr)));
			Add(S0 + I, Cur * Amp * Fade);
		}
	}

	// Two-operator FM bell.
	void Bell(float Start, float Freq, float Amp, float Tau, float Ratio = 3.5f, float Index = 2.5f)
	{
		const long S0 = static_cast<long>(Start * static_cast<float>(Sr));
		const long N = static_cast<long>(Tau * 5.f * static_cast<float>(Sr));
		for (long I = 0; I < N; ++I)
		{
			const float T = static_cast<float>(I) / static_cast<float>(Sr);
			const float Env = MinF(1.f, T * 400.f) * std::exp(-T / Tau);
			const float Mod = std::sin(SynTau * Freq * Ratio * T) * Index * std::exp(-T / (Tau * 0.4f));
			Add(S0 + I, std::sin(SynTau * Freq * T + Mod) * Amp * Env);
		}
	}

	// Warm pad chord (a few detuned saw-ish partials through a soft envelope).
	void Pad(float Start, float Dur, const float* Notes, int Count, float Amp)
	{
		const long S0 = static_cast<long>(Start * static_cast<float>(Sr));
		const long N = static_cast<long>(Dur * static_cast<float>(Sr));
		for (int K = 0; K < Count; ++K)
		{
			const float F = SynMidi(Notes[K]);
			float P1 = 0.f;
			float P2 = 0.13f;
			for (long I = 0; I < N; ++I)
			{
				const float T = static_cast<float>(I) / static_cast<float>(Sr);
				const float Env = SmoothStep(MinF(T / (Dur * 0.35f), 1.f)) * SmoothStep(MinF((Dur - T) / (Dur * 0.35f), 1.f));
				P1 += F * 1.003f / static_cast<float>(Sr);
				P2 += F * 0.997f / static_cast<float>(Sr);
				const float V = std::sin(P1 * SynTau) + 0.35f * std::sin(P1 * SynTau * 2.f) + 0.15f * std::sin(P2 * SynTau * 3.f) + std::sin(P2 * SynTau);
				Add(S0 + I, V * Amp * 0.5f * Env);
			}
		}
	}

	void Finish(std::vector<int16_t>& Out, float Peak)
	{
		float Max = 1e-6f;
		for (float V : Data)
		{
			Max = MaxF(Max, AbsF(V));
		}
		const float Gain = Peak / Max;
		Out.resize(Data.size());
		for (size_t I = 0; I < Data.size(); ++I)
		{
			const float V = std::tanh(Data[I] * Gain * 1.1f) / std::tanh(1.1f);
			Out[I] = static_cast<int16_t>(ClampI(static_cast<int>(V * 32767.f), -32767, 32767));
		}
	}
};

std::array<SfxInfo, NumSfx> SynBuildInfo()
{
	std::array<SfxInfo, NumSfx> T{};
	auto Set = [&T](Sfx S, const char* Name, float Vol, float Interval, bool bPos)
	{
		SfxInfo& I = T[static_cast<size_t>(S)];
		I.Name = Name;
		I.Volume = Vol;
		I.MinInterval = Interval;
		I.bPositional = bPos;
	};
	Set(Sfx::UiTap, "UiTap", 0.5f, 0.03f, false);
	Set(Sfx::UiConfirm, "UiConfirm", 0.6f, 0.05f, false);
	Set(Sfx::UiError, "UiError", 0.55f, 0.25f, false);
	Set(Sfx::Select, "Select", 0.5f, 0.06f, false);
	Set(Sfx::Acknowledge, "Acknowledge", 0.5f, 0.15f, false);
	Set(Sfx::SwordHit, "SwordHit", 0.55f, 0.05f, true);
	Set(Sfx::ArrowShoot, "ArrowShoot", 0.45f, 0.06f, true);
	Set(Sfx::ArrowHit, "ArrowHit", 0.45f, 0.06f, true);
	Set(Sfx::MagicCast, "MagicCast", 0.45f, 0.08f, true);
	Set(Sfx::MagicHit, "MagicHit", 0.45f, 0.08f, true);
	Set(Sfx::Chop, "Chop", 0.35f, 0.12f, true);
	Set(Sfx::Mine, "Mine", 0.35f, 0.12f, true);
	Set(Sfx::Hammer, "Hammer", 0.4f, 0.12f, true);
	Set(Sfx::Deliver, "Deliver", 0.3f, 0.25f, true);
	Set(Sfx::BuildComplete, "BuildComplete", 0.7f, 0.3f, false);
	Set(Sfx::UnitTrained, "UnitTrained", 0.55f, 0.2f, false);
	Set(Sfx::ResearchDone, "ResearchDone", 0.7f, 0.5f, false);
	Set(Sfx::UnitDie, "UnitDie", 0.5f, 0.1f, true);
	Set(Sfx::GloamDie, "GloamDie", 0.5f, 0.08f, true);
	Set(Sfx::BuildingCollapse, "BuildingCollapse", 0.8f, 0.3f, true);
	Set(Sfx::TowerShoot, "TowerShoot", 0.45f, 0.08f, true);
	Set(Sfx::Alarm, "Alarm", 0.8f, 4.f, false);
	Set(Sfx::WaveHorn, "WaveHorn", 0.85f, 3.f, false);
	Set(Sfx::BeaconLit, "BeaconLit", 0.85f, 1.f, false);
	Set(Sfx::Victory, "Victory", 0.9f, 5.f, false);
	Set(Sfx::Defeat, "Defeat", 0.9f, 5.f, false);
	Set(Sfx::Heal, "Heal", 0.3f, 0.3f, true);
	Set(Sfx::Charge, "Charge", 0.6f, 0.3f, true);
	Set(Sfx::Brace, "Brace", 0.6f, 0.3f, true);
	Set(Sfx::Sunburst, "Sunburst", 0.75f, 0.3f, true);
	Set(Sfx::StarAward, "StarAward", 0.8f, 0.2f, false);
	return T;
}
} // namespace

const SfxInfo& GetSfxInfo(Sfx S)
{
	static const std::array<SfxInfo, NumSfx> Table = SynBuildInfo();
	const int I = static_cast<int>(S);
	return Table[static_cast<size_t>(I >= 0 && I < NumSfx ? I : 0)];
}

void SynthSfx(Sfx S, int Sr, std::vector<int16_t>& Out)
{
	switch (S)
	{
	case Sfx::UiTap:
	{
		SynBuffer B(Sr, 0.07f);
		B.Tone(0.f, 0.07f, 1300.f, 900.f, 0.6f, 0.002f, 0.02f, SynWave::Sine);
		B.Finish(Out, 0.7f);
		return;
	}
	case Sfx::UiConfirm:
	{
		SynBuffer B(Sr, 0.3f);
		B.Bell(0.f, SynMidi(76), 0.5f, 0.08f, 2.f, 1.2f);
		B.Bell(0.07f, SynMidi(83), 0.5f, 0.1f, 2.f, 1.2f);
		B.Finish(Out, 0.7f);
		return;
	}
	case Sfx::UiError:
	{
		SynBuffer B(Sr, 0.28f);
		B.Tone(0.f, 0.11f, 190.f, 170.f, 0.5f, 0.003f, 0.2f, SynWave::Square);
		B.Tone(0.14f, 0.12f, 160.f, 140.f, 0.5f, 0.003f, 0.2f, SynWave::Square);
		B.Finish(Out, 0.55f);
		return;
	}
	case Sfx::Select:
	{
		SynBuffer B(Sr, 0.1f);
		B.Tone(0.f, 0.1f, 700.f, 520.f, 0.6f, 0.002f, 0.03f, SynWave::Triangle);
		B.NoiseBurst(0.f, 0.03f, 0.3f, 0.001f, 0.01f, 2, 2500.f, 2000.f, 1.5f);
		B.Finish(Out, 0.6f);
		return;
	}
	case Sfx::Acknowledge:
	{
		SynBuffer B(Sr, 0.35f);
		B.Pluck(0.f, SynMidi(57), 0.6f, 0.3f, 0.5f);
		B.Pluck(0.03f, SynMidi(64), 0.45f, 0.3f, 0.5f);
		B.Finish(Out, 0.6f);
		return;
	}
	case Sfx::SwordHit:
	{
		SynBuffer B(Sr, 0.3f);
		B.NoiseBurst(0.f, 0.06f, 1.f, 0.001f, 0.015f, 1, 3000.f, 1500.f, 0.8f);
		const float Partials[4] = {1320.f, 2170.f, 3380.f, 4410.f};
		for (int K = 0; K < 4; ++K)
		{
			B.Tone(0.f, 0.28f, Partials[K], Partials[K] * 0.99f, 0.25f / static_cast<float>(K + 1), 0.001f, 0.06f + 0.02f * static_cast<float>(K), SynWave::Sine);
		}
		B.Tone(0.f, 0.08f, 220.f, 120.f, 0.5f, 0.001f, 0.03f, SynWave::Sine);
		B.Finish(Out, 0.75f);
		return;
	}
	case Sfx::ArrowShoot:
	case Sfx::TowerShoot:
	{
		const bool bTower = S == Sfx::TowerShoot;
		SynBuffer B(Sr, 0.3f);
		B.Pluck(0.f, bTower ? 150.f : 210.f, 0.6f, 0.2f, 0.3f);
		B.NoiseBurst(0.01f, 0.22f, 0.7f, 0.02f, 0.07f, 2, 1800.f, 5200.f, 2.f);
		B.Finish(Out, 0.6f);
		return;
	}
	case Sfx::ArrowHit:
	{
		SynBuffer B(Sr, 0.18f);
		B.Tone(0.f, 0.15f, 190.f, 90.f, 0.8f, 0.001f, 0.04f, SynWave::Sine);
		B.NoiseBurst(0.f, 0.05f, 0.5f, 0.001f, 0.012f, 0, 2500.f, 900.f, 0.7f);
		B.Finish(Out, 0.65f);
		return;
	}
	case Sfx::MagicCast:
	{
		SynBuffer B(Sr, 0.45f);
		const float Notes[4] = {72.f, 76.f, 79.f, 84.f};
		for (int K = 0; K < 4; ++K)
		{
			B.Bell(0.05f * static_cast<float>(K), SynMidi(Notes[K]), 0.35f, 0.08f, 1.5f, 1.5f);
		}
		B.NoiseBurst(0.f, 0.35f, 0.15f, 0.08f, 0.15f, 2, 3000.f, 7000.f, 3.f);
		B.Finish(Out, 0.55f);
		return;
	}
	case Sfx::MagicHit:
	{
		SynBuffer B(Sr, 0.35f);
		B.Bell(0.f, SynMidi(88), 0.5f, 0.07f, 2.7f, 3.f);
		B.NoiseBurst(0.f, 0.2f, 0.4f, 0.002f, 0.05f, 2, 6000.f, 2500.f, 1.5f);
		B.Tone(0.f, 0.12f, 300.f, 150.f, 0.4f, 0.001f, 0.04f, SynWave::Sine);
		B.Finish(Out, 0.6f);
		return;
	}
	case Sfx::Chop:
	{
		SynBuffer B(Sr, 0.14f);
		B.Tone(0.f, 0.12f, 240.f, 150.f, 0.9f, 0.001f, 0.03f, SynWave::Triangle);
		B.NoiseBurst(0.f, 0.05f, 0.6f, 0.001f, 0.01f, 2, 1400.f, 900.f, 1.2f);
		B.Finish(Out, 0.6f);
		return;
	}
	case Sfx::Mine:
	{
		SynBuffer B(Sr, 0.3f);
		B.Bell(0.f, 1560.f, 0.4f, 0.06f, 2.76f, 1.8f);
		B.Bell(0.f, 2350.f, 0.25f, 0.05f, 1.5f, 1.f);
		B.NoiseBurst(0.f, 0.03f, 0.5f, 0.001f, 0.008f, 1, 4000.f, 3000.f, 0.7f);
		B.Finish(Out, 0.55f);
		return;
	}
	case Sfx::Hammer:
	{
		SynBuffer B(Sr, 0.14f);
		B.Tone(0.f, 0.12f, 320.f, 220.f, 0.8f, 0.001f, 0.025f, SynWave::Triangle);
		B.NoiseBurst(0.f, 0.04f, 0.5f, 0.001f, 0.01f, 2, 1800.f, 1200.f, 1.f);
		B.Finish(Out, 0.6f);
		return;
	}
	case Sfx::Deliver:
	{
		SynBuffer B(Sr, 0.35f);
		B.Bell(0.f, SynMidi(84), 0.4f, 0.06f, 3.f, 1.2f);
		B.Bell(0.06f, SynMidi(91), 0.35f, 0.08f, 3.f, 1.2f);
		B.Finish(Out, 0.5f);
		return;
	}
	case Sfx::BuildComplete:
	{
		SynBuffer B(Sr, 1.1f);
		const float Notes[4] = {60.f, 64.f, 67.f, 72.f};
		for (int K = 0; K < 4; ++K)
		{
			B.Pluck(0.08f * static_cast<float>(K), SynMidi(Notes[K]), 0.6f, 0.9f, 0.6f);
		}
		B.Tone(0.f, 0.1f, 180.f, 120.f, 0.5f, 0.001f, 0.04f, SynWave::Triangle);
		B.Finish(Out, 0.7f);
		return;
	}
	case Sfx::UnitTrained:
	{
		SynBuffer B(Sr, 0.5f);
		B.Tone(0.f, 0.45f, SynMidi(62), SynMidi(62), 0.45f, 0.04f, 0.3f, SynWave::Saw, 0.004f);
		B.Tone(0.12f, 0.33f, SynMidi(69), SynMidi(69), 0.4f, 0.03f, 0.25f, SynWave::Saw, 0.004f);
		SynBuffer F(Sr, 0.5f);
		SynBiquad Lp;
		Lp.Set(0, static_cast<float>(Sr), 1800.f, 0.7f);
		for (size_t I = 0; I < B.Data.size(); ++I)
		{
			F.Data[I] = Lp.Process(B.Data[I]);
		}
		F.Finish(Out, 0.6f);
		return;
	}
	case Sfx::ResearchDone:
	{
		SynBuffer B(Sr, 1.2f);
		const float Notes[5] = {67.f, 71.f, 74.f, 79.f, 83.f};
		for (int K = 0; K < 5; ++K)
		{
			B.Bell(0.07f * static_cast<float>(K), SynMidi(Notes[K]), 0.4f, 0.25f, 2.f, 1.f);
		}
		B.Finish(Out, 0.65f);
		return;
	}
	case Sfx::UnitDie:
	{
		SynBuffer B(Sr, 0.5f);
		B.Tone(0.f, 0.45f, 330.f, 140.f, 0.5f, 0.005f, 0.18f, SynWave::Triangle);
		B.NoiseBurst(0.f, 0.15f, 0.4f, 0.002f, 0.05f, 0, 1200.f, 300.f, 0.7f);
		B.Finish(Out, 0.55f);
		return;
	}
	case Sfx::GloamDie:
	{
		SynBuffer B(Sr, 0.45f);
		B.Tone(0.f, 0.4f, 520.f, 90.f, 0.6f, 0.003f, 0.15f, SynWave::Saw);
		B.NoiseBurst(0.f, 0.3f, 0.5f, 0.005f, 0.1f, 2, 900.f, 250.f, 2.f);
		SynBuffer F(Sr, 0.45f);
		SynBiquad Lp;
		Lp.Set(0, static_cast<float>(Sr), 1400.f, 1.2f);
		for (size_t I = 0; I < B.Data.size(); ++I)
		{
			F.Data[I] = Lp.Process(B.Data[I]);
		}
		F.Finish(Out, 0.55f);
		return;
	}
	case Sfx::BuildingCollapse:
	{
		SynBuffer B(Sr, 1.5f);
		B.NoiseBurst(0.f, 1.4f, 1.f, 0.01f, 0.45f, 0, 700.f, 120.f, 0.8f);
		B.Tone(0.f, 0.6f, 90.f, 45.f, 0.8f, 0.005f, 0.25f, SynWave::Sine);
		for (int K = 0; K < 5; ++K)
		{
			B.Tone(0.1f + 0.17f * static_cast<float>(K), 0.1f, 260.f - 30.f * static_cast<float>(K), 140.f, 0.35f, 0.001f, 0.03f, SynWave::Triangle);
		}
		B.Finish(Out, 0.85f);
		return;
	}
	case Sfx::Alarm:
	{
		SynBuffer B(Sr, 1.2f);
		B.Tone(0.f, 0.5f, SynMidi(50), SynMidi(50), 0.5f, 0.06f, 0.6f, SynWave::Saw, 0.006f);
		B.Tone(0.5f, 0.65f, SynMidi(53), SynMidi(53), 0.5f, 0.05f, 0.7f, SynWave::Saw, 0.006f);
		SynBuffer F(Sr, 1.2f);
		SynBiquad Lp;
		Lp.Set(0, static_cast<float>(Sr), 1100.f, 0.9f);
		for (size_t I = 0; I < B.Data.size(); ++I)
		{
			F.Data[I] = Lp.Process(B.Data[I]);
		}
		F.Finish(Out, 0.8f);
		return;
	}
	case Sfx::WaveHorn:
	{
		SynBuffer B(Sr, 1.6f);
		B.Tone(0.f, 1.5f, SynMidi(38), SynMidi(37), 0.5f, 0.2f, 1.2f, SynWave::Saw, 0.01f);
		B.Tone(0.f, 1.5f, SynMidi(38.35f), SynMidi(37.4f), 0.45f, 0.25f, 1.2f, SynWave::Saw, 0.012f);
		B.Tone(0.1f, 1.3f, SynMidi(44), SynMidi(43), 0.3f, 0.3f, 1.f, SynWave::Square, 0.01f);
		SynBuffer F(Sr, 1.6f);
		SynBiquad Lp;
		Lp.Set(0, static_cast<float>(Sr), 700.f, 1.4f);
		for (size_t I = 0; I < B.Data.size(); ++I)
		{
			F.Data[I] = Lp.Process(B.Data[I]);
		}
		F.Finish(Out, 0.85f);
		return;
	}
	case Sfx::BeaconLit:
	{
		SynBuffer B(Sr, 2.0f);
		B.NoiseBurst(0.f, 0.9f, 0.6f, 0.4f, 0.4f, 2, 400.f, 3500.f, 1.2f);
		const float Notes[4] = {62.f, 66.f, 69.f, 74.f};
		for (int K = 0; K < 4; ++K)
		{
			B.Bell(0.55f + 0.05f * static_cast<float>(K), SynMidi(Notes[K]), 0.35f, 0.45f, 2.f, 1.f);
		}
		B.Finish(Out, 0.8f);
		return;
	}
	case Sfx::Victory:
	{
		SynBuffer B(Sr, 3.2f);
		const float Notes[7] = {60.f, 64.f, 67.f, 72.f, 67.f, 72.f, 76.f};
		const float Times[7] = {0.f, 0.15f, 0.3f, 0.45f, 0.75f, 0.9f, 1.05f};
		for (int K = 0; K < 7; ++K)
		{
			B.Pluck(Times[K], SynMidi(Notes[K]), 0.6f, 1.2f, 0.7f);
		}
		const float Chord[4] = {48.f, 55.f, 64.f, 72.f};
		B.Pad(1.0f, 2.1f, Chord, 4, 0.18f);
		B.Bell(1.05f, SynMidi(84), 0.35f, 0.6f, 2.f, 0.8f);
		B.Finish(Out, 0.85f);
		return;
	}
	case Sfx::Defeat:
	{
		SynBuffer B(Sr, 3.2f);
		const float Notes[5] = {69.f, 65.f, 62.f, 60.f, 57.f};
		for (int K = 0; K < 5; ++K)
		{
			B.Pluck(0.3f * static_cast<float>(K), SynMidi(Notes[K]), 0.55f, 1.2f, 0.4f);
		}
		const float Chord[3] = {45.f, 52.f, 60.f};
		B.Pad(1.2f, 2.f, Chord, 3, 0.18f);
		B.Finish(Out, 0.8f);
		return;
	}
	case Sfx::Heal:
	{
		SynBuffer B(Sr, 0.5f);
		B.Tone(0.f, 0.45f, SynMidi(72), SynMidi(79), 0.35f, 0.08f, 0.25f, SynWave::Sine);
		B.Tone(0.f, 0.45f, SynMidi(76), SynMidi(84), 0.25f, 0.1f, 0.25f, SynWave::Sine);
		B.Finish(Out, 0.45f);
		return;
	}
	case Sfx::Charge:
	{
		SynBuffer B(Sr, 0.7f);
		for (int K = 0; K < 4; ++K)
		{
			B.Tone(0.1f * static_cast<float>(K), 0.08f, 140.f, 90.f, 0.6f, 0.001f, 0.03f, SynWave::Triangle);
		}
		B.Tone(0.2f, 0.45f, SynMidi(62), SynMidi(69), 0.4f, 0.03f, 0.3f, SynWave::Saw, 0.004f);
		B.NoiseBurst(0.f, 0.6f, 0.3f, 0.1f, 0.3f, 2, 600.f, 2500.f, 1.f);
		B.Finish(Out, 0.65f);
		return;
	}
	case Sfx::Brace:
	{
		SynBuffer B(Sr, 0.4f);
		B.Tone(0.f, 0.35f, 520.f, 500.f, 0.4f, 0.001f, 0.12f, SynWave::Triangle);
		B.Tone(0.f, 0.35f, 1180.f, 1160.f, 0.25f, 0.001f, 0.1f, SynWave::Sine);
		B.NoiseBurst(0.f, 0.05f, 0.5f, 0.001f, 0.01f, 1, 2500.f, 2000.f, 0.7f);
		B.Finish(Out, 0.6f);
		return;
	}
	case Sfx::Sunburst:
	{
		SynBuffer B(Sr, 1.0f);
		B.Tone(0.f, 0.4f, 120.f, 60.f, 0.7f, 0.002f, 0.15f, SynWave::Sine);
		B.NoiseBurst(0.f, 0.6f, 0.6f, 0.005f, 0.2f, 0, 5000.f, 800.f, 0.8f);
		const float Notes[3] = {74.f, 78.f, 81.f};
		for (int K = 0; K < 3; ++K)
		{
			B.Bell(0.02f, SynMidi(Notes[K]), 0.3f, 0.35f, 2.f, 1.2f);
		}
		B.Finish(Out, 0.8f);
		return;
	}
	case Sfx::StarAward:
	{
		SynBuffer B(Sr, 0.8f);
		B.Bell(0.f, SynMidi(88), 0.5f, 0.2f, 3.f, 1.5f);
		B.Bell(0.08f, SynMidi(95), 0.4f, 0.25f, 3.f, 1.5f);
		B.Finish(Out, 0.7f);
		return;
	}
	case Sfx::Count:
		break;
	}
	Out.clear();
}

void SynthMusic(int Sr, float Seconds, bool bBattle, std::vector<int16_t>& Out)
{
	// D Dorian-ish progression: Dm - Bb - F - C, four bars of four seconds, repeated with variation.
	const float Bar = 4.f;
	const int Bars = MaxI(4, static_cast<int>(Seconds / Bar + 0.5f));
	SynBuffer B(Sr, Bar * static_cast<float>(Bars), true);
	const float Roots[4] = {50.f, 46.f, 53.f, 48.f};    // D3, Bb2, F3, C3
	const float Thirds[4] = {3.f, 4.f, 4.f, 4.f};       // minor on i, major elsewhere
	const float Pentatonic[6] = {62.f, 65.f, 67.f, 69.f, 72.f, 74.f};
	Rng Random(0xBEAC04u);
	for (int BarIndex = 0; BarIndex < Bars; ++BarIndex)
	{
		const int C = BarIndex % 4;
		const float T0 = Bar * static_cast<float>(BarIndex);
		const float Root = Roots[C];
		const float Chord[3] = {Root - 12.f, Root - 5.f, Root + Thirds[C]};
		B.Pad(T0 - 0.6f, Bar + 1.2f, Chord, 3, bBattle ? 0.12f : 0.1f);

		// Lute arpeggio (8ths, or 16ths in battle)
		const int Steps = bBattle ? 16 : 8;
		const float Pattern[8] = {0.f, 7.f, 12.f, Thirds[C] + 12.f, 19.f, Thirds[C] + 12.f, 12.f, 7.f};
		for (int S = 0; S < Steps; ++S)
		{
			const float Note = Root + Pattern[S % 8];
			const float Amp = (S % 4 == 0) ? 0.34f : 0.24f;
			B.Pluck(T0 + static_cast<float>(S) * Bar / static_cast<float>(Steps), SynMidi(Note), Amp, 1.4f, 0.3f);
		}

		// Sparse chime melody on alternate bars
		if (BarIndex % 2 == 1 || bBattle)
		{
			const int Count = Random.RangeInt(1, 3);
			for (int K = 0; K < Count; ++K)
			{
				const float When = T0 + static_cast<float>(Random.RangeInt(0, 7)) * 0.5f;
				B.Bell(When, SynMidi(Pentatonic[Random.RangeInt(0, 5)]), 0.14f, 0.7f, 2.f, 0.8f);
			}
		}

		if (bBattle)
		{
			for (int Beat = 0; Beat < 4; ++Beat)
			{
				const float When = T0 + static_cast<float>(Beat);
				B.Tone(When, 0.35f, 95.f, 48.f, 0.55f, 0.002f, 0.12f, SynWave::Sine);
				if (Beat % 2 == 1)
				{
					B.NoiseBurst(When, 0.15f, 0.2f, 0.002f, 0.05f, 2, 1800.f, 1200.f, 1.f);
				}
			}
		}
	}
	// Warm, non-fatiguing background: gentle low-pass run twice around the loop so the seam stays continuous.
	SynBiquad Warm;
	Warm.Set(0, static_cast<float>(Sr), 4200.f, 0.6f);
	for (int Pass = 0; Pass < 2; ++Pass)
	{
		for (float& V : B.Data)
		{
			const float Y = Warm.Process(V);
			if (Pass == 1)
			{
				V = Y;
			}
		}
	}
	B.Finish(Out, 0.6f);
}

bool SfxForEvent(const GameEvent& E, Sfx& Out)
{
	const bool bMine = E.Owner == Team::Player;
	switch (E.Type)
	{
	case EventType::ProjectileFired:
		switch (E.Arch)
		{
		case Archetype::Ranger:
			Out = Sfx::ArrowShoot;
			return true;
		case Archetype::Watchtower:
		case Archetype::ThornSpire:
			Out = Sfx::TowerShoot;
			return true;
		default:
			Out = Sfx::MagicCast;
			return true;
		}
	case EventType::Hit:
	{
		const Archetype Src = static_cast<Archetype>(E.Sub);
		if (Src == Archetype::Ranger || Src == Archetype::Watchtower || Src == Archetype::ThornSpire)
		{
			Out = Sfx::ArrowHit;
		}
		else if (Src == Archetype::Sage || Src == Archetype::Hexer)
		{
			Out = Sfx::MagicHit;
		}
		else
		{
			Out = Sfx::SwordHit;
		}
		return true;
	}
	case EventType::Healed:
		Out = Sfx::Heal;
		return true;
	case EventType::GatherStrike:
		Out = E.Sub == 0 ? Sfx::Mine : (E.Sub == 1 ? Sfx::Chop : Sfx::Hammer);
		return true;
	case EventType::ResourceDelivered:
		Out = Sfx::Deliver;
		return bMine;
	case EventType::BuildingCompleted:
		Out = E.Arch == Archetype::Beacon ? Sfx::BeaconLit : Sfx::BuildComplete;
		return bMine;
	case EventType::UnitTrained:
		Out = Sfx::UnitTrained;
		return bMine;
	case EventType::ResearchCompleted:
		Out = Sfx::ResearchDone;
		return bMine;
	case EventType::EntityDied:
		if (E.Arch != Archetype::None && E.Arch < Archetype::Count && GetDef(E.Arch).Kind == EntityKind::Building)
		{
			Out = Sfx::BuildingCollapse;
		}
		else if (E.Arch != Archetype::None && E.Arch < Archetype::Count && GetDef(E.Arch).Kind == EntityKind::Resource)
		{
			return false;
		}
		else
		{
			Out = E.Owner == Team::Enemy ? Sfx::GloamDie : Sfx::UnitDie;
		}
		return true;
	case EventType::UnderAttack:
		Out = Sfx::Alarm;
		return bMine;
	case EventType::WaveIncoming:
		Out = Sfx::WaveHorn;
		return true;
	case EventType::AbilityCast:
		switch (static_cast<Ability>(E.Sub))
		{
		case Ability::Brace:
			Out = Sfx::Brace;
			return true;
		case Ability::Volley:
			Out = Sfx::ArrowShoot;
			return true;
		case Ability::Charge:
			Out = Sfx::Charge;
			return true;
		case Ability::Sunburst:
			Out = Sfx::Sunburst;
			return true;
		case Ability::None:
		case Ability::Count:
			break;
		}
		return false;
	case EventType::CommandMove:
	case EventType::CommandAttack:
	case EventType::CommandGather:
	case EventType::CommandBuild:
		Out = Sfx::Acknowledge;
		return bMine;
	case EventType::BuildingPlaced:
		Out = Sfx::Hammer;
		return bMine;
	case EventType::NotEnoughSunstone:
	case EventType::NotEnoughTimber:
	case EventType::SupplyBlocked:
	case EventType::InvalidPlacement:
	case EventType::RequirementMissing:
	case EventType::QueueFull:
	case EventType::Notice:
		Out = Sfx::UiError;
		return bMine;
	case EventType::ObjectiveCompleted:
		Out = Sfx::StarAward;
		return true;
	case EventType::MissionWon:
		Out = Sfx::Victory;
		return true;
	case EventType::MissionLost:
		Out = Sfx::Defeat;
		return true;
	case EventType::TutorialStep:
		Out = Sfx::UiConfirm;
		return true;
	case EventType::EntitySpawned:
	case EventType::ConstructionCancelled:
	case EventType::AttackStarted:
	case EventType::TreeFelled:
	case EventType::NodeDepleted:
	case EventType::WaveSpawned:
	case EventType::ObjectiveFailed:
		break;
	}
	return false;
}

} // namespace bh
