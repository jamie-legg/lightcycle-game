// ArmaAIController.h - Port of gAIBase/gAIPlayer from src/tron/gAIBase.h
// AI state machine and decision making for lightcycle opponents

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Core/ArmaTypes.h"
#include "ArmaAIController.generated.h"

// Forward declarations
class AArmaCycle;
class AArmaWall;
class UArmaGridSubsystem;

/**
 * FArmaAISensor - Port of gAISensor/gSensor
 * Raycasting sensor for obstacle detection
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaAISensor
{
	GENERATED_BODY()

	// Ray origin
	UPROPERTY(BlueprintReadOnly, Category = "AI Sensor")
	FArmaCoord Origin;

	// Ray direction
	UPROPERTY(BlueprintReadOnly, Category = "AI Sensor")
	FArmaCoord Direction;

	// Distance to hit
	UPROPERTY(BlueprintReadOnly, Category = "AI Sensor")
	float Distance;

	// Hit point
	UPROPERTY(BlueprintReadOnly, Category = "AI Sensor")
	FArmaCoord HitPoint;

	// What we hit
	UPROPERTY(BlueprintReadOnly, Category = "AI Sensor")
	TWeakObjectPtr<AArmaWall> HitWall;

	UPROPERTY(BlueprintReadOnly, Category = "AI Sensor")
	TWeakObjectPtr<AArmaCycle> HitCycle;

	// Was it our own wall?
	UPROPERTY(BlueprintReadOnly, Category = "AI Sensor")
	bool bHitOwnWall;

	// Danger factor (0 = safe, 1 = deadly)
	UPROPERTY(BlueprintReadOnly, Category = "AI Sensor")
	float Danger;

	FArmaAISensor()
		: Distance(FLT_MAX), bHitOwnWall(false), Danger(0.0f)
	{}

	// Cast the sensor ray
	void PerformCast(UWorld* World, AArmaCycle* OwnerCycle, const FArmaCoord& InOrigin, const FArmaCoord& InDir, float MaxDistance);
};

/**
 * FArmaAIThinkData - Port of ThinkData struct
 * Data passed between thinking functions
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaAIThinkData
{
	GENERATED_BODY()

	// Direction to turn (-1 = right, 0 = none, 1 = left)
	UPROPERTY(BlueprintReadWrite, Category = "AI")
	int32 Turn;

	// When to think again
	UPROPERTY(BlueprintReadWrite, Category = "AI")
	float ThinkAgain;

	// Sensor data
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	FArmaAISensor Front;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	FArmaAISensor Left;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	FArmaAISensor Right;

	FArmaAIThinkData()
		: Turn(0), ThinkAgain(0.0f)
	{}
};

/**
 * AArmaAIController - Port of gAIPlayer
 * AI controller for lightcycle opponents
 */
UCLASS()
class ARMAGETRONUE5_API AArmaAIController : public AAIController
{
	GENERATED_BODY()

public:
	AArmaAIController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	//////////////////////////////////////////////////////////////////////////
	// AI State Machine - Port from gAIPlayer
	//////////////////////////////////////////////////////////////////////////

	// Current AI state
	UPROPERTY(BlueprintReadOnly, Category = "AI|State")
	EArmaAIState CurrentState;

	// Switch to a new state
	UFUNCTION(BlueprintCallable, Category = "AI|State")
	void SwitchToState(EArmaAIState NewState, float MinTime = 10.0f);

	// Get current target
	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	AArmaCycle* GetTarget() const { return Target.Get(); }

	// Set target
	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	void SetTarget(AArmaCycle* NewTarget) { Target = NewTarget; }

	// Clear target
	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	void ClearTarget() { Target = nullptr; }

	//////////////////////////////////////////////////////////////////////////
	// AI Character Settings
	//////////////////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Character")
	FArmaAICharacter AICharacterSettings;

	//////////////////////////////////////////////////////////////////////////
	// Route System
	//////////////////////////////////////////////////////////////////////////

	// Add a waypoint to follow
	UFUNCTION(BlueprintCallable, Category = "AI|Route")
	void AddWaypoint(const FArmaCoord& Point);

	// Set complete route
	UFUNCTION(BlueprintCallable, Category = "AI|Route")
	void SetRoute(const TArray<FArmaCoord>& Route);

	// Clear route
	UFUNCTION(BlueprintCallable, Category = "AI|Route")
	void ClearRoute();

