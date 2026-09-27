// Beaconhold - world-space overlays drawn on the canvas under the Slate UI: health and
// construction bars, floating resource numbers, the tutorial pointer and the selection box.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "BhHUD.generated.h"

class ABhDirector;

UCLASS()
class ABhHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	ABhDirector* FindDirector() const;
	void DrawBars(ABhDirector& D, float Scale);
	void DrawFloatTexts(ABhDirector& D, float Scale);
	void DrawTutorialPointer(ABhDirector& D, float Scale);
	void DrawSelectionBox(ABhDirector& D, float Scale);
};
