// ArmaCycle.h - Port of gCycle from src/tron/gCycle.h
// Complete lightcycle actor with rendering, wall building, and collision

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Core/ArmaTypes.h"
#include "ArmaCycle.generated.h"

// Forward declarations
class UArmaCycleMovementComponent;
class AArmaWall;
class AArmaPlayerController;
class UStaticMeshComponent;
class UNiagaraComponent;
class UAudioComponent;

/**
 * FCycleMemoryEntry - Port of gCycleMemoryEntry
 * AI memory about nearby walls
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FCycleMemoryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory")
	TWeakObjectPtr<AArmaCycle> Cycle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory")
	float Distance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory")
	int32 Side;  // -1 = left, 1 = right

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory")
	float Time;

	FCycleMemoryEntry() : Distance(0.0f), Side(0), Time(0.0f) {}
};

/**
 * FCycleMemory - Port of gCycleMemory
 * Collection of memory entries about other cycles
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FCycleMemory
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory")
	TArray<FCycleMemoryEntry> Entries;

	FCycleMemoryEntry* Remember(AArmaCycle* Cycle);
	FCycleMemoryEntry* Latest(int32 Side) const;
	FCycleMemoryEntry* Earliest(int32 Side) const;
	void Clear() { Entries.Empty(); }
};

/**
 * ECycleTacticalPosition - Port of TacticalPosition enum
 */
UENUM(BlueprintType)
enum class ECycleTacticalPosition : uint8
{
	Start		= 0,
	NS			= 1,
	Goal		= 2,
	Defense		= 3,
	Midfield	= 4,
	Sumo		= 5,
	Offense		= 6,
	Attacking	= 7,
	Count		= 8
};

/**
 * AArmaCycle - Port of gCycle
 * The complete lightcycle pawn
 */
UCLASS(BlueprintType, Blueprintable)
class ARMAGETRONUE5_API AArmaCycle : public APawn
{
	GENERATED_BODY()

public:
	AArmaCycle();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	//////////////////////////////////////////////////////////////////////////
	// Movement Component Access
	//////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category = "Cycle")
	UArmaCycleMovementComponent* GetCycleMovement() const { return CycleMovement; }

	//////////////////////////////////////////////////////////////////////////
	// Wall Building - Port from gCycle
	//////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	void SetCurrentWall(AArmaWall* Wall);

	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	AArmaWall* GetCurrentWall() const { return CurrentWall; }

	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	void DropWall(bool bBuildNew = true);

	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	void SetWallBuilding(bool bBuild);

	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	bool IsWallBuilding() const { return bBuildingWall; }

	// Wall configuration (static - from gCycle)
	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	static void SetWallsStayUpDelay(float Delay) { WallsStayUpDelay = Delay; }

	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	static float GetWallsStayUpDelay() { return WallsStayUpDelay; }

	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	static void SetWallsLength(float Length) { WallsLength = Length; }

	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	static float GetWallsLength() { return WallsLength; }

	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	static void SetExplosionRadius(float Radius) { ExplosionRadius = Radius; }

	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	static float GetExplosionRadius() { return ExplosionRadius; }

	// Instance wall length
	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	float GetMaxWallsLength() const;

	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	float GetThisWallsLength() const;

	UFUNCTION(BlueprintCallable, Category = "Cycle|Wall")
	float GetWallEndSpeed() const;

	//////////////////////////////////////////////////////////////////////////
	// Death and Kill System - Port from gCycle
	//////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category = "Cycle|Death")
	void Kill();

	UFUNCTION(BlueprintCallable, Category = "Cycle|Death")
	void KillWithReason(const FString& Reason);

	UFUNCTION(BlueprintCallable, Category = "Cycle|Death")
	void KillAt(const FArmaCoord& Position);

	UFUNCTION(BlueprintCallable, Category = "Cycle|Death")
	void Killed(AArmaCycle* Killer, int32 Type = 0);

	UFUNCTION(BlueprintCallable, Category = "Cycle|Death")
	bool IsAlive() const;

	UFUNCTION(BlueprintCallable, Category = "Cycle|Death")
	bool IsVulnerable() const;

	UPROPERTY(BlueprintReadWrite, Category = "Cycle|Death")
	FString DeathReason;

	//////////////////////////////////////////////////////////////////////////
	// Collision - Port from gCycle
	//////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category = "Cycle|Collision")
	bool EdgeIsDangerous(AArmaWall* Wall, float Time, float Alpha) const;

