// Beaconhold — the director.
#include "Game/BhDirector.h"

#include "Game/BhAssets.h"
#include "Game/BhAudio.h"
#include "Game/BhCommon.h"
#include "Game/BhGameInstance.h"

#include "BhSerialize.h"

#include "Async/Async.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include <algorithm>

namespace
{
constexpr int32 MinimapPixelsPerTile = 2;

FLinearColor DirResourceColor(int32 Resource)
{
	return Resource == 0 ? FLinearColor(1.f, 0.82f, 0.3f) : FLinearColor(0.85f, 0.62f, 0.38f);
}

bh::TileRect DirFootprint(bh::Archetype A, const bh::Vec2& Center)
{
	const int32 F = FMath::Max(1, bh::GetDef(A).Footprint);
	const int32 X0 = FMath::FloorToInt(Center.X - static_cast<float>(F) * 0.5f + 0.01f);
	const int32 Y0 = FMath::FloorToInt(Center.Y - static_cast<float>(F) * 0.5f + 0.01f);
	return bh::TileRect(X0, Y0, X0 + F, Y0 + F);
}
} // namespace

// ============================================================================ Projector

bool FBhProjector::ScreenToGround(float X, float Y, bh::Vec2& Out) const
{
	const APlayerController* PC = Controller.Get();
	if (PC == nullptr)
	{
		return false;
	}
	FVector Origin;
	FVector Direction;
	if (!PC->DeprojectScreenPositionToWorld(X, Y, Origin, Direction) || Direction.Z > -1e-4)
	{
		return false;
	}
	const double T = -Origin.Z / Direction.Z;
	Out = BhUE::ToSim(Origin + Direction * T);
	return true;
}

bool FBhProjector::WorldToScreen(const bh::Vec2& P, float Height, float& OutX, float& OutY) const
{
	const APlayerController* PC = Controller.Get();
	if (PC == nullptr)
	{
		return false;
	}
	FVector2D Screen;
	if (!PC->ProjectWorldLocationToScreen(BhUE::ToWorld(P, Height), Screen, true))
	{
		return false;
	}
	OutX = static_cast<float>(Screen.X);
	OutY = static_cast<float>(Screen.Y);
	return true;
}

float FBhProjector::GetScreenWidth() const
{
	int32 W = 0;
	int32 H = 0;
	if (const APlayerController* PC = Controller.Get())
	{
		PC->GetViewportSize(W, H);
	}
	return static_cast<float>(W);
}

float FBhProjector::GetScreenHeight() const
{
	int32 W = 0;
	int32 H = 0;
	if (const APlayerController* PC = Controller.Get())
	{
		PC->GetViewportSize(W, H);
	}
	return static_cast<float>(H);
}

// ============================================================================ Lifetime

ABhDirector::ABhDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SceneRoot);
	Camera->SetUsingAbsoluteLocation(true);
	Camera->SetUsingAbsoluteRotation(true);
	Camera->bConstrainAspectRatio = false;
	// The palette already carries the final colours: keep exposure fixed and effects off.
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = AEM_Manual;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = 0.f;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = 0;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.f;
	Camera->PostProcessBlendWeight = 1.f;
}

void ABhDirector::BeginPlay()
{
	Super::BeginPlay();
	if (UBhGameInstance* GI = GetGameInstance<UBhGameInstance>())
	{
		Assets = GI->GetAssets();
		MusicVolume = GI->GetUserSettings().MusicVolume;
		EffectsVolume = GI->GetUserSettings().SfxVolume;
	}
	WorldView.Init(this, Assets);
	Fx.Init(this, Assets);

	// Exact palette colours in development builds on desktop (mobile renders LDR anyway).
	if (UGameViewportClient* Viewport = GetWorld() != nullptr ? GetWorld()->GetGameViewport() : nullptr)
	{
		Viewport->EngineShowFlags.SetTonemapper(false);
		Viewport->EngineShowFlags.SetEyeAdaptation(false);
	}

	// Audio: synthesize everything off the game thread, stream through one procedural wave.
	Mixer = MakeShared<FBhMixer, ESPMode::ThreadSafe>();
	Mixer->SetVolumes(1.f, MusicVolume, EffectsVolume);
	TSharedPtr<FBhMixer, ESPMode::ThreadSafe> Worker = Mixer;
	(void)Async(EAsyncExecution::ThreadPool, [Worker]() { Worker->Synthesize(); });
	SynthWave = NewObject<UBhSynthWave>(this);
	SynthWave->Mixer = Mixer;
	AudioComponent = NewObject<UAudioComponent>(this);
	AudioComponent->bAutoActivate = false;
	AudioComponent->bIsUISound = true;
	AudioComponent->bAllowSpatialization = false;
	AudioComponent->SetSound(SynthWave);
	AudioComponent->RegisterComponent();
	AudioComponent->Play();
}

void ABhDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Leaving the app mid-mission keeps the mission to continue later.
	if (IsInMission() && !IsMissionOver())
	{
		TArray<uint8> Data;
		FString Text;
		UBhGameInstance* GI = GetGameInstance<UBhGameInstance>();
		if (GI != nullptr && WriteSuspended(Data, Text))
		{
			GI->WriteSuspended(Data, Text);
		}
	}
	EndSession();
	if (AudioComponent != nullptr)
	{
		AudioComponent->Stop();
	}
	if (SynthWave != nullptr)
	{
		SynthWave->Mixer.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void ABhDirector::SetController(APlayerController* Controller)
{
	Projector.Controller = Controller;
}

// ============================================================================ Sessions

void ABhDirector::BeginSession(TUniquePtr<bh::Session> NewSession, EBhDirectorMode NewMode)
{
	EndSession();
	Session = MoveTemp(NewSession);
	Mode = NewMode;
	bEndHandled = false;
	bSummaryPending = false;
	BattleTimer = 0.f;
	ShakeAmount = 0.f;
	PendingNotices.Reset();
	RecentNotices.Reset();
	FloatTexts.Reset();
	Pings.clear();
	Hud = bh::HudModel();
	bTutorialMarker = false;
	WorldView.BuildWorld(*Session);

	// Minimap texture sized to this map.
	bh::PaintMinimap(Session->GetWorld().GetMap(), MinimapPixelsPerTile, MinimapBase);
	MinimapImage = MinimapBase;
	MinimapTexture = UBhAssets::CreateTexture(MinimapImage, true, true, TEXT("BhMinimap"));
	MinimapTimer = 0.f;
	MinimapBaseTimer = 0.f;
	if (Mode == EBhDirectorMode::Mission)
	{
		bh::BuildHudModel(*Session, Hud);
	}
	if (Mixer.IsValid())
	{
		Mixer->SetMusic(0);
	}
}

void ABhDirector::EndSession()
{
	WorldView.Clear();
	Fx.Clear();
	Session.Reset();
	Mode = EBhDirectorMode::None;
	// The UI may still draw the old minimap this frame: keep it alive until the next change.
	RetiredMinimap = MinimapTexture;
	MinimapTexture = nullptr;
}

void ABhDirector::ShowBackdrop()
{
	TUniquePtr<bh::Session> New = MakeUnique<bh::Session>();
	bh::SessionConfig Config;
	Config.MissionIndex = 0;
	Config.bTutorial = false;
	std::string Error;
	if (!New->Start(Config, Error))
	{
		UE_LOG(LogBeaconhold, Error, TEXT("Beaconhold: backdrop failed: %s"), *BhUE::ToFString(Error));
		return;
	}
	New->bPaused = true;
	BackdropTime = 0.f;
	BeginSession(MoveTemp(New), EBhDirectorMode::Backdrop);
}

bool ABhDirector::StartMission(int32 MissionIndex, bh::Difficulty Diff, bool bTutorial)
{
	TUniquePtr<bh::Session> New = MakeUnique<bh::Session>();
	bh::SessionConfig Config;
	Config.MissionIndex = MissionIndex;
	Config.Diff = Diff;
	Config.bTutorial = bTutorial;
	if (UBhGameInstance* GI = GetGameInstance<UBhGameInstance>())
	{
		for (int32 B = 0; B < bh::NumBoons; ++B)
		{
			Config.BoonRanks[B] = GI->GetProgress().BoonRanks[B];
		}
	}
	std::string Error;
	if (!New->Start(Config, Error))
	{
		UE_LOG(LogBeaconhold, Error, TEXT("Beaconhold: mission %d failed to start: %s"), MissionIndex, *BhUE::ToFString(Error));
		return false;
	}
	BeginSession(MoveTemp(New), EBhDirectorMode::Mission);
	return true;
}

bool ABhDirector::ResumeMission(const TArray<uint8>& Data)
{
	TUniquePtr<bh::Session> New = MakeUnique<bh::Session>();
	std::vector<uint8_t> Bytes(Data.GetData(), Data.GetData() + Data.Num());
	std::string Error;
	if (!bh::LoadSession(*New, Bytes, Error))
	{
		UE_LOG(LogBeaconhold, Warning, TEXT("Beaconhold: suspended mission could not be restored: %s"), *BhUE::ToFString(Error));
		return false;
	}
	BeginSession(MoveTemp(New), EBhDirectorMode::Mission);
	return true;
}

bool ABhDirector::IsInMission() const
{
	return Session.IsValid() && Mode == EBhDirectorMode::Mission;
}

bool ABhDirector::IsMissionOver() const
{
	return IsInMission() && Session->GetMission().Outcome != bh::MissionOutcome::InProgress;
}

int32 ABhDirector::GetMissionIndex() const
{
	return Session.IsValid() ? Session->GetConfig().MissionIndex : 0;
}

bh::Difficulty ABhDirector::GetDifficulty() const
{
	return Session.IsValid() ? Session->GetConfig().Diff : bh::Difficulty::Normal;
}

bool ABhDirector::GetTutorialEnabled() const
{
	return Session.IsValid() && Session->GetConfig().bTutorial;
}

bool ABhDirector::WriteSuspended(TArray<uint8>& OutData, FString& OutSummary) const
{
	if (!IsInMission() || IsMissionOver())
	{
		return false;
	}
	std::vector<uint8_t> Bytes;
	bh::SaveSession(*Session, Bytes);
	OutData.SetNumUninitialized(static_cast<int32>(Bytes.size()));
	if (!Bytes.empty())
	{
		FMemory::Memcpy(OutData.GetData(), Bytes.data(), Bytes.size());
	}
	const bh::MissionRuntime& M = Session->GetMission();
	OutSummary = FString::Printf(TEXT("%s - %s"), *BhUE::ToFString(M.Def().Title), *BhUE::TimeText(M.Elapsed).ToString());
	return OutData.Num() > 0;
}

bool ABhDirector::TakeSummary(bh::MissionSummary& Out)
{
	if (!bSummaryPending)
	{
		return false;
	}
	bSummaryPending = false;
	Out = Summary;
	return true;
}

// ============================================================================ Control & input

void ABhDirector::SetPaused(bool bPause)
{
	if (IsInMission())
	{
		Session->bPaused = bPause;
	}
}

bool ABhDirector::IsPaused() const
{
	return Session.IsValid() && Session->bPaused;
}

void ABhDirector::SetSpeed(float Speed)
{
	if (IsInMission())
	{
		Session->Speed = FMath::Clamp(Speed, 0.5f, 3.f);
	}
}

float ABhDirector::GetSpeed() const
{
	return Session.IsValid() ? Session->Speed : 1.f;
}

void ABhDirector::ExecuteAction(const bh::ActionId& Id)
{
	if (IsInMission() && !Session->bPaused)
	{
		Session->ExecuteAction(Id);
	}
}

void ABhDirector::SelectOnlyType(bh::Archetype Type)
{
	if (!IsInMission() || Session->bPaused)
	{
		return;
	}
	bh::PlayerControl& Control = Session->GetControl();
	std::vector<bh::EntityId> Ids;
	for (bh::EntityId Id : Control.Selection)
	{
		const bh::Entity* E = Session->GetWorld().Find(Id);
		if (E != nullptr && E->Type == Type)
		{
			Ids.push_back(Id);
		}
	}
	if (!Ids.empty())
	{
		Control.SelectMany(Session->GetWorld(), Ids);
	}
}

void ABhDirector::ContinueTutorial()
{
	if (IsInMission())
	{
		Session->ContinueTutorial();
	}
}

void ABhDirector::SkipTutorial()
{
	if (IsInMission())
	{
		Session->SkipTutorial();
	}
}

void ABhDirector::JumpCamera(const bh::Vec2& P)
{
	if (IsInMission())
	{
		Session->JumpCamera(P);
	}
}

void ABhDirector::PointerDown(int32 Id, float X, float Y, bool bMouse)
{
	if (IsInMission() && !Session->bPaused)
	{
		Session->PointerDown(Id, X, Y, bMouse);
	}
}

void ABhDirector::PointerMove(int32 Id, float X, float Y)
{
	if (IsInMission() && !Session->bPaused)
	{
		Session->PointerMove(Id, X, Y);
	}
}

void ABhDirector::PointerUp(int32 Id, float X, float Y)
{
	if (IsInMission())
	{
		Session->PointerUp(Id, X, Y);
	}
}

void ABhDirector::MouseCommand(float X, float Y)
{
	if (IsInMission() && !Session->bPaused)
	{
		Session->MouseCommand(X, Y, Projector);
	}
}

void ABhDirector::MouseWheel(float Delta)
{
	if (IsInMission())
	{
		Session->MouseWheel(Delta);
	}
}

void ABhDirector::PanByScreen(float Dx, float Dy)
{
	if (IsInMission())
	{
		Session->PanByScreen(Dx, Dy, Projector);
	}
}

void ABhDirector::KeyboardPan(float Right, float Up, float DeltaSeconds)
{
	if (!IsInMission() || (Right == 0.f && Up == 0.f))
	{
		return;
	}
	const float Speed = 0.9f * Session->GetCamera().Distance; // tiles per second
	Session->PanByWorld(bh::Vec2(Right, -Up) * (Speed * DeltaSeconds));
}

// ============================================================================ Per frame

void ABhDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Session.IsValid())
	{
		return;
	}
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	UpdateView();

	if (Mode == EBhDirectorMode::Mission)
	{
		Session->Update(Dt, &Projector);
		if (!Session->bPaused)
		{
			AnimTime += Dt * Session->Speed;
		}
	}
	else
	{
		// Menu backdrop: nothing simulates, the camera drifts and the flags keep waving.
		BackdropTime += Dt;
		AnimTime += Dt;
		bh::CameraRig& Rig = Session->GetCamera();
		const bh::Vec2 Keep = Session->GetKeepPos();
		Rig.Focus = Keep + bh::Vec2(3.5f * FMath::Sin(BackdropTime * 0.05f) + 2.f, 2.2f * FMath::Sin(BackdropTime * 0.037f) - 1.f);
		Rig.Distance = 17.f + 2.f * FMath::Sin(BackdropTime * 0.06f);
		Session->Update(Dt, nullptr);
	}

	Session->TakeEvents(Events);
	for (const bh::GameEvent& E : Events)
	{
		HandleEvent(E);
	}
	const float Alpha = Session->GetAlpha();
	const bool bFrozen = Session->bPaused && Mode == EBhDirectorMode::Mission;
	WorldView.Tick(*Session, Alpha, AnimTime, bFrozen ? 0.f : Dt, ViewTiles);
	Fx.SyncProjectiles(Session->GetWorld(), Alpha);
	Fx.Tick(bFrozen ? 0.f : Dt * Session->Speed);
	UpdateFloatTexts(bFrozen ? 0.f : Dt);
	UpdateCamera(Dt);

	if (Mode == EBhDirectorMode::Mission)
	{
		bh::BuildHudModel(*Session, Hud);
		bTutorialMarker = bh::FindTutorialMarker(*Session, TutorialPos, TutorialHeight);
		WorldView.SetTutorialMarker(bTutorialMarker, TutorialPos);
		CheckMissionEnd();
		UpdateMinimap(Dt, false);
		UpdateMusic(Dt);
	}
	if (Assets != nullptr)
	{
		Assets->FlushPalette();
	}
}

