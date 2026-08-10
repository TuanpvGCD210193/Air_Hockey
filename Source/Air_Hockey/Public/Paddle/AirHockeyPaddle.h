#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AirHockeyNetTypes.h"
#include "AirHockeyPaddle.generated.h"

UCLASS()
class AIR_HOCKEY_API AAirHockeyPaddle : public APawn
{
	GENERATED_BODY()

public:
	AAirHockeyPaddle();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FVector GetPaddleVelocity() const { return CurrentVelocity; }
	int32 GetPlayerIndex() const { return PlayerIndex; }
	void SetPlayerIndex(int32 InIndex) { PlayerIndex = InIndex; }

	// STEP 2.1: Fast Unreliable RPC for smooth position sync to Server
	UFUNCTION(Server, Unreliable, WithValidation)
	void Server_SendPaddlePosition(FVector ClampedPosition, FVector CalculatedVelocity);

	// Client-Side Prediction & Server Reconciliation RPCs
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SendMove(FPaddleMove Move);

	UFUNCTION(Client, Reliable)
	void Client_ReconcileState(FPaddleState AuthoritativeState);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirHockey|Components")
	class USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirHockey|Components")
	class UStaticMeshComponent* PaddleMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Movement")
	float MaxSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Movement")
	float SmoothingSpeed = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Collision")
	float PaddleRadius = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Bounds")
	float TableLength = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Bounds")
	float TableWidth = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Bounds")
	float TableZHeight = 35.0f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Player")
	int32 PlayerIndex = 1;

	UPROPERTY(ReplicatedUsing = OnRep_ServerState)
	FPaddleState ServerState;

	UFUNCTION()
	void OnRep_ServerState();

	void PerformMove(const FVector& TargetLocation, float DeltaTime, FVector& OutPosition, FVector& OutVelocity);

private:
	TArray<FPaddleMove> UnacknowledgedMoves;
	uint32 CurrentSequenceNumber = 0;
	FVector CurrentVelocity = FVector::ZeroVector;
	FVector LastValidTargetInput = FVector::ZeroVector;
};
