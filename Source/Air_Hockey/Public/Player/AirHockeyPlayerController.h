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
	virtual void ClientRestart_Implementation(APawn* NewPawn) override;

	FVector GetMouseWorldPositionOnTablePlane() const;

	void SetupTopDownCamera();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Input")
	float TableZHeight = 35.0f;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Camera")
	FVector CameraLocation = FVector(0.0f, 0.0f, 1900.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Camera") 
	FRotator CameraRotation = FRotator(-90.0f, 0.0f, 0.0f);

	UPROPERTY(Transient)
	class ACameraActor* TopDownCameraActor;
};