void ABhDirector::UpdateView()
{
	const float W = Projector.GetScreenWidth();
	const float H = Projector.GetScreenHeight();
	bViewValid = false;
	if (W <= 0.f || H <= 0.f)
	{
		return;
	}
	const float Xs[4] = {0.f, W, W, 0.f};
	const float Ys[4] = {0.f, 0.f, H, H};
	bool bAll = true;
	for (int32 I = 0; I < 4; ++I)
	{
		bAll = bAll && Projector.ScreenToGround(Xs[I], Ys[I], ViewCorners[I]);
	}
	if (!bAll)
	{
		return;
	}
	bViewValid = true;
	FVector2D Min(ViewCorners[0].X, ViewCorners[0].Y);
	FVector2D Max = Min;
	for (int32 I = 1; I < 4; ++I)
	{
		Min.X = FMath::Min<double>(Min.X, ViewCorners[I].X);
		Min.Y = FMath::Min<double>(Min.Y, ViewCorners[I].Y);
		Max.X = FMath::Max<double>(Max.X, ViewCorners[I].X);
		Max.Y = FMath::Max<double>(Max.Y, ViewCorners[I].Y);
	}
	ViewTiles = FBox2D(Min, Max);
}

bool ABhDirector::IsOnScreen(const bh::Vec2& P, float Margin) const
{
	return P.X >= ViewTiles.Min.X - Margin && P.X <= ViewTiles.Max.X + Margin && P.Y >= ViewTiles.Min.Y - Margin && P.Y <= ViewTiles.Max.Y + Margin;
}

