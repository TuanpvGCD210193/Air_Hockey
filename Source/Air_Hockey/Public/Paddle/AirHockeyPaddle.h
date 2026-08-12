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

	// STEP 17.1: Reliable Ordered RPC Queuing (Guaranteed Strict Packet Order)
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SendPaddlePosition(FVector ClampedPosition, FVector CalculatedVelocity);

	// STEP 17.1: Direct Server-to-Client Reliable RPC Relay
	UFUNCTION(Client, Reliable)
	void Client_ReceiveOpponentPaddlePosition(int32 SenderPlayerIndex, FVector Position, FVector Velocity);

	// STEP 18.1: 10ms High-Precision Fixed Sampler RPCs
	UFUNCTION(Server, Unreliable, WithValidation)
	void Server_Send10msPacket(FPaddle10msPacket Packet);

	UFUNCTION(Client, Unreliable)
	void Client_Receive10msPacket(FPaddle10msPacket Packet);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Materials")
	class UMaterialInterface* Player1Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Materials")
	class UMaterialInterface* Player2Material;

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

	// Unthrottled 0ms network tracking
	FVector LastSentPosition = FVector::ZeroVector;

	// STEP 8.1: Client Snapshot Buffer for Rollback & Curved Spline Interpolation
	struct FPaddleSnapshot
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float TimeStamp = 0.0f;

		FPaddleSnapshot() {}
		FPaddleSnapshot(const FVector& InPos, const FVector& InVel, float InTime)
			: Position(InPos), Velocity(InVel), TimeStamp(InTime) {}
	};

	TArray<FPaddleSnapshot> SnapshotBuffer;
	float SampleTimer = 0.0f;
	uint32 LocalSequenceCounter = 0;
	TArray<FPaddle10msSample> LocalSampleBuffer;

	// STEP 19.1: Adaptive Jitter Buffer & Playback Clock Variables
	float ClientPlaybackTime = 0.0f;
	float AdaptivePlaybackRate = 1.0f;
	bool bIsJitterBufferInitialized = false;
	float TargetJitterDelay = 0.120f;

	// STEP 14.2: Deterministic Rollback State Struct & Re-Simulation Engine
	struct FPaddleRollbackState
	{
		uint32 SequenceNumber = 0;
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float TimeStamp = 0.0f;

		FPaddleRollbackState() {}
		FPaddleRollbackState(uint32 InSeq, const FVector& InPos, const FVector& InVel, float InTime)
			: SequenceNumber(InSeq), Position(InPos), Velocity(InVel), TimeStamp(InTime) {}
	};

	void RollbackAndResimulate(const FPaddleRollbackState& AuthoritativeState);
};
