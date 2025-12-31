// ArmaGrid.h - Port of eGrid from src/engine/eGrid.h
// Collision grid system with faces, edges, and points

#pragma once

#include "CoreMinimal.h"
#include "ArmaTypes.h"
#include "Components/ActorComponent.h"
#include "ArmaGrid.generated.h"

// Forward declarations
class AArmaWall;
class AArmaCycle;
class UArmaGridSubsystem;

/**
 * FArmaGridPoint - Port of ePoint from original
 * A point in the grid mesh
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaGridPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FArmaCoord Position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	TArray<int32> EdgeIDs;  // IDs of half-edges emanating from this point

	FArmaGridPoint() : ID(-1) {}
	FArmaGridPoint(int32 InID, const FArmaCoord& InPos) : ID(InID), Position(InPos) {}
};

/**
 * FArmaGridHalfEdge - Port of eHalfEdge from original
 * A directed edge in the grid mesh (each edge has two half-edges)
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaGridHalfEdge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 PointID;  // Point this edge emanates from

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 TwinID;  // The opposite half-edge

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 NextID;  // Next edge in face (counter-clockwise)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 PrevID;  // Previous edge in face

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 FaceID;  // Face this edge borders

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	TWeakObjectPtr<AArmaWall> Wall;  // Wall on this edge (if any)

	FArmaGridHalfEdge()
		: ID(-1), PointID(-1), TwinID(-1), NextID(-1), PrevID(-1), FaceID(-1)
	{}
};

/**
 * FArmaGridFace - Port of eFace from original
 * A triangular face in the grid mesh
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaGridFace
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 ID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 EdgeID;  // One of the edges bounding this face

	// Cached data for quick checks
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FArmaCoord Center;

	FArmaGridFace() : ID(-1), EdgeID(-1) {}
};

/**
 * EArmaAxisDirection - Winding directions for grid-aligned movement
 */
UENUM(BlueprintType)
enum class EArmaAxisDirection : uint8
{
	Right	= 0,
	Up		= 1,
	Left	= 2,
	Down	= 3
};

/**
 * FArmaAxis - Port of eAxis from src/engine/eAxis.h
 * Manages the valid directions on a grid
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaAxis
{
	GENERATED_BODY()

	// Number of valid directions (4 for standard grid)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis")
	int32 WindingNumber;

	// Direction vectors for each winding
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Axis")
	TArray<FArmaCoord> Directions;

	FArmaAxis() : WindingNumber(4)
	{
		// Standard 4-directional grid
		Directions.Add(FArmaCoord(1.0f, 0.0f));   // Right (0)
		Directions.Add(FArmaCoord(0.0f, 1.0f));   // Up (1)
		Directions.Add(FArmaCoord(-1.0f, 0.0f));  // Left (2)
		Directions.Add(FArmaCoord(0.0f, -1.0f));  // Down (3)
	}

	// Get direction for a winding number
	FArmaCoord GetDirection(int32 Winding) const
	{
		int32 Wrapped = ((Winding % WindingNumber) + WindingNumber) % WindingNumber;
		return Directions.IsValidIndex(Wrapped) ? Directions[Wrapped] : FArmaCoord::UnitX;
	}

	// Get winding number for a direction
	int32 GetWinding(const FArmaCoord& Dir) const
	{
		float BestDot = -2.0f;
		int32 BestWinding = 0;
		for (int32 i = 0; i < Directions.Num(); i++)
		{
			float Dot = Dir.Dot(Directions[i]);
			if (Dot > BestDot)
			{
				BestDot = Dot;
				BestWinding = i;
			}
		}
		return BestWinding;
	}

	// Turn function - changes winding based on turn direction
	int32 Turn(int32 CurrentWinding, int32 Direction) const
	{
		// Direction > 0: turn left (counter-clockwise), < 0: turn right (clockwise)
		int32 NewWinding = CurrentWinding + (Direction > 0 ? 1 : -1);
		return ((NewWinding % WindingNumber) + WindingNumber) % WindingNumber;
	}

	// Get angle for a winding
	float GetAngle(int32 Winding) const
	{
		return (2.0f * PI * Winding) / WindingNumber;
	}
};

/**
 * UArmaGridSubsystem - Port of eGrid
 * Manages the game grid, collision detection, and spatial queries
 */