void ABhDirector::UpdateCamera(float DeltaSeconds)
{
	if (Camera == nullptr || !Session.IsValid())
	{
		return;
	}
	const bh::CameraRig& Rig = Session->GetCamera();
	// Constant vertical view (the rig's FOV is horizontal at 16:9); wider screens see more.
	const float VerticalHalf = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Rig.Fov * 0.5f)) * (9.f / 16.f));
	float Aspect = 16.f / 9.f;
	const float W = Projector.GetScreenWidth();
	const float H = Projector.GetScreenHeight();
	if (W > 0.f && H > 0.f)
	{
		Aspect = W / H;
	}
	const float HorizontalFov = FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(VerticalHalf) * Aspect));
	Camera->SetFieldOfView(FMath::Clamp(HorizontalFov, 20.f, 120.f));

	const FRotator Rotation(-Rig.Pitch, Rig.Yaw, 0.f);
	FVector Location = BhUE::ToWorld(Rig.Focus) - Rotation.Vector() * (Rig.Distance * BhUE::TileSize);
	if (ShakeAmount > 0.f)
	{
		ShakeAmount = FMath::Max(0.f, ShakeAmount - DeltaSeconds * 2.5f);
		const float T = AnimTime * 47.f;
		Location += FVector(FMath::Sin(T * 1.3f), FMath::Cos(T * 1.7f), FMath::Sin(T * 2.1f)) * (ShakeAmount * 18.f);
	}
	Camera->SetWorldLocationAndRotation(Location, Rotation);
}

