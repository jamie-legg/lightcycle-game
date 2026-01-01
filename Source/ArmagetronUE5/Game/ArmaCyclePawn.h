// ArmaCyclePawn.h - Armagetron/Snake style test pawn with glowing trails

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ArmaCyclePawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;

/**
 * AArmaCyclePawn - Snake/Armagetron style movement with glowing trails
 */
UCLASS()
class ARMAGETRONUE5_API AArmaCyclePawn : public APawn
{
	GENERATED_BODY()

public:
	AArmaCyclePawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// ========== Components ==========
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* CycleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* CameraArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* Camera;

	// ========== Movement Settings ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MoveSpeed = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MaxSpeed = 2000.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float BaseSpeed = 800.0f;
	
	// Armagetron turn mechanics
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float TurnSpeedFactor = 0.95f;  // Speed multiplied by this on each turn (sg_cycleTurnSpeedFactor)
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SpeedDecayBelow = 5.0f;  // Acceleration toward base speed when slow (sg_speedCycleDecayBelow)
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SpeedDecayAbove = 0.1f;  // Deceleration toward base speed when fast (sg_speedCycleDecayAbove)
	
	// Turn delay (sg_delayCycle from original - minimum time between turns)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float TurnDelay = 0.1f;  // 100ms minimum between turns (sg_delayCycle)
	
	// Turn memory - how many turns can be queued (sg_cycleTurnMemory)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	int32 TurnMemory = 3;  // Max queued turns (sg_cycleTurnMemory)
	
	// Check if a turn is currently allowed
	UFUNCTION(BlueprintCallable, Category = "Movement")
	bool CanMakeTurn() const;

	// ========== Rubber System ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubber")
	float MaxRubber = 100.0f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Rubber")
	float CurrentRubber = 100.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubber")
	float RubberRegenRate = 10.0f;  // Per second when not grinding
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubber")
	float RubberDecayRate = 50.0f;  // Per second when grinding
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubber")
	float RubberActivationDistance = 50.0f;  // Distance to wall to start using rubber
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubber")
	float MinWallDistance = 1.0f;  // Closest distance rubber allows (sg_rubberCycleMinDistance, very small like original)
	
	UPROPERTY(BlueprintReadOnly, Category = "Rubber")
	bool bIsGrinding = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "Rubber")
	float DistanceToWall = 9999.0f;
	
	// Track which side of the wall we're on (positive = one side, negative = other side)
	// Used to prevent going through walls when turning quickly alongside them
	UPROPERTY(BlueprintReadOnly, Category = "Rubber")
	float CurrentWallSide = 0.0f;  // Side of the wall we're currently on (from last collision check)
	
	UPROPERTY(BlueprintReadOnly, Category = "Rubber")
	int32 CurrentWallID = 0;  // ID of the wall we're tracking side for
	
	// Grace period after turn to allow escaping tight situations (digging mechanic)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubber")
	float TurnGracePeriod = 0.15f;  // Time after turn where collision is relaxed
	
	UPROPERTY(BlueprintReadOnly, Category = "Rubber")
	float LastTurnTime = 0.0f;  // Time of last turn for grace period
	
	// ========== Perfect Turn Protection (sg_rubberCycleMinAdjust from original) ==========
	// When adjusting to a wall after a turn, allow getting closer by at least this percentage
	// This is the key mechanic for "digging" - right after a turn, minimum distance is reduced
	// Formula: maxStop = (distSinceLastTurn + space) * (1 - MinAdjust)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubber")
	float RubberMinAdjust = 0.05f;  // sg_rubberCycleMinAdjust - 5% means we can get 5% closer after turning
	
	// Track position at last turn for calculating distance
	UPROPERTY(BlueprintReadOnly, Category = "Rubber")
	FVector LastTurnPosition = FVector::ZeroVector;
	
	// Get distance traveled since last turn
	UFUNCTION(BlueprintCallable, Category = "Rubber")
	float GetDistanceSinceLastTurn() const;
	
	// ========== Digging/Gap System (Armagetron advanced mechanic) ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digging")
	float DiggingRubberMultiplier = 3.0f;  // Extra rubber cost when turning into a wall
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Digging")
	float MinDigDistance = 5.0f;  // Minimum distance for a dig segment
	
	UPROPERTY(BlueprintReadOnly, Category = "Digging")
	float GapLeft = 0.0f;  // Detected gap size on left side
	
	UPROPERTY(BlueprintReadOnly, Category = "Digging")
	float GapRight = 0.0f;  // Detected gap size on right side

	// ========== Wall Acceleration ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acceleration")
	float WallAcceleration = 50000.0f;  // Speed boost per second near walls (sg_accelerationCycle, increased 100x)
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acceleration")
	float WallAccelDistance = 400.0f;  // Distance to trigger acceleration (sg_nearCycle)
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acceleration")
	float WallAccelOffset = 10.0f;  // Offset for inverse formula (sg_accelerationCycleOffs)
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acceleration")
	float SlingshotMultiplier = 2.0f;  // Bonus when between two walls

	// ========== Death/Respawn ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
	float SpawnInvulnerabilityTime = 2.0f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Respawn")
	float SpawnTime = 0.0f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Respawn")
	int32 CurrentRound = 1;
	
	UPROPERTY(BlueprintReadOnly, Category = "Respawn")
	int32 DeathCount = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "Respawn")
	FVector SpawnLocation = FVector(-200, 0, 92);
	
	UPROPERTY(BlueprintReadOnly, Category = "Respawn")
	FVector SpawnDirection = FVector(1, 0, 0);

	// ========== Visual Settings ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	FLinearColor CycleColor = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f); // Cyan

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	float TrailHeight = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	float TrailWidth = 15.0f;  // Wall thickness - increased to prevent going through walls

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	float EmissiveStrength = 20.0f;

	// ========== Wall Length/Decay (sg_wallsLength) ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallLength")
	float MaxWallsLength = -1.0f;  // -1 = infinite, >0 = max total wall length (WALLS_LENGTH)
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallLength")
	float WallDecayRate = 0.0f;  // Units per second that wall tail shrinks (0 = no decay)
	
	UPROPERTY(BlueprintReadOnly, Category = "WallLength")
	float TotalWallLength = 0.0f;  // Current total length of all walls

	// ========== State ==========
	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsAlive = true;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	FVector MoveDirection = FVector(1, 0, 0);
	
	// ========== Physics Functions ==========
	UFUNCTION(BlueprintCallable, Category = "Physics")
	bool IsVulnerable() const;
	
	UFUNCTION(BlueprintCallable, Category = "Physics")
	void Die();
	
	UFUNCTION(BlueprintCallable, Category = "Physics")
	void Respawn();
	
	// Collision callbacks
	UFUNCTION()
	void OnWallHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	UFUNCTION()
	void OnWallOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

