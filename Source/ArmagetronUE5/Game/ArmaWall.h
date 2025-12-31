// ArmaWall.h - Port of gWall/gNetPlayerWall from src/tron/gWall.h
// Lightcycle trail wall with procedural mesh and hole mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ArmaTypes.h"
#include "ProceduralMeshComponent.h"
#include "ArmaWall.generated.h"

// Forward declarations
class AArmaCycle;
// class AArmaExplosion removed - using AActor* instead
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

/**
 * FArmaWallSegment - Port of gPlayerWallCoord
 * A coordinate entry for wall segments
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaWallSegment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	float Pos;  // Start position (distance from cycle start)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	float Time;  // Time this segment was created

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	bool bIsDangerous;  // True if this segment is solid (not a hole)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	TWeakObjectPtr<AActor> Holer;  // Who created this hole (if applicable)

	FArmaWallSegment()
		: Pos(0.0f), Time(0.0f), bIsDangerous(true)
	{}

	FArmaWallSegment(float InPos, float InTime, bool bInDangerous = true)
		: Pos(InPos), Time(InTime), bIsDangerous(bInDangerous)
	{}
};

/**
 * AArmaWall - Port of gPlayerWall/gNetPlayerWall
 * The lightcycle trail wall actor with procedural mesh
 */
UCLASS(BlueprintType, Blueprintable)
class ARMAGETRONUE5_API AArmaWall : public AActor
{
	GENERATED_BODY()

public:
	AArmaWall();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//////////////////////////////////////////////////////////////////////////
	// Initialization
	//////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category = "Wall")
	void Initialize(AArmaCycle* OwnerCycle, const FArmaColor& Color);

	UFUNCTION(BlueprintCallable, Category = "Wall")
	void Finalize();

	//////////////////////////////////////////////////////////////////////////
	// Wall Updates
	//////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category = "Wall")
	void UpdateEnd(const FArmaCoord& NewEnd, float Time);

	// Add a checkpoint for more accurate interpolation
	UFUNCTION(BlueprintCallable, Category = "Wall")
	void Checkpoint();

	//////////////////////////////////////////////////////////////////////////
	// Position/Time Queries - Port from gNetPlayerWall
	//////////////////////////////////////////////////////////////////////////

	// Get time at a given alpha along wall (0 = start, 1 = end)
	UFUNCTION(BlueprintCallable, Category = "Wall|Query")
	float GetTimeAtAlpha(float Alpha) const;

	// Get position (distance) at a given alpha
	UFUNCTION(BlueprintCallable, Category = "Wall|Query")
	float GetPosAtAlpha(float Alpha) const;

	// Get alpha from position
	UFUNCTION(BlueprintCallable, Category = "Wall|Query")
	float GetAlphaFromPos(float Pos) const;

	// Start/End accessors
	UFUNCTION(BlueprintCallable, Category = "Wall|Query")
	float GetBeginPos() const { return BeginDist; }

	UFUNCTION(BlueprintCallable, Category = "Wall|Query")
	float GetEndPos() const { return EndDist; }

	UFUNCTION(BlueprintCallable, Category = "Wall|Query")
	float GetBeginTime() const { return BeginTime; }

	UFUNCTION(BlueprintCallable, Category = "Wall|Query")
	float GetEndTime() const { return EndTime; }

	UFUNCTION(BlueprintCallable, Category = "Wall|Query")
	FArmaCoord GetBeginPoint() const { return BeginPoint; }

	UFUNCTION(BlueprintCallable, Category = "Wall|Query")
	FArmaCoord GetEndPoint() const { return EndPoint; }

	UFUNCTION(BlueprintCallable, Category = "Wall|Query")
	FArmaCoord GetDirection() const { return Direction; }

	//////////////////////////////////////////////////////////////////////////
	// Danger Queries - Port from gPlayerWall
	//////////////////////////////////////////////////////////////////////////

	// Is wall dangerous anywhere?
	UFUNCTION(BlueprintCallable, Category = "Wall|Danger")
	bool IsDangerousAnywhere(float Time) const;

	// Is wall dangerous at specific point?
	UFUNCTION(BlueprintCallable, Category = "Wall|Danger")
	bool IsDangerous(float Alpha, float Time) const;

	// Is wall dangerous apart from holes?
	UFUNCTION(BlueprintCallable, Category = "Wall|Danger")
	bool IsDangerousApartFromHoles(float Alpha, float Time) const;