void ABhDirector::AddShake(float Amount, const bh::Vec2& Pos)
{
	if (IsOnScreen(Pos, 2.f))
	{
		ShakeAmount = FMath::Min(1.f, ShakeAmount + Amount);
	}
}

void ABhDirector::UpdateMinimap(float DeltaSeconds, bool bForce)
{
	MinimapTimer -= DeltaSeconds;
	MinimapBaseTimer -= DeltaSeconds;
	for (bh::MinimapPing& P : Pings)
	{
		P.Age += DeltaSeconds;
	}
	Pings.erase(std::remove_if(Pings.begin(), Pings.end(), [](const bh::MinimapPing& P) { return P.Age > bh::MinimapPingLife; }), Pings.end());
	if (!bForce && MinimapTimer > 0.f)
	{
		return;
	}
	MinimapTimer = 0.2f;
	if (MinimapBaseTimer <= 0.f)
	{
		// Felled trees change the terrain picture.
		MinimapBaseTimer = 2.f;
		bh::PaintMinimap(Session->GetWorld().GetMap(), MinimapPixelsPerTile, MinimapBase);
	}
	bh::PaintMinimapOverlay(*Session, MinimapBase, MinimapPixelsPerTile, ViewCorners, bViewValid, Pings, MinimapImage);
	UBhAssets::UpdateTexture(MinimapTexture, MinimapImage);
}

FIntPoint ABhDirector::GetMinimapSize() const
{
	return FIntPoint(MinimapImage.W, MinimapImage.H);
}

void ABhDirector::UpdateMusic(float DeltaSeconds)
{
	BattleTimer = FMath::Max(0.f, BattleTimer - DeltaSeconds);
	if (Mixer.IsValid())
	{
		Mixer->SetMusic(BattleTimer > 0.f ? 1 : 0);
	}
}

void ABhDirector::UpdateFloatTexts(float DeltaSeconds)
{
	for (int32 I = FloatTexts.Num() - 1; I >= 0; --I)
	{
		FBhFloatText& T = FloatTexts[I];
		T.Age += DeltaSeconds;
		T.Location.Z += 55.f * DeltaSeconds;
		if (T.Age >= T.Life)
		{
			FloatTexts.RemoveAtSwap(I);
		}
	}
}

void ABhDirector::CheckMissionEnd()
{
	if (bEndHandled || !IsMissionOver())
	{
		return;
	}
	bEndHandled = true;
	Summary = bh::BuildSummary(*Session);
	bSummaryPending = true;
	PlayCue(Summary.bWon ? bh::Sfx::Victory : bh::Sfx::Defeat);
	if (UBhGameInstance* GI = GetGameInstance<UBhGameInstance>())
	{
		if (Summary.bWon)
		{
			GI->RecordVictory(GetMissionIndex(), Summary.Stars, GetDifficulty());
		}
		GI->ClearSuspended();
	}
}

