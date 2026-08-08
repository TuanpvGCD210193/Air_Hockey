#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AirHockeyNetTypes.h"
#include "AirHockeyPuck.generated.h"

UCLASS()
class AIR_HOCKEY_API AAirHockeyPuck : public AActor
{
	GENERATED_BODY()

public:
	AAirHockeyPuck();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void LaunchPuck(const FVector& InitialVelocity);
	void ResetPuck(const FVector& NewLocation, const FVector& InitialVel);
	FVector GetPuckVelocity() const { return CurrentVelocity; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirHockey|Components")
	class USceneComponent* RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirHockey|Components")
	class UStaticMeshComponent* PuckMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Physics")
	float PuckRadius = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Bounds")
	float TableLength = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Bounds")
	float TableWidth = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Bounds")
	float GoalWidth = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Bounds")
	float TableZHeight = 35.0f;

	UPROPERTY(ReplicatedUsing = OnRep_PuckState)
	FPuckState ServerPuckState;

	UFUNCTION()
	void OnRep_PuckState();

private:
	FVector CurrentVelocity = FVector::ZeroVector;

	void UpdatePuckPhysics(float DeltaTime);
	void HandleWallBounce(const FVector& SurfaceNormal);
	void HandlePaddleHit(AActor* PaddleActor, const FVector& PaddleVelocity);
};
