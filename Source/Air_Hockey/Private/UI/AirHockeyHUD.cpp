#include "UI/AirHockeyHUD.h"
#include "AirHockeyGameState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

AAirHockeyHUD::AAirHockeyHUD()
{
}

void AAirHockeyHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas) return;

	AAirHockeyGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirHockeyGameState>() : nullptr;
	if (!GS) return;

	float ScreenWidth = Canvas->ClipX;
	float ScreenHeight = Canvas->ClipY;

	// STEP 5.2: Top Header Scoreboard Bar (Dark Translucent Box)
	float BarWidth = 520.0f;
	float BarHeight = 60.0f;
	float BarX = (ScreenWidth - BarWidth) / 2.0f;
	float BarY = 20.0f;

	DrawRect(FLinearColor(0.02f, 0.02f, 0.05f, 0.75f), BarX, BarY, BarWidth, BarHeight);
	DrawRect(FLinearColor(1.0f, 0.2f, 0.2f, 1.0f), BarX, BarY + BarHeight - 3.0f, BarWidth / 2.0f, 3.0f);
	DrawRect(FLinearColor(0.2f, 0.6f, 1.0f, 1.0f), BarX + BarWidth / 2.0f, BarY + BarHeight - 3.0f, BarWidth / 2.0f, 3.0f);

	UFont* RenderFont = GEngine ? GEngine->GetMediumFont() : nullptr;

	// Draw Player 1 (Red) Name
	FString P1Name = TEXT("PLAYER 1");
	DrawText(P1Name, FLinearColor(1.0f, 0.3f, 0.3f, 1.0f), BarX + 25.0f, BarY + 18.0f, RenderFont, 1.2f);

	// Draw Center Replicated Score Text: "Player1Score  -  Player2Score"
	FString ScoreText = FString::Printf(TEXT("%d   -   %d"), GS->Player1Score, GS->Player2Score);
	DrawText(ScoreText, FLinearColor::Yellow, BarX + 215.0f, BarY + 16.0f, RenderFont, 1.4f);

	// Draw Player 2 (Blue) Name
	FString P2Name = TEXT("PLAYER 2");
	DrawText(P2Name, FLinearColor(0.3f, 0.7f, 1.0f, 1.0f), BarX + 380.0f, BarY + 18.0f, RenderFont, 1.2f);

	// STEP 5.2: Victory Banner Overlay when Match Ends (10 Points Reached)
	if (GS->bIsGameOver)
	{
		float BannerW = 600.0f;
		float BannerH = 100.0f;
		float BannerX = (ScreenWidth - BannerW) / 2.0f;
		float BannerY = (ScreenHeight - BannerH) / 2.0f;

		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f), BannerX, BannerY, BannerW, BannerH);
		DrawRect(FLinearColor::Yellow, BannerX, BannerY, BannerW, 4.0f);
		DrawRect(FLinearColor::Yellow, BannerX, BannerY + BannerH - 4.0f, BannerW, 4.0f);

		FString WinText = FString::Printf(TEXT("PLAYER %d WINS THE MATCH!"), GS->WinningPlayerId);
		DrawText(WinText, FLinearColor::Yellow, BannerX + 110.0f, BannerY + 32.0f, RenderFont, 1.6f);
	}
}