// ============================================================================ UI data

void ABhDirector::TakeNotices(TArray<FBhNoticeEntry>& Out)
{
	Out = MoveTemp(PendingNotices);
	PendingNotices.Reset();
}

void ABhDirector::PushNotice(const bh::Notice& N)
{
	const double Now = FPlatformTime::Seconds();
	FText Text = BhUE::ToText(N.Text);
	// The same message within a few seconds is noise (e.g. repeated "Not enough Sunstone").
	for (const FBhNoticeEntry& Recent : RecentNotices)
	{
		if (Recent.Text.EqualTo(Text) && Now - Recent.Time < 3.0)
		{
			return;
		}
	}
	FBhNoticeEntry Entry;
	Entry.Text = MoveTemp(Text);
	Entry.Severity = N.Severity;
	Entry.IconId = N.IconId;
	Entry.Time = Now;
	PendingNotices.Add(Entry);
	RecentNotices.Add(Entry);
	if (RecentNotices.Num() > 8)
	{
		RecentNotices.RemoveAt(0);
	}
}

bool ABhDirector::GetTutorialMarker(FVector& OutTop) const
{
	if (!bTutorialMarker || !IsInMission())
	{
		return false;
	}
	OutTop = BhUE::ToWorld(TutorialPos, TutorialHeight + 0.4f);
	return true;
}

void ABhDirector::GetBars(bool bShowAll, TArray<FBhBarInfo>& Out) const
{
	if (!IsInMission())
	{
		Out.Reset();
		return;
	}
	WorldView.GetBars(bShowAll, ViewTiles, Out);
}

bool ABhDirector::GetSelectionBox(FVector2D& OutMin, FVector2D& OutMax) const
{
	float X0 = 0.f;
	float Y0 = 0.f;
	float X1 = 0.f;
	float Y1 = 0.f;
	if (!IsInMission() || !Session->GetSelectionBox(X0, Y0, X1, Y1))
	{
		return false;
	}
	OutMin = FVector2D(FMath::Min(X0, X1), FMath::Min(Y0, Y1));
	OutMax = FVector2D(FMath::Max(X0, X1), FMath::Max(Y0, Y1));
	return true;
}

// ============================================================================ Audio

void ABhDirector::PlayCue(bh::Sfx Cue, float Volume)
{
	if (Mixer.IsValid())
	{
		Mixer->Play(Cue, Volume, 0.f);
	}
}

void ABhDirector::SetVolumes(float Music, float Effects)
{
	MusicVolume = Music;
	EffectsVolume = Effects;
	if (Mixer.IsValid())
	{
		Mixer->SetVolumes(1.f, Music, Effects);
	}
}

void ABhDirector::PlayWorldCue(bh::Sfx Cue, const bh::Vec2& Pos)
{
	if (!Mixer.IsValid() || !Session.IsValid())
	{
		return;
	}
	const bh::SfxInfo& Info = bh::GetSfxInfo(Cue);
	float Volume = 1.f;
	float Pan = 0.f;
	if (Info.bPositional)
	{
		const bh::CameraRig& Rig = Session->GetCamera();
		const float Reach = FMath::Max(8.f, Rig.Distance * 0.95f);
		const float Dist = bh::Vec2::Dist(Pos, Rig.Focus);
		Volume = FMath::Clamp(1.25f - Dist / Reach, 0.f, 1.f);
		if (Volume <= 0.05f)
		{
			return;
		}
		float Sx = 0.f;
		float Sy = 0.f;
		const float W = Projector.GetScreenWidth();
		if (W > 0.f && Projector.WorldToScreen(Pos, 0.5f, Sx, Sy))
		{
			Pan = FMath::Clamp((Sx / W) * 2.f - 1.f, -1.f, 1.f) * 0.7f;
		}
	}
	Mixer->Play(Cue, Volume, Pan);
}

// ============================================================================ Events

