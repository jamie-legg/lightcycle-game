// ArmaWallRegistry.h - Global wall registry for collision detection

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArmaWallRegistry.generated.h"

/**
 * Wall type - Rim walls behave differently from cycle walls
 */
UENUM(BlueprintType)
enum class EArmaWallType : uint8
{
	Rim,		// Arena boundary - no acceleration boost
	Cycle,		// Player/AI trail - provides acceleration when nearby
};

/**
 * A single wall segment for 2D collision in the global registry
 */
USTRUCT(BlueprintType)
struct FArmaRegisteredWall
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector2D Start = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector2D End = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	EArmaWallType WallType = EArmaWallType::Cycle;

	UPROPERTY(BlueprintReadOnly)
	AActor* OwnerActor = nullptr;  // The cycle that created this (nullptr for rim walls)

	UPROPERTY(BlueprintReadOnly)
	AActor* VisualActor = nullptr;  // The mesh actor

	UPROPERTY(BlueprintReadOnly)
	float CreationTime = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	int32 WallID = 0;

	FArmaRegisteredWall() {}
	FArmaRegisteredWall(FVector2D InStart, FVector2D InEnd, EArmaWallType InType, AActor* InOwner, AActor* InVisual, float InTime, int32 InID)
		: Start(InStart), End(InEnd), WallType(InType), OwnerActor(InOwner), VisualActor(InVisual), CreationTime(InTime), WallID(InID) {}
};

/**
 * UArmaWallRegistry - World subsystem that holds all walls
 * All cycles register their walls here, and all cycles check against this registry
 */
UCLASS()
class ARMAGETRONUE5_API UArmaWallRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Get the singleton for this world
	static UArmaWallRegistry* Get(UWorld* World);

	// Register a new wall segment, returns wall ID
	UFUNCTION(BlueprintCallable, Category = "Walls")
	int32 RegisterWall(FVector2D Start, FVector2D End, EArmaWallType WallType, AActor* Owner, AActor* VisualActor);

	// Update a wall's end position (for growing walls)
	UFUNCTION(BlueprintCallable, Category = "Walls")
	void UpdateWallEnd(int32 WallID, FVector2D NewEnd);

	// Remove all walls owned by an actor
	UFUNCTION(BlueprintCallable, Category = "Walls")
	void RemoveWallsByOwner(AActor* Owner);

	// Remove a specific wall
	UFUNCTION(BlueprintCallable, Category = "Walls")
	void RemoveWall(int32 WallID);

	// Clear all walls
	UFUNCTION(BlueprintCallable, Category = "Walls")
	void ClearAllWalls();

	// Get all walls (for collision checking)
	UFUNCTION(BlueprintCallable, Category = "Walls")
	const TArray<FArmaRegisteredWall>& GetAllWalls() const { return Walls; }
	
	// Get total wall count
	UFUNCTION(BlueprintCallable, Category = "Walls")
	int32 GetWallCount() const { return Walls.Num(); }

	// Spawn arena rim walls (rectangular boundary)
	UFUNCTION(BlueprintCallable, Category = "Walls")
	void SpawnArenaRim(float HalfWidth, float HalfHeight, float WallHeight = 150.0f);

	// Check for ray-wall intersection, returns closest hit distance
	// Returns MAX_FLT if no hit
	// OutSide: Returns which side of the wall we're on (positive = one side, negative = other side)
	UFUNCTION(BlueprintCallable, Category = "Walls")
	float RaycastWalls(FVector2D Origin, FVector2D Direction, float MaxDistance, 
		AActor* IgnoreOwner, float GraceTime, FArmaRegisteredWall& OutHitWall, float& OutSide) const;

	// Get distance to nearest wall (for acceleration)
	UFUNCTION(BlueprintCallable, Category = "Walls")
	float GetDistanceToNearestCycleWall(FVector2D Position, FVector2D Direction, float MaxDistance, AActor* IgnoreOwner) const;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	UPROPERTY()
	TArray<FArmaRegisteredWall> Walls;

	int32 NextWallID = 1;

	// Helper: point-to-segment distance
	float DistanceToSegment(FVector2D Point, FVector2D SegStart, FVector2D SegEnd) const;

	// Helper: ray-segment intersection
	bool RaySegmentIntersection(FVector2D RayOrigin, FVector2D RayDir, FVector2D SegStart, FVector2D SegEnd, float& OutT) const;
};

