#include "AirHockeyPlayerController.h"

AAirHockeyPlayerController::AAirHockeyPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AAirHockeyPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
}

void AAirHockeyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

FVector AAirHockeyPlayerController::GetMouseWorldPositionOnTablePlane() const
{
	FVector WorldLocation, WorldDirection;
	if (DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
	{
		if (!FMath::IsNearlyZero(WorldDirection.Z))
		{
			float T = (TableZHeight - WorldLocation.Z) / WorldDirection.Z;
			return WorldLocation + WorldDirection * T;
		}
	}
	return FVector::ZeroVector;
}