void ABhDirector::HandleEvent(const bh::GameEvent& E)
{
	if (Mode != EBhDirectorMode::Mission)
	{
		return;
	}
	bh::Sfx Cue = bh::Sfx::UiTap;
	if (bh::SfxForEvent(E, Cue))
	{
		PlayWorldCue(Cue, E.Pos);
	}
	bh::Notice N;
	if (bh::MakeNotice(E, *Session, N))
	{
		PushNotice(N);
	}
	const bool bVisible = IsOnScreen(E.Pos, 2.f);
	const FVector At = BhUE::ToWorld(E.Pos);
	switch (E.Type)
	{
	case bh::EventType::AttackStarted:
		WorldView.NotifySwing(E.A, AnimTime);
		if (E.Owner == bh::Team::Player || E.Owner == bh::Team::Enemy)
		{
			BattleTimer = FMath::Max(BattleTimer, 8.f);
		}
		break;
	case bh::EventType::Hit:
	{
		WorldView.NotifyHit(E.A, AnimTime);
		if (E.Owner == bh::Team::Player)
		{
			BattleTimer = 14.f;
		}
		if (bVisible)
		{
			const bh::Archetype Source = static_cast<bh::Archetype>(E.Sub);
			FBhBurst B;
			B.Kind = (Source == bh::Archetype::Hexer || Source == bh::Archetype::GloamHeart) ? bh::FxMesh::Gloom : bh::FxMesh::Spark;
			B.Count = E.Value > 20.f ? 7 : 4;
			B.Speed = 260.f;
			B.UpBias = 0.55f;
			B.Life = 0.35f;
			B.Size = 0.9f;
			Fx.Burst(At + FVector(0.f, 0.f, 45.f), B);
		}
		break;
	}
	case bh::EventType::Healed:
		if (bVisible)
		{
			FBhBurst B;
			B.Kind = bh::FxMesh::Heal;
			B.Count = 5;
			B.Speed = 90.f;
			B.UpBias = 1.f;
			B.Life = 0.9f;
			B.Gravity = -60.f;
			Fx.Burst(At + FVector(0.f, 0.f, 30.f), B);
		}
		break;
	case bh::EventType::GatherStrike:
		if (bVisible)
		{
			FBhBurst B;
			B.Kind = E.Sub == 0 ? bh::FxMesh::Shard : (E.Sub == 1 ? bh::FxMesh::Chip : bh::FxMesh::Dust);
			B.Count = 3;
			B.Speed = 200.f;
			B.UpBias = 0.7f;
			B.Life = 0.5f;
			Fx.Burst(At + FVector(0.f, 0.f, 30.f), B);
		}
		break;
	case bh::EventType::ResourceDelivered:
		if (E.Owner == bh::Team::Player && bVisible)
		{
			FBhFloatText& T = FloatTexts.AddDefaulted_GetRef();
			T.Text = FString::Printf(TEXT("+%d"), FMath::RoundToInt(E.Value));
			T.Location = BhUE::ToWorld(E.Pos, 1.3f);
			T.Color = DirResourceColor(E.Sub);
		}
		break;
	case bh::EventType::TreeFelled:
		if (bVisible)
		{
			FBhBurst B;
			B.Kind = bh::FxMesh::Chip;
			B.Count = 8;
			B.Speed = 240.f;
			B.Life = 0.8f;
			Fx.Burst(At + FVector(0.f, 0.f, 40.f), B);
			B.Kind = bh::FxMesh::Dust;
			B.Count = 4;
			B.Speed = 80.f;
			B.Gravity = -40.f;
			Fx.Burst(At, B);
		}
		break;
	case bh::EventType::NodeDepleted:
	case bh::EventType::ConstructionCancelled:
	case bh::EventType::BuildingPlaced:
	{
		if (E.Type != bh::EventType::NodeDepleted && E.Arch != bh::Archetype::None)
		{
			WorldView.NotifyFootprintChanged(DirFootprint(E.Arch, E.Pos));
		}
		if (bVisible)
		{
			FBhBurst B;
			B.Kind = bh::FxMesh::Dust;
			B.Count = 8;
			B.Speed = 160.f;
			B.UpBias = 0.3f;
			B.Life = 0.9f;
			B.Gravity = -30.f;
			B.Spread = 70.f;
			Fx.Burst(At, B);
		}
		break;
	}
	case bh::EventType::BuildingCompleted:
		if (bVisible)
		{
			FBhBurst B;
			B.Kind = bh::FxMesh::Dust;
			B.Count = 10;
			B.Speed = 170.f;
			B.UpBias = 0.3f;
			B.Life = 1.f;
			B.Gravity = -30.f;
			B.Spread = 90.f;
			Fx.Burst(At, B);
			B.Kind = bh::FxMesh::Spark;
			B.Count = 10;
			B.Speed = 300.f;
			B.UpBias = 0.9f;
			B.Life = 0.8f;
			B.Gravity = 500.f;
			Fx.Burst(At + FVector(0.f, 0.f, 120.f), B);
			if (E.Arch == bh::Archetype::Beacon)
			{
				Fx.GroundRing(At, 0.5f, 6.f, 1.4f, FLinearColor(1.f, 0.8f, 0.35f, 0.9f), bh::DecalTex::SoftGlow);
			}
		}
		break;
	case bh::EventType::EntityDied:
	{
		const bool bBuilding = E.Arch != bh::Archetype::None && bh::IsBuilding(E.Arch);
		if (bBuilding)
		{
			WorldView.NotifyFootprintChanged(DirFootprint(E.Arch, E.Pos));
			AddShake(0.35f, E.Pos);
		}
		if (bVisible)
		{
			FBhBurst B;
			const bool bGloam = E.Owner == bh::Team::Enemy;
			B.Kind = bBuilding ? bh::FxMesh::Smoke : (bGloam ? bh::FxMesh::Gloom : bh::FxMesh::Dust);
			B.Count = bBuilding ? 12 : 6;
			B.Speed = bBuilding ? 150.f : 110.f;
			B.UpBias = 0.8f;
			B.Life = bBuilding ? 1.6f : 0.8f;
			B.Gravity = -50.f;
			B.Spread = bBuilding ? 110.f : 25.f;
			Fx.Burst(At + FVector(0.f, 0.f, 20.f), B);
			if (bBuilding)
			{
				B.Kind = bh::FxMesh::Ember;
				B.Count = 8;
				B.Speed = 280.f;
				B.Gravity = 600.f;
				B.Life = 0.9f;
				Fx.Burst(At + FVector(0.f, 0.f, 80.f), B);
			}
		}
		break;
	}
	case bh::EventType::AbilityCast:
		if (bVisible)
		{
			const bh::Ability A = static_cast<bh::Ability>(E.Sub);
			if (A == bh::Ability::Sunburst)
			{
				Fx.GroundRing(At, 0.4f, bh::GetAbilityDef(A).Radius, 0.8f, FLinearColor(1.f, 0.85f, 0.4f, 0.95f), bh::DecalTex::RangeRing);
				FBhBurst B;
				B.Kind = bh::FxMesh::Spark;
				B.Count = 14;
				B.Speed = 420.f;
				B.UpBias = 0.35f;
				B.Life = 0.6f;
				Fx.Burst(At + FVector(0.f, 0.f, 60.f), B);
			}
			else
			{
				Fx.GroundRing(At, 0.3f, 2.2f, 0.6f, FLinearColor(0.6f, 0.8f, 1.f, 0.9f), bh::DecalTex::SelectionRing);
			}
		}
		break;
	case bh::EventType::UnderAttack:
	{
		bh::MinimapPing P;
		P.Pos = E.Pos;
		Pings.push_back(P);
		BattleTimer = 14.f;
		break;
	}
	case bh::EventType::WaveIncoming:
	case bh::EventType::WaveSpawned:
	{
		bh::MinimapPing P;
		P.Pos = E.Pos;
		Pings.push_back(P);
		if (E.Type == bh::EventType::WaveIncoming)
		{
			BattleTimer = FMath::Max(BattleTimer, 20.f);
		}
		break;
	}
	case bh::EventType::CommandMove:
	case bh::EventType::CommandAttack:
	case bh::EventType::CommandGather:
	case bh::EventType::CommandBuild:
		if (E.Owner == bh::Team::Player)
		{
			const bool bAttack = E.Type == bh::EventType::CommandAttack || (E.Type == bh::EventType::CommandMove && E.Sub == 1);
			const bool bWork = E.Type == bh::EventType::CommandGather || E.Type == bh::EventType::CommandBuild;
			const FLinearColor Color = bAttack ? FLinearColor(1.f, 0.35f, 0.3f, 0.95f) : (bWork ? FLinearColor(1.f, 0.85f, 0.35f, 0.95f) : FLinearColor(0.5f, 1.f, 0.55f, 0.95f));
			Fx.GroundRing(At, 0.9f, 0.25f, 0.45f, Color);
		}
		break;
	case bh::EventType::UnitTrained:
	case bh::EventType::ResearchCompleted:
		if (bVisible && E.Owner == bh::Team::Player)
		{
			FBhBurst B;
			B.Kind = bh::FxMesh::Spark;
			B.Count = 6;
			B.Speed = 150.f;
			B.UpBias = 1.f;
			B.Life = 0.7f;
			B.Gravity = 100.f;
			Fx.Burst(At + FVector(0.f, 0.f, 40.f), B);
		}
		break;
	default:
		break;
	}
}
