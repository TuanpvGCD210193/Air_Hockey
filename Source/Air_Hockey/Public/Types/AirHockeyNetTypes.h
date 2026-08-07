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