	// Get who created a hole at alpha
	UFUNCTION(BlueprintCallable, Category = "Wall|Danger")
	AActor* GetHoler(float Alpha, float Time) const;

	//////////////////////////////////////////////////////////////////////////
	// Hole System - Port from gNetPlayerWall
	//////////////////////////////////////////////////////////////////////////

	// Blow a hole in the wall
	UFUNCTION(BlueprintCallable, Category = "Wall|Hole")
	void BlowHole(float BeginDist, float EndDist, AActor* Holer);

	//////////////////////////////////////////////////////////////////////////
	// Owner
	//////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintCallable, Category = "Wall")
	AArmaCycle* GetOwnerCycle() const { return OwnerCycle.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Wall")
	int32 GetWindingNumber() const { return WindingNumber; }

	//////////////////////////////////////////////////////////////////////////
	// Visual
	//////////////////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Visual")
	FArmaColor WallColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Visual")
	float WallHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Visual")
	float WallThickness;

	//////////////////////////////////////////////////////////////////////////
	// Events
	//////////////////////////////////////////////////////////////////////////

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWallHole, float, BeginPos, float, EndPos);
	UPROPERTY(BlueprintAssignable, Category = "Wall|Events")
	FOnWallHole OnHoleCreated;

protected:
	//////////////////////////////////////////////////////////////////////////
	// Components
	//////////////////////////////////////////////////////////////////////////

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProceduralMeshComponent* WallMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProceduralMeshComponent* TopGlowMesh;

	//////////////////////////////////////////////////////////////////////////
	// Internal State
	//////////////////////////////////////////////////////////////////////////

	UPROPERTY()
	TWeakObjectPtr<AArmaCycle> OwnerCycle;

	UPROPERTY()
	FArmaCoord BeginPoint;

	UPROPERTY()
	FArmaCoord EndPoint;

	UPROPERTY()
	FArmaCoord Direction;

	UPROPERTY()
	float BeginDist;

	UPROPERTY()
	float EndDist;

	UPROPERTY()
	float BeginTime;

	UPROPERTY()
	float EndTime;

	UPROPERTY()
	int32 WindingNumber;

	UPROPERTY()
	bool bFinalized;

	UPROPERTY()
	bool bInGrid;

	UPROPERTY()
	float GriddingTime;

	UPROPERTY()
	bool bPreliminary;

	UPROPERTY()
	float ObsoletedTime;

	// Wall segments (for holes)
	UPROPERTY()
	TArray<FArmaWallSegment> Segments;

	// Material
	UPROPERTY()
	UMaterialInstanceDynamic* WallMaterial;

	//////////////////////////////////////////////////////////////////////////
	// Mesh Generation
	//////////////////////////////////////////////////////////////////////////

	void GenerateMesh();
	void UpdateMesh();

	// Generate vertices for a wall quad
	void GenerateWallQuad(
		TArray<FVector>& Vertices,
		TArray<int32>& Triangles,
		TArray<FVector>& Normals,
		TArray<FVector2D>& UVs,
		TArray<FLinearColor>& Colors,
		const FArmaCoord& Start,
		const FArmaCoord& End,
		float Height,
		float Thickness
	);

	// Find segment index for a given alpha
	int32 FindSegmentIndex(float Alpha) const;

	// Find segment index for a given distance
	int32 FindSegmentIndexByPos(float Pos) const;
};

/**
 * AArmaWallRim - Port of gWallRim
 * The arena boundary wall
 */
UCLASS(BlueprintType, Blueprintable)
class ARMAGETRONUE5_API AArmaWallRim : public AActor
{
	GENERATED_BODY()

public:
	AArmaWallRim();

	virtual void BeginPlay() override;

	// Initialize rim wall segment
	UFUNCTION(BlueprintCallable, Category = "Wall")
	void Initialize(const FArmaCoord& Start, const FArmaCoord& End, float Height = 10000.0f);

	// Is this wall splittable?
	UFUNCTION(BlueprintCallable, Category = "Wall")
	bool IsSplittable() const { return true; }

	// Get wall height
	UFUNCTION(BlueprintCallable, Category = "Wall")
	float GetHeight() const { return RimHeight; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProceduralMeshComponent* RimMesh;

	UPROPERTY()
	FArmaCoord StartPoint;

	UPROPERTY()
	FArmaCoord EndPoint;

	UPROPERTY()
	float RimHeight;

	UPROPERTY()
	float TextureBegin;

	UPROPERTY()
	float TextureEnd;

	void GenerateRimMesh();
};