	UFUNCTION(BlueprintCallable, Category = "Cycle|Collision")
	void PassEdge(AArmaWall* Wall, float Time, float Alpha, int32 Recursion = 1);

	UFUNCTION(BlueprintCallable, Category = "Cycle|Collision")
	bool IsMe(AActor* Other) const;

	//////////////////////////////////////////////////////////////////////////
	// Visual Properties - Port from gCycle
	//////////////////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle|Visual")
	FArmaColor CycleColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle|Visual")
	FArmaColor TrailColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle|Visual")
	float Skew;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle|Visual")
	float SkewDot;

	//////////////////////////////////////////////////////////////////////////
	// AI Memory - Port from gCycle
	//////////////////////////////////////////////////////////////////////////

	UPROPERTY(BlueprintReadOnly, Category = "Cycle|AI")
	FCycleMemory Memory;

	//////////////////////////////////////////////////////////////////////////
	// Tactical Positioning (for AI)
	//////////////////////////////////////////////////////////////////////////

	UPROPERTY(BlueprintReadWrite, Category = "Cycle|Tactical")
	ECycleTacticalPosition TacticalPosition;

	UPROPERTY(BlueprintReadWrite, Category = "Cycle|Tactical")
	int32 ClosestZoneID;

	//////////////////////////////////////////////////////////////////////////
	// Camera
	//////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category = "Cycle|Camera")
	FVector GetCameraPosition() const;

	UFUNCTION(BlueprintCallable, Category = "Cycle|Camera")
	FVector GetPredictPosition() const;

	UFUNCTION(BlueprintCallable, Category = "Cycle|Camera")
	FVector GetCameraTop() const;

	//////////////////////////////////////////////////////////////////////////
	// Input Actions
	//////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category = "Cycle|Input")
	void TurnLeft();

	UFUNCTION(BlueprintCallable, Category = "Cycle|Input")
	void TurnRight();

	UFUNCTION(BlueprintCallable, Category = "Cycle|Input")
	void StartBrake();

	UFUNCTION(BlueprintCallable, Category = "Cycle|Input")
	void StopBrake();

	//////////////////////////////////////////////////////////////////////////
	// Events
	//////////////////////////////////////////////////////////////////////////

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCycleSpawn);
	UPROPERTY(BlueprintAssignable, Category = "Cycle|Events")
	FOnCycleSpawn OnSpawn;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCycleKill, AArmaCycle*, Victim, int32, Type);
	UPROPERTY(BlueprintAssignable, Category = "Cycle|Events")
	FOnCycleKill OnKill;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCycleDeath, FString, Reason);
	UPROPERTY(BlueprintAssignable, Category = "Cycle|Events")
	FOnCycleDeath OnDeath;

protected:
	//////////////////////////////////////////////////////////////////////////
	// Components
	//////////////////////////////////////////////////////////////////////////

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* CycleBodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* FrontWheelMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* RearWheelMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UArmaCycleMovementComponent* CycleMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* TrailParticles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UAudioComponent* EngineSound;

	//////////////////////////////////////////////////////////////////////////
	// Internal State
	//////////////////////////////////////////////////////////////////////////

	UPROPERTY()
	AArmaWall* CurrentWall;

	UPROPERTY()
	AArmaWall* LastWall;

	UPROPERTY()
	bool bBuildingWall;

	UPROPERTY()
	bool bDropWallRequested;

	UPROPERTY()
	float SpawnTime;

	UPROPERTY()
	float LastTimeAnim;

	UPROPERTY()
	FArmaCoord LastGoodPosition;

	UPROPERTY()
	FArmaCoord CorrectPosSmooth;

	UPROPERTY()
	FArmaCoord PredictPosition;

	UPROPERTY()
	float CorrectDistanceSmooth;

	// Static wall settings
	static float WallsStayUpDelay;
	static float WallsLength;
	static float ExplosionRadius;

	//////////////////////////////////////////////////////////////////////////
	// Internal Methods
	//////////////////////////////////////////////////////////////////////////

	virtual void OnRemoveFromGame();
	virtual void OnRoundEnd();
	virtual void RightBeforeDeath(int32 NumTries);

	void UpdateWallBuilding(float DeltaTime);
	void UpdateWheelAnimation(float DeltaTime);
	void UpdateSkew(float DeltaTime);
	void SpawnNewWall();

	// Handle movement component death
	UFUNCTION()
	void OnMovementDeath(float Time);

	// Handle turn notification
	UFUNCTION()
	void OnMovementTurn(int32 Direction);
};

