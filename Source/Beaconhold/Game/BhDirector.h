// Beaconhold — the director: runs a simulation session and presents it (world view, effects,
// camera, audio, minimap, notices). One director lives in the map for the whole app session;
// the menu backdrop and every mission are sessions it starts and ends.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Game/BhFx.h"
#include "Game/BhWorldView.h"

#include "BhHud.h"
#include "BhRender.h"
#include "BhSession.h"
#include "BhSynth.h"

#include <vector>

#include "BhDirector.generated.h"

class APlayerController;
class FBhMixer;
class UAudioComponent;
class UBhAssets;
class UBhSynthWave;
class UCameraComponent;
class USceneComponent;
class UTexture2D;

// Screen <-> ground conversions for the simulation, through the player's camera.
class FBhProjector : public bh::IViewProjector
{
public:
	TWeakObjectPtr<APlayerController> Controller;

	virtual bool ScreenToGround(float X, float Y, bh::Vec2& Out) const override;
	virtual bool WorldToScreen(const bh::Vec2& P, float Height, float& OutX, float& OutY) const override;
	virtual float GetScreenWidth() const override;
	virtual float GetScreenHeight() const override;
};

struct FBhNoticeEntry
{
	FText Text;
	bh::NoticeSeverity Severity = bh::NoticeSeverity::Info;
	bh::Icon IconId = bh::Icon::None;
	double Time = 0.0;
};

struct FBhFloatText
{
	FString Text;
	FVector Location = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Age = 0.f;
	float Life = 1.2f;
};

enum class EBhDirectorMode : uint8
{
	None,
	Backdrop,
	Mission,
};

UCLASS()
class ABhDirector : public AActor
{
	GENERATED_BODY()

public:
	ABhDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	void SetController(APlayerController* Controller);
	UBhAssets* GetAssets() const { return Assets; }

	// ------------------------------------------------------------------ Flow
	// The menu background: the first map, frozen, with a slowly drifting camera.
	void ShowBackdrop();
	bool StartMission(int32 MissionIndex, bh::Difficulty Diff, bool bTutorial);
	bool ResumeMission(const TArray<uint8>& Data);
	bool IsInMission() const;
	bool IsMissionOver() const;
	int32 GetMissionIndex() const;
	bh::Difficulty GetDifficulty() const;
	bool GetTutorialEnabled() const;
	// Serializes the running mission (for suspend / continue later).
	bool WriteSuspended(TArray<uint8>& OutData, FString& OutSummary) const;
	// The results of a mission that just ended (returned once).
	bool TakeSummary(bh::MissionSummary& Out);

	// ------------------------------------------------------------------ Mission control
	void SetPaused(bool bPause);
	bool IsPaused() const;
	void SetSpeed(float Speed);
	float GetSpeed() const;
	void ExecuteAction(const bh::ActionId& Id);
	// Narrows a multi-selection to the units of one type (tapping a group in the HUD).
	void SelectOnlyType(bh::Archetype Type);
	void ContinueTutorial();
	void SkipTutorial();
	void JumpCamera(const bh::Vec2& P);

	// ------------------------------------------------------------------ Input (viewport pixels)
	void PointerDown(int32 Id, float X, float Y, bool bMouse);
	void PointerMove(int32 Id, float X, float Y);
	void PointerUp(int32 Id, float X, float Y);
	void MouseCommand(float X, float Y);
	void MouseWheel(float Delta);
	// Drags the map by a screen-space offset (right mouse button).
	void PanByScreen(float Dx, float Dy);
	void KeyboardPan(float Right, float Up, float DeltaSeconds);

	// ------------------------------------------------------------------ UI data
	const bh::HudModel& GetHud() const { return Hud; }
	const bh::Session* GetSession() const { return Session.Get(); }
	void TakeNotices(TArray<FBhNoticeEntry>& Out);
	UTexture2D* GetMinimapTexture() const { return MinimapTexture; }
	FIntPoint GetMinimapSize() const;
	const TArray<FBhFloatText>& GetFloatTexts() const { return FloatTexts; }
	bool GetTutorialMarker(FVector& OutTop) const;
	void GetBars(bool bShowAll, TArray<FBhBarInfo>& Out) const;
	bool GetSelectionBox(FVector2D& OutMin, FVector2D& OutMax) const;

	// ------------------------------------------------------------------ Audio
	void PlayCue(bh::Sfx Cue, float Volume = 1.f);
	void SetVolumes(float Music, float Effects);

private:
	void BeginSession(TUniquePtr<bh::Session> NewSession, EBhDirectorMode NewMode);
	void EndSession();
	void HandleEvent(const bh::GameEvent& E);
	void PushNotice(const bh::Notice& N);
	void UpdateView();
	void UpdateCamera(float DeltaSeconds);
	void UpdateMinimap(float DeltaSeconds, bool bForce);
	void UpdateMusic(float DeltaSeconds);
	void UpdateFloatTexts(float DeltaSeconds);
	void CheckMissionEnd();
	void PlayWorldCue(bh::Sfx Cue, const bh::Vec2& Pos);
	bool IsOnScreen(const bh::Vec2& P, float Margin) const;
	void AddShake(float Amount, const bh::Vec2& Pos);

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<UAudioComponent> AudioComponent;

	UPROPERTY()
	TObjectPtr<UBhSynthWave> SynthWave;

	UPROPERTY()
	TObjectPtr<UTexture2D> MinimapTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> RetiredMinimap;

	UPROPERTY()
	TObjectPtr<UBhAssets> Assets;

	TUniquePtr<bh::Session> Session;
	EBhDirectorMode Mode = EBhDirectorMode::None;
	FBhProjector Projector;
	FBhWorldView WorldView;
	FBhFx Fx;
	TSharedPtr<FBhMixer, ESPMode::ThreadSafe> Mixer;

	bh::HudModel Hud;
	std::vector<bh::GameEvent> Events;
	TArray<FBhNoticeEntry> PendingNotices;
	TArray<FBhNoticeEntry> RecentNotices;
	TArray<FBhFloatText> FloatTexts;
	std::vector<bh::MinimapPing> Pings;
	bh::ImageRGBA MinimapBase;
	bh::ImageRGBA MinimapImage;
	float MinimapTimer = 0.f;
	float MinimapBaseTimer = 0.f;

	bh::Vec2 ViewCorners[4];
	bool bViewValid = false;
	FBox2D ViewTiles = FBox2D(FVector2D(0.0, 0.0), FVector2D(64.0, 64.0));

	float AnimTime = 0.f;
	float BackdropTime = 0.f;
	float ShakeAmount = 0.f;
	float BattleTimer = 0.f;
	float MusicVolume = 0.6f;
	float EffectsVolume = 0.9f;
	bool bEndHandled = false;
	bool bSummaryPending = false;
	bh::MissionSummary Summary;
	bool bTutorialMarker = false;
	bh::Vec2 TutorialPos;
	float TutorialHeight = 0.f;
};