UCLASS()
class ARMAGETRONUE5_API UArmaGridSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UArmaGridSubsystem();

	// UWorldSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// Grid creation and management
	UFUNCTION(BlueprintCallable, Category = "Grid")
	void CreateGrid(float Size);

	UFUNCTION(BlueprintCallable, Category = "Grid")
	void ClearGrid();

	// Port of FindSurroundingFace - find which face contains a point
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 FindSurroundingFace(const FArmaCoord& Coord, int32 StartFaceID = -1) const;

	// Port of DrawLine - insert a wall into the grid
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 DrawLine(int32 StartPointID, const FArmaCoord& End, AArmaWall* Wall = nullptr);

	// Insert a new point into the grid
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 InsertPoint(const FArmaCoord& Coord, int32 GuessFaceID = -1);

	// Get grid winding number (number of valid directions)
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 GetWindingNumber() const { return Axis.WindingNumber; }

	// Get direction for a winding
	UFUNCTION(BlueprintCallable, Category = "Grid")
	FArmaCoord GetDirection(int32 Winding) const { return Axis.GetDirection(Winding); }

	// Get winding for a direction
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 GetDirectionWinding(const FArmaCoord& Dir) const { return Axis.GetWinding(Dir); }

	// Turn function
	UFUNCTION(BlueprintCallable, Category = "Grid")
	int32 Turn(int32 CurrentWinding, int32 Direction) const { return Axis.Turn(CurrentWinding, Direction); }

	// Process walls in range (for explosions)
	void ProcessWallsInRange(const FArmaCoord& Pos, float Range, TFunctionRef<void(AArmaWall*)> Processor);

	// Accessors
	const FArmaAxis& GetAxis() const { return Axis; }
	FArmaGridPoint* GetPoint(int32 ID);
	FArmaGridHalfEdge* GetEdge(int32 ID);
	FArmaGridFace* GetFace(int32 ID);

	// Check if point is inside a face
	bool IsPointInFace(const FArmaCoord& Point, int32 FaceID) const;

	// Ray cast against grid edges
	bool RayCast(const FArmaCoord& Start, const FArmaCoord& End, FArmaCoord& HitPoint, AArmaWall*& HitWall) const;

protected:
	UPROPERTY()
	FArmaAxis Axis;

	UPROPERTY()
	TArray<FArmaGridPoint> Points;

	UPROPERTY()
	TArray<FArmaGridHalfEdge> Edges;

	UPROPERTY()
	TArray<FArmaGridFace> Faces;

	UPROPERTY()
	float GridSize;

	// Internal helpers
	int32 AddPoint(const FArmaCoord& Coord);
	int32 AddEdgePair(int32 Point1ID, int32 Point2ID);
	int32 AddFace(int32 EdgeID);
	void SplitEdge(int32 EdgeID, int32 NewPointID);
	void ConnectEdges(int32 Edge1ID, int32 Edge2ID);

	// Find which edge a point lies on (for insertion)
	int32 FindEdgeForPoint(const FArmaCoord& Point, int32 FaceID) const;
};

/**
 * AArmaArena - Port of gArena
 * The game arena/map
 */
UCLASS(BlueprintType, Blueprintable)
class ARMAGETRONUE5_API AArmaArena : public AActor
{
	GENERATED_BODY()

public:
	AArmaArena();

	virtual void BeginPlay() override;

	// Arena setup
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void PrepareArena(float SizeMultiplier = 1.0f);

	// Get a random position in the arena
	UFUNCTION(BlueprintCallable, Category = "Arena")
	FArmaCoord GetRandomPosition(float Factor = 0.8f) const;

	// Get the least dangerous spawn point
	UFUNCTION(BlueprintCallable, Category = "Arena")
	FArmaCoord GetBestSpawnPoint() const;

	// Get closest spawn point to location
	UFUNCTION(BlueprintCallable, Category = "Arena")
	FArmaCoord GetClosestSpawnPoint(const FArmaCoord& Location) const;

	// Check if position is inside arena
	UFUNCTION(BlueprintCallable, Category = "Arena")
	bool IsInsideArena(const FArmaCoord& Position) const;

	// Get arena bounds
	UFUNCTION(BlueprintCallable, Category = "Arena")
	FBox2D GetArenaBounds() const;

	// Size accessor
	UFUNCTION(BlueprintCallable, Category = "Arena")
	float GetArenaSize() const { return ArenaSize; }

	// Size multiplier (static, like original)
	UFUNCTION(BlueprintCallable, Category = "Arena")
	static float GetSizeMultiplier() { return SizeMultiplier; }

	UFUNCTION(BlueprintCallable, Category = "Arena")
	static void SetSizeMultiplier(float Mult) { SizeMultiplier = Mult; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float ArenaSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	TArray<FArmaCoord> SpawnPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	TArray<FArmaCoord> SpawnDirections;

	static float SizeMultiplier;

	// Create rim walls around arena
	void CreateRimWalls();

	// Generate spawn points
	void GenerateSpawnPoints(int32 NumPoints = 8);

	// Visual components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* RootSceneComponent;
};

/**
 * FArmaSpawnPoint - Port of gSpawnPoint
 * A spawn location in the arena
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaSpawnPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	FArmaCoord Location;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	FArmaCoord Direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float LastUseTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float DangerLevel;

	FArmaSpawnPoint()
		: LastUseTime(-100.0f), DangerLevel(0.0f)
	{}

	FArmaSpawnPoint(const FArmaCoord& InLoc, const FArmaCoord& InDir)
		: Location(InLoc), Direction(InDir), LastUseTime(-100.0f), DangerLevel(0.0f)
	{}

	// Calculate danger based on nearby walls and cycles
	void UpdateDanger(const UWorld* World);
};

