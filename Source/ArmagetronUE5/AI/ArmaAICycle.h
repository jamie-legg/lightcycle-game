// ArmaAICycle.h - AI-controlled lightcycle that respawns on death

#pragma once

#include "CoreMinimal.h"
#include "Game/ArmaCyclePawn.h"
#include "Core/ArmaTypes.h"
#include "ArmaAICycle.generated.h"

// Use EArmaAIState from ArmaTypes.h

/**
 * Sensor data structure - based on Armagetron's gAISensor
 */
USTRUCT(BlueprintType)
struct FArmaAISensorData
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	float Distance = 9999.0f;
	
	UPROPERTY(BlueprintReadOnly)
	bool bHit = false;
	
	UPROPERTY(BlueprintReadOnly)
	FVector HitPoint = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadOnly)
	bool bIsOwnWall = false;
	
	UPROPERTY(BlueprintReadOnly)
	bool bIsRim = false;
};

/**
 * AArmaAICycle - AI-controlled cycle based on Armagetron's gAIPlayer
 */
UCLASS()
class ARMAGETRONUE5_API AArmaAICycle : public AArmaCyclePawn
{
	GENERATED_BODY()
	
public:
	AArmaAICycle();
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	
	// ========== AI Settings ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float AIThinkInterval = 0.1f;  // How often to make decisions (sg_delayCycle)
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float SensorRange = 500.0f;  // How far to look
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float EmergencyDistance = 100.0f;  // Distance to trigger emergency turn
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float ReactionTime = 0.15f;  // Delay before reacting (AI_REACTION)
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float RespawnDelay = 2.0f;  // Time before respawning after death
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 AIIQ = 100;  // Intelligence (0-100), affects reaction time
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FLinearColor AIColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);  // Bright Red (same as Armagetron enemy)
	
	// ========== AI State ==========
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	EArmaAIState CurrentState = EArmaAIState::Survive;
	
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	FArmaAISensorData FrontSensor;
	
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	FArmaAISensorData LeftSensor;
	
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	FArmaAISensorData RightSensor;
	
protected:
	// ========== AI Logic ==========
	
	// Main think function - called periodically
	void Think();
	
	// State-specific think functions (from Armagetron)
	void ThinkSurvive();
	void ThinkTrace();
	
	// Emergency survival - turn away from imminent collision
	bool EmergencySurvive(int PreferredDirection = 0);
	
	// Cast a sensor in a direction
	FArmaAISensorData CastSensor(FVector Direction, float Range);
	
	// Update all three sensors
	void UpdateSensors();
	
	// Execute a turn decision
	void ExecuteTurn(int Direction);  // -1 = left, 1 = right, 0 = straight
	
	// Respawn the AI
	void AIRespawn();
	
	// ========== AI State Tracking ==========
	float LastThinkTime = 0.0f;
	float NextThinkTime = 0.0f;
	float DeathTime = 0.0f;
	bool bWaitingToRespawn = false;
	
	// Trace state
	int TraceSide = 1;  // Which side to trace (-1 left, 1 right)
	
	// Decision tracking
	int PendingTurn = 0;
	float TurnDecisionTime = 0.0f;
};

