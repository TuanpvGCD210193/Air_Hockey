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
	TArray<FPuck10msSample> GetLocalPuckSampleBuffer() const { return LocalPuckSampleBuffer; }

	void HandleWallBounce(const FVector& SurfaceNormal);
	void HandlePaddleHit(AActor* PaddleActor, const FVector& PaddleVelocity);

	// STEP 25.1: Server State Reconstruction & Lag Compensated Hit Registration Engine
	void HandlePaddleHitLagCompensated(AActor* PaddleActor, const FVector& PaddleVelocity, float HitAge);

	// STEP 27.1: Pure Client-Side Prediction & Server Consensus Engine (Rocket League Architecture)
	void StartClientPrediction();
	bool IsClientPredicting() const { return bIsClientPredictingPuck; }

	// STEP 23.1: 10ms High-Precision Puck RPC
	UFUNCTION(NetMulticast, Unreliable)
	void Client_ReceivePuck10msPacket(FPuck10msPacket Packet);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirHockey|Components")
	class USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirHockey|Components")
	class UStaticMeshComponent* PuckMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Physics")
	float PuckRadius = 40.0f;

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

public:
	// STEP 15.1: Server World History Snapshot Struct for State Reconstruction
	struct FPuckWorldSnapshot
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float TimeStamp = 0.0f;

		FPuckWorldSnapshot() {}
		FPuckWorldSnapshot(const FVector& InPos, const FVector& InVel, float InTime)
			: Position(InPos), Velocity(InVel), TimeStamp(InTime) {}
	};

	TArray<FPuckWorldSnapshot> WorldHistoryBuffer;

	// STEP 23.1: Client 10ms Puck Jitter Buffer & Adaptive Playback Variables
	TArray<FPuckWorldSnapshot> PuckSnapshotBuffer;
	float PuckSampleTimer = 0.0f;
	TArray<FPuck10msSample> LocalPuckSampleBuffer;
	float PuckClientPlaybackTime = 0.0f;
	float PuckAdaptivePlaybackRate = 1.0f;
	bool bIsPuckJitterBufferInitialized = false;
	float TargetPuckJitterDelay = 0.120f;
	// STEP 27.1: Pure Client-Side Prediction Variables
	bool bIsClientPredictingPuck = false;
	float ClientPredictionTimer = 0.0f;
	float MaxClientPredictionDuration = 1.5f;
};
