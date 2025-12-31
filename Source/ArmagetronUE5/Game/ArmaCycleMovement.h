// ArmaCycleMovement.h - Port of gCycleMovement from src/tron/gCycleMovement.h
// Core lightcycle physics: speed, turning, rubber, braking

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/MovementComponent.h"
#include "Core/ArmaTypes.h"
#include "ArmaCycleMovement.generated.h"

// Forward declarations
class AArmaCycle;
class AArmaWall;
class UArmaGridSubsystem;

/**
 * FArmaDestination - Port of gDestination from original
 * A point on the map that the cycle should reach
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaDestination
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destination")
	FArmaCoord Position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destination")
	FArmaCoord Direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destination")
	float GameTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destination")
	float Distance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destination")
	float Speed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destination")
	bool bBraking;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destination")
	int32 Turns;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destination")
	bool bHasBeenUsed;

	FArmaDestination()
		: GameTime(0.0f), Distance(0.0f), Speed(0.0f), bBraking(false), Turns(0), bHasBeenUsed(false)
	{}
};

/**
 * FArmaEnemyInfluence - Port of gEnemyInfluence
 * Tracks enemies that influenced this cycle (for kill attribution)
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaEnemyInfluence
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence")
	TWeakObjectPtr<AArmaCycle> LastEnemy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence")
	float LastTime;

	FArmaEnemyInfluence() : LastTime(-1000.0f) {}

	void AddInfluence(AArmaCycle* Enemy, float Time, float TimePenalty);
	AArmaCycle* GetEnemy() const { return LastEnemy.Get(); }
	float GetTime() const { return LastTime; }
};

/**
 * UArmaCycleMovementComponent - Port of gCycleMovement
 * Handles all lightcycle physics and movement
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ARMAGETRONUE5_API UArmaCycleMovementComponent : public UMovementComponent
{
	GENERATED_BODY()

public:
	UArmaCycleMovementComponent();

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//////////////////////////////////////////////////////////////////////////
	// Speed System - Port from gCycleMovement
	//////////////////////////////////////////////////////////////////////////
	
	// Current speed
	UFUNCTION(BlueprintCallable, Category = "Cycle|Speed")
	float GetSpeed() const { return CurrentSpeed; }

	// Speed multiplier (global game setting)
	UFUNCTION(BlueprintCallable, Category = "Cycle|Speed")
	static float GetSpeedMultiplier() { return SpeedMultiplier; }

	UFUNCTION(BlueprintCallable, Category = "Cycle|Speed")
	static void SetSpeedMultiplier(float Mult) { SpeedMultiplier = Mult; }

	// Maximum speed a cycle can reach on its own
	UFUNCTION(BlueprintCallable, Category = "Cycle|Speed")
	static float GetMaximalSpeed();

	// Rubber speed (decay rate toward walls)
	UFUNCTION(BlueprintCallable, Category = "Cycle|Speed")
	static float GetRubberSpeed() { return RubberSpeed; }

	//////////////////////////////////////////////////////////////////////////
	// Direction and Turning - Port from gCycleMovement
	//////////////////////////////////////////////////////////////////////////

	// Current direction
	UFUNCTION(BlueprintCallable, Category = "Cycle|Direction")
	FArmaCoord GetDirection() const { return DirDrive; }

	// Last direction (before last turn)
	UFUNCTION(BlueprintCallable, Category = "Cycle|Direction")
	FArmaCoord GetLastDirection() const { return LastDirDrive; }

	// Winding number (grid direction index)
	UFUNCTION(BlueprintCallable, Category = "Cycle|Direction")
	int32 GetWindingNumber() const { return WindingNumber; }

	// Can we make a turn right now?
	UFUNCTION(BlueprintCallable, Category = "Cycle|Turn")
	bool CanMakeTurn(int32 Direction) const;

	// Can we make a turn at given time?
	UFUNCTION(BlueprintCallable, Category = "Cycle|Turn")
	bool CanMakeTurnAtTime(float Time, int32 Direction) const;

	// Get turn delay
	UFUNCTION(BlueprintCallable, Category = "Cycle|Turn")
	float GetTurnDelay() const;

	// Get turn delay for same direction
	UFUNCTION(BlueprintCallable, Category = "Cycle|Turn")
	float GetTurnDelayDb() const;

	// Get time of next possible turn
	UFUNCTION(BlueprintCallable, Category = "Cycle|Turn")
	float GetNextTurnTime(int32 Direction) const;

	// Execute a turn
	UFUNCTION(BlueprintCallable, Category = "Cycle|Turn")
	bool Turn(int32 Direction);

	// Distance since last turn
	UFUNCTION(BlueprintCallable, Category = "Cycle|Turn")
	float GetDistanceSinceLastTurn() const;

	//////////////////////////////////////////////////////////////////////////
	// Rubber System - Port from gCycleMovement
	//////////////////////////////////////////////////////////////////////////

	// Current rubber used
	UFUNCTION(BlueprintCallable, Category = "Cycle|Rubber")
	float GetRubber() const { return Rubber; }

	UFUNCTION(BlueprintCallable, Category = "Cycle|Rubber")
	void SetRubber(float InRubber) { Rubber = InRubber; }

	// Rubber malus (penalty factor)
	UFUNCTION(BlueprintCallable, Category = "Cycle|Rubber")
	float GetRubberMalus() const { return RubberMalus; }

	// Time rubber was depleted
	UFUNCTION(BlueprintCallable, Category = "Cycle|Rubber")
	float GetRubberDepleteTime() const { return RubberDepleteTime; }

	// Maximum space ahead (raycast to wall)
	UFUNCTION(BlueprintCallable, Category = "Cycle|Rubber")
	float GetMaxSpaceAhead(float MaxReport) const;

	//////////////////////////////////////////////////////////////////////////
	// Braking System - Port from gCycleMovement
	//////////////////////////////////////////////////////////////////////////

	// Is braking active?
	UFUNCTION(BlueprintCallable, Category = "Cycle|Brake")
	bool IsBraking() const { return bBraking; }

	// Set braking
	UFUNCTION(BlueprintCallable, Category = "Cycle|Brake")
	void SetBraking(bool bInBraking) { bBraking = bInBraking; }

	// Braking reservoir (1 = full, 0 = empty)
	UFUNCTION(BlueprintCallable, Category = "Cycle|Brake")
	float GetBrakingReservoir() const { return BrakingReservoir; }

	//////////////////////////////////////////////////////////////////////////
	// Distance and Stats
	//////////////////////////////////////////////////////////////////////////

	// Total distance traveled
	UFUNCTION(BlueprintCallable, Category = "Cycle|Stats")
	float GetDistance() const { return Distance; }

	// Number of turns taken
	UFUNCTION(BlueprintCallable, Category = "Cycle|Stats")
	int32 GetTurns() const { return TurnCount; }

	// Current acceleration
	UFUNCTION(BlueprintCallable, Category = "Cycle|Stats")
	float GetAcceleration() const { return Acceleration; }

	//////////////////////////////////////////////////////////////////////////
	// Alive/Death State
	//////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category = "Cycle|State")
	bool IsAlive() const { return AliveState > 0; }

	UFUNCTION(BlueprintCallable, Category = "Cycle|State")
	bool IsVulnerable() const;

	UFUNCTION(BlueprintCallable, Category = "Cycle|State")
	void Die(float Time);

	//////////////////////////////////////////////////////////////////////////
	// Wall Interaction
	//////////////////////////////////////////////////////////////////////////

	// Check if an edge/wall is dangerous to this cycle
	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	bool EdgeIsDangerous(AArmaWall* Wall, float Time, float Alpha) const;

	// Called when passing an edge
	void PassEdge(AArmaWall* Wall, float Time, float Alpha, int32 Recursion = 1);

	//////////////////////////////////////////////////////////////////////////
	// Destination System
	//////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category = "Cycle|Destination")
	void AddDestination();

	UFUNCTION(BlueprintCallable, Category = "Cycle|Destination")
	void AdvanceDestination();

	FArmaDestination* GetCurrentDestination() { return CurrentDestination; }

	//////////////////////////////////////////////////////////////////////////
	// Events
	//////////////////////////////////////////////////////////////////////////

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCycleTurn, int32, Direction);
	UPROPERTY(BlueprintAssignable, Category = "Cycle|Events")
	FOnCycleTurn OnTurn;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCycleDeath, float, Time);
	UPROPERTY(BlueprintAssignable, Category = "Cycle|Events")
	FOnCycleDeath OnDeath;

protected:
	//////////////////////////////////////////////////////////////////////////
	// Core Movement
	//////////////////////////////////////////////////////////////////////////

	// Main timestep function - port of TimestepCore
	virtual bool TimestepCore(float CurrentTime, bool bCalculateAcceleration = true);

	// Calculate acceleration to apply
	virtual void CalculateAcceleration();

	// Apply calculated acceleration
	virtual void ApplyAcceleration(float DeltaTime);

	// Called when acceleration makes a sharp jump
	virtual void AccelerationDiscontinuity();

	// Move safely without throwing exceptions
	void MoveSafely(const FArmaCoord& Dest, float StartTime, float EndTime);

	// Called right before death
	virtual void RightBeforeDeath(int32 NumTries);

	// Internal turn function
	virtual bool DoTurn(int32 Direction);

	// Internal distance calculation
	virtual float DoGetDistanceSinceLastTurn() const;

	//////////////////////////////////////////////////////////////////////////
	// State Variables - Port from gCycleMovement
	//////////////////////////////////////////////////////////////////////////

	// Alive state: 1 = alive, -1 = just died, 0 = dead
	UPROPERTY(BlueprintReadOnly, Category = "Cycle|State")
	int32 AliveState;

	// Enemy influence tracking
	UPROPERTY()
	FArmaEnemyInfluence EnemyInfluence;

	// Destination management
	UPROPERTY()
	TArray<FArmaDestination> DestinationList;

	FArmaDestination* CurrentDestination;
	FArmaDestination* LastDestination;

	// Direction
	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Direction")
	FArmaCoord DirDrive;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Direction")
	FArmaCoord LastDirDrive;

	// Physics state
	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Physics")
	float CurrentSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Physics")
	float VerletSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Physics")
	float Acceleration;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Physics")
	float LastTimestep;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Physics")
	float Distance;

	// Turn state
	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Turn")
	int32 TurnCount;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Turn")
	int32 WindingNumber;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Turn")
	int32 WindingNumberWrapped;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Turn")
	FArmaCoord LastTurnPos;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Turn")
	float LastTurnTimeRight;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Turn")
	float LastTurnTimeLeft;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Turn")
	float LastTimeAlive;

	// Pending turns queue
	UPROPERTY()
	TArray<int32> PendingTurns;

	// Braking state
	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Brake")
	bool bBraking;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Brake")
	float BrakingReservoir;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Brake")
	float BrakeUsage;

	// Rubber state
	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Rubber")
	float Rubber;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Rubber")
	float RubberMalus;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Rubber")
	float RubberSpeedFactor;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Rubber")
	float RubberDepleteTime;

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|Rubber")
	float RubberUsage;

	// Space ahead cache
	mutable bool bRefreshSpaceAhead;
	mutable float CachedMaxSpaceAhead;
	mutable float MaxSpaceMaxCast;

	// Gap detection for squeezing through walls
	mutable float Gap[2];
	mutable bool bKeepLookingForGap[2];
	mutable bool bGapIsBackdoor[2];

	//////////////////////////////////////////////////////////////////////////
	// Static Settings - Port from gCycleMovement
	//////////////////////////////////////////////////////////////////////////

	static float SpeedMultiplier;
	static float RubberSpeed;

	// Reference to grid subsystem
	UPROPERTY()
	UArmaGridSubsystem* GridSubsystem;

	// Owner cycle reference
	UPROPERTY()
	AArmaCycle* OwnerCycle;

	// Initialize internal state
	void InitializeMovement();
};

// Global turn speed factor (from original)
float GetTurnSpeedFactor();

