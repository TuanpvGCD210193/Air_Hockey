#include "UI/AirHockeyHUD.h"
#include "AirHockeyGameState.h"

AAirHockeyHUD::AAirHockeyHUD()
{
}

void AAirHockeyHUD::DrawHUD()
{
	Super::DrawHUD();

	AAirHockeyGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirHockeyGameState>() : nullptr;
	if (GS)
	{
		static bool bLogged = false;
		if (!bLogged)
		{
			UE_LOG(LogTemp, Warning, TEXT("[STEP 5.1 DEBUG] AirHockeyHUD Active & Bound to GameState!"));
			bLogged = true;
		}
	}
}
