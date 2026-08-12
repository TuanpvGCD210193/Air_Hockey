#pragma once

#include "CoreMinimal.h"
#include "AirHockeyNetTypes.generated.h"

/**
 * Struct chứa thông tin di chuyển của Paddle gửi từ Client lên Server để Predict & Reconcile
 */
USTRUCT(BlueprintType)
struct AIR_HOCKEY_API FPaddleMove
{
	GENERATED_BODY()

	UPROPERTY()
	float TimeStamp = 0.0f;

	UPROPERTY()
	FVector TargetInputPosition = FVector::ZeroVector;

	UPROPERTY()
	FVector CalculatedVelocity = FVector::ZeroVector;

	UPROPERTY()
	uint32 SequenceNumber = 0;
};

/**
 * Struct chứa trạng thái chuẩn của Paddle do Server xác nhận (Authoritative State)
 */
USTRUCT(BlueprintType)
struct AIR_HOCKEY_API FPaddleState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY()
	uint32 LastProcessedSequenceNumber = 0;
};

/**
 * Struct chứa trạng thái của Puck (Vị trí & Vận tốc)
 */
USTRUCT(BlueprintType)
struct AIR_HOCKEY_API FPuckState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY()
	float TimeStamp = 0.0f;
};

/**
 * STEP 18.1: 10ms High-Precision Mouse Sampling & Redundant Net Packet
 */
USTRUCT(BlueprintType)
struct AIR_HOCKEY_API FPaddle10msSample
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY()
	uint32 SequenceID = 0;

	UPROPERTY()
	float TimeStamp = 0.0f;

	FPaddle10msSample() {}
	FPaddle10msSample(const FVector& InPos, const FVector& InVel, uint32 InSeq, float InTime)
		: Position(InPos), Velocity(InVel), SequenceID(InSeq), TimeStamp(InTime) {}
};

USTRUCT(BlueprintType)
struct AIR_HOCKEY_API FPaddle10msPacket
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PlayerIndex = 1;

	UPROPERTY()
	TArray<FPaddle10msSample> RedundantSamples;

	FPaddle10msPacket() {}
};