	//////////////////////////////////////////////////////////////////////////
	// Configuration
	//////////////////////////////////////////////////////////////////////////

	// Set number of AIs and their IQ
	UFUNCTION(BlueprintCallable, Category = "AI|Config")
	static void SetNumberOfAIs(int32 Num, int32 MinPlayers, int32 IQ, int32 Tries = -2);

	// Configure AIs (called from menu)
	UFUNCTION(BlueprintCallable, Category = "AI|Config")
	static void ConfigureAIs();

	//////////////////////////////////////////////////////////////////////////
	// Static Callbacks - For cycle interaction events
	//////////////////////////////////////////////////////////////////////////

	// Called when cycle A drives close to wall of cycle B
	static void CycleBlocksWay(AArmaCycle* A, AArmaCycle* B, int32 ADir, int32 BDir, float BDist, int32 Winding);

	// Called when cycle blocks rim wall
	static void CycleBlocksRim(AArmaCycle* A, int32 ADir);

	// Called when a hole is created in a wall
	static void BreakWall(AArmaCycle* A, float ADist);

protected:
	//////////////////////////////////////////////////////////////////////////
	// Think Functions - Port from gAIPlayer
	//////////////////////////////////////////////////////////////////////////

	// Main think function - returns time until next think
	UFUNCTION(BlueprintCallable, Category = "AI|Think")
	float Think();

	// State-specific thinking
	virtual void ThinkSurvive(FArmaAIThinkData& Data);
	virtual void ThinkTrace(FArmaAIThinkData& Data);
	virtual void ThinkPath(FArmaAIThinkData& Data);
	virtual void ThinkCloseCombat(FArmaAIThinkData& Data);
	virtual void ThinkRoute(FArmaAIThinkData& Data);

	// Emergency handlers
	virtual bool EmergencySurvive(FArmaAIThinkData& Data, int32 EnemyEvade = -1, int32 PreferredSide = 0);
	virtual void EmergencyTrace(FArmaAIThinkData& Data);
	virtual void EmergencyPath(FArmaAIThinkData& Data);
	virtual void EmergencyCloseCombat(FArmaAIThinkData& Data);
	virtual void EmergencyRoute(FArmaAIThinkData& Data);

	// Act on gathered data
	virtual void ActOnData(FArmaAIThinkData& Data);

	//////////////////////////////////////////////////////////////////////////
	// Helpers
	//////////////////////////////////////////////////////////////////////////

	// Set trace side for wall tracing
	void SetTraceSide(int32 Side);

	// Cast sensors in all directions
	void CastSensors(FArmaAIThinkData& Data);

	// Find best turn direction
	int32 FindBestTurn(const FArmaAIThinkData& Data) const;

	// Check if a turn is safe
	bool IsTurnSafe(int32 Direction, float LookAhead) const;

	// Calculate distance to nearest enemy
	float GetDistanceToNearestEnemy() const;

	// Get controlled cycle
	AArmaCycle* GetCycle() const;

	//////////////////////////////////////////////////////////////////////////
	// State Variables
	//////////////////////////////////////////////////////////////////////////

	// Target for offensive modes
	UPROPERTY()
	TWeakObjectPtr<AArmaCycle> Target;

	// For pathfinding mode
	UPROPERTY()
	TArray<FArmaCoord> Path;

	UPROPERTY()
	float LastPathTime;

	// For route mode
	UPROPERTY()
	TArray<FArmaCoord> RoutePoints;

	UPROPERTY()
	int32 CurrentRouteIndex;

	// For trace mode
	UPROPERTY()
	int32 TraceSide;

	UPROPERTY()
	float LastChangeAttempt;

	UPROPERTY()
	float LazySideChange;

	// State timing
	UPROPERTY()
	float NextStateChange;

	// Emergency state
	UPROPERTY()
	bool bEmergency;

	UPROPERTY()
	int32 TriesLeft;

	// Free side indicator
	UPROPERTY()
	float FreeSide;

	// Thinking timing
	UPROPERTY()
	float LastThinkTime;

	UPROPERTY()
	float NextThinkTime;

	UPROPERTY()
	float Concentration;

	// Grid reference
	UPROPERTY()
	UArmaGridSubsystem* GridSubsystem;

	// Random number generator
	FRandomStream RandomStream;
};