protected:
	// ========== Physics Helpers ==========
	void UpdateRubber(float DeltaTime);
	void UpdateWallAcceleration(float DeltaTime);
	void UpdateWallDecay();  // Remove old wall segments when exceeding max length
	bool CheckWallCollision(const FVector& DesiredMove, FVector& OutNewLocation);
	float GetDistanceToNearestWall(FVector Direction) const;
	void SpawnSpark(FVector Location, FVector Normal);
	void UpdateInvulnerabilityBlink();
	void ClearAllWalls();
	// ========== Turn Queue (sg_cycleTurnMemory from original) ==========
	// Pending turns queue (like original pendingTurns deque)
	// Values: -1 = left, +1 = right
	TArray<int32> PendingTurns;
	
	// Process any pending turns that are now ready to execute
	void ProcessPendingTurns();
	
	// ========== Input Handlers ==========
	void TurnLeft();
	void TurnRight();
	
	// Internal turn execution (called directly or from queue)
	void ExecuteTurnLeft();
	void ExecuteTurnRight();
	void OnBrakePressed();
	void OnBrakeReleased();
	
	void OnMouseX(float Value);
	void OnMouseY(float Value);
	void OnZoomIn();
	void OnZoomOut();

	// ========== Trail System ==========
	// Wall segment as 2D line (start and end points)
	struct FWallSegment
	{
		FVector2D Start;
		FVector2D End;
		AActor* Actor;
		float CreationTime; // When this wall was created
		
		FWallSegment() : Start(FVector2D::ZeroVector), End(FVector2D::ZeroVector), Actor(nullptr), CreationTime(0) {}
		FWallSegment(FVector2D InStart, FVector2D InEnd, AActor* InActor, float InTime) 
			: Start(InStart), End(InEnd), Actor(InActor), CreationTime(InTime) {}
	};
	
	UPROPERTY()
	TArray<AActor*> WallActors;
	
	// Wall segments for 2D collision (parallel array with WallActors)
	TArray<FWallSegment> WallSegments;
	
	// The currently growing wall segment (updated every frame)
	// Uses procedural mesh component (like rim walls) for proper rendering
	UPROPERTY()
	class AActor* CurrentWallActor;
	
	UPROPERTY()
	UMaterialInstanceDynamic* CurrentWallMaterial;
	
	FVector CurrentWallStart;
	float GameStartTime;
	int32 WallCount = 0;
	
	// ID of current growing wall in the global registry (declared above in Rubber section)
	
	void StartNewWallSegment();
	void UpdateCurrentWall();
	AActor* SpawnWallSegment(FVector Start, FVector End);
	void CreateCurrentWallActor();
	
	// 2D collision helper
	float DistanceToLineSegment2D(FVector2D Point, FVector2D LineStart, FVector2D LineEnd) const;

	// ========== Environment ==========
	void SpawnFloorGrid();
	void SpawnArenaWalls();
	void SpawnAmbientLighting();

	// ========== VFX ==========
	UPROPERTY()
	UPointLightComponent* CycleGlowLight;
	
	UPROPERTY()
	UMaterialInstanceDynamic* GlowMaterial;
	
	void CreateGlowMaterial();
	void SpawnCycleGlow();
	void ApplyChromeMaterial();

	// ========== Camera State ==========
	float TargetArmLength = 800.0f;
	float CameraYawOffset = 0.0f;
	float CameraPitch = -35.0f;
	bool bIsBraking = false;
	
	// Smooth pawn rotation
	float TargetPawnYaw = 0.0f;
	float CurrentPawnYaw = 0.0f;
	float PawnRotationSpeed = 10.0f; // Degrees per second multiplier

	void UpdateCamera(float DeltaTime);
	void DrawHUD();

	// ========== Debug Controls ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebugDrawEnabled = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	int32 DebugSelectedVar = 0;  // 0=Speed, 1=MaxSpeed, 2=Rubber, 3=RubberDecay, 4=WallAccel, 5=AccelDist
	
	void DebugNextVar();
	void DebugPrevVar();
	void DebugIncreaseVar();
	void DebugDecreaseVar();
	void DebugToggleDraw();
	void DrawDebugRays();
	void DrawDebugSliders();

	// ========== ESC Menu ==========
	UPROPERTY(BlueprintReadOnly, Category = "Menu")
	bool bMenuOpen = false;
	
	void ToggleMenu();
	void DrawMenu();
	void ResumeGame();
	void OpenServerBrowser();
	void QuitToDesktop();
	
	// Menu selection
	int32 MenuSelection = 0;
	void MenuUp();
	void MenuDown();
	void MenuSelect();
};

