#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AirHockeyPlayerController.generated.h"

UCLASS()
class AIR_HOCKEY_API AAirHockeyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAirHockeyPlayerController();

	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	FVector GetMouseWorldPositionOnTablePlane() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Input")
	float TableZHeight = 0.0f;
};
