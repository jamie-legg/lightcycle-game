// ArmaWall.cpp - Implementation of wall/trail system

#include "ArmaWall.h"
#include "ArmaCycle.h"
#include "ArmaCycleMovement.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

//////////////////////////////////////////////////////////////////////////
// AArmaWall Implementation
//////////////////////////////////////////////////////////////////////////

AArmaWall::AArmaWall()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create procedural mesh for wall
	WallMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WallMesh"));
	RootComponent = WallMesh;
	WallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WallMesh->SetCollisionProfileName(TEXT("BlockAll"));
	WallMesh->bUseComplexAsSimpleCollision = false;

	// Create top glow mesh
	TopGlowMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TopGlowMesh"));
	TopGlowMesh->SetupAttachment(RootComponent);
	TopGlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Default values
	WallColor = FArmaColor::Red;
	WallHeight = 2.0f;
	WallThickness = 0.1f;
	BeginDist = 0.0f;
	EndDist = 0.0f;
	BeginTime = 0.0f;
	EndTime = 0.0f;
	WindingNumber = 0;
	bFinalized = false;
	bInGrid = false;
	GriddingTime = 0.0f;
	bPreliminary = false;
	ObsoletedTime = -1.0f;
}

void AArmaWall::BeginPlay()
{
	Super::BeginPlay();

	// Create dynamic material instance - try to load base material first
	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Wall_Base"));
	
	// Fallback to engine's BasicShapeMaterial which is guaranteed to work and renders properly
	if (!BaseMaterial)
	{
		BaseMaterial = LoadObject<UMaterialInterface>(nullptr, 
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	
	// Final fallback to engine default material
	if (!BaseMaterial)
	{
		BaseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	
	if (BaseMaterial)
	{
		WallMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (WallMaterial)
		{
			WallMesh->SetMaterial(0, WallMaterial);
			TopGlowMesh->SetMaterial(0, WallMaterial);
		}
	}
}

void AArmaWall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update mesh if wall is still growing
	if (!bFinalized && OwnerCycle.IsValid())
	{
		UpdateMesh();
	}
}

void AArmaWall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AArmaWall::Initialize(AArmaCycle* InOwnerCycle, const FArmaColor& Color)
{
	OwnerCycle = InOwnerCycle;
	WallColor = Color;

	if (InOwnerCycle)
	{
		FVector Loc = InOwnerCycle->GetActorLocation();
		BeginPoint = FArmaCoord(Loc.X, Loc.Y);
		EndPoint = BeginPoint;

		FVector Forward = InOwnerCycle->GetActorForwardVector();
		Direction = FArmaCoord(Forward.X, Forward.Y).Normalized();

		BeginTime = GetWorld()->GetTimeSeconds();
		EndTime = BeginTime;

		// Get distance from cycle movement
		if (UArmaCycleMovementComponent* Movement = InOwnerCycle->GetCycleMovement())
		{
			BeginDist = Movement->GetDistance();
			EndDist = BeginDist;
			WindingNumber = Movement->GetWindingNumber();
		}
	}

	// Initial segment (all dangerous)
	Segments.Add(FArmaWallSegment(BeginDist, BeginTime, true));

	// Update material color - use "Color" parameter for BasicShapeMaterial
	if (WallMaterial)
	{
		FLinearColor WallLinearColor = WallColor.ToLinearColor();
		WallMaterial->SetVectorParameterValue(TEXT("Color"), WallLinearColor);
		WallMaterial->SetVectorParameterValue(TEXT("BaseColor"), WallLinearColor);
	}

	// Generate initial mesh
	GenerateMesh();
}

void AArmaWall::Finalize()
{
	bFinalized = true;
}

void AArmaWall::UpdateEnd(const FArmaCoord& NewEnd, float Time)
{
	EndPoint = NewEnd;
	EndTime = Time;

	// Calculate new end distance
	FArmaCoord Delta = EndPoint - BeginPoint;
	EndDist = BeginDist + Delta.Norm();
}

void AArmaWall::Checkpoint()
{
	// Add a checkpoint segment for interpolation accuracy
	float CurrentDist = EndDist;
	float CurrentTime = GetWorld()->GetTimeSeconds();
	
	// Only add if there's been meaningful movement
	if (Segments.Num() > 0)
	{
		float LastDist = Segments.Last().Pos;
		if (CurrentDist - LastDist > 1.0f)  // At least 1 unit of movement
		{
			Segments.Add(FArmaWallSegment(CurrentDist, CurrentTime, true));
		}
	}
}

float AArmaWall::GetTimeAtAlpha(float Alpha) const
{
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return FMath::Lerp(BeginTime, EndTime, Alpha);
}

float AArmaWall::GetPosAtAlpha(float Alpha) const
{
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return FMath::Lerp(BeginDist, EndDist, Alpha);
}

float AArmaWall::GetAlphaFromPos(float Pos) const
{
	float Length = EndDist - BeginDist;
	if (Length < KINDA_SMALL_NUMBER)
		return 0.0f;
	
	return FMath::Clamp((Pos - BeginDist) / Length, 0.0f, 1.0f);
}

bool AArmaWall::IsDangerousAnywhere(float Time) const
{
	// Check if any segment is dangerous
	for (const FArmaWallSegment& Segment : Segments)
	{
		if (Segment.bIsDangerous && Segment.Time <= Time)
			return true;
	}
	return false;
}

bool AArmaWall::IsDangerous(float Alpha, float Time) const
{
	// Find segment at this alpha
	float Pos = GetPosAtAlpha(Alpha);
	int32 Index = FindSegmentIndexByPos(Pos);
	
	if (Segments.IsValidIndex(Index))
	{
		const FArmaWallSegment& Segment = Segments[Index];
		return Segment.bIsDangerous && Segment.Time <= Time;
	}

	return true;  // Default dangerous
}

bool AArmaWall::IsDangerousApartFromHoles(float Alpha, float Time) const
{
	// Check danger ignoring holes
	float WallTime = GetTimeAtAlpha(Alpha);
	return WallTime <= Time;
}

AActor* AArmaWall::GetHoler(float Alpha, float Time) const
{
	float Pos = GetPosAtAlpha(Alpha);
	int32 Index = FindSegmentIndexByPos(Pos);
	
	if (Segments.IsValidIndex(Index))
	{
		return Segments[Index].Holer.Get();
	}

	return nullptr;
}

void AArmaWall::BlowHole(float HoleBeginDist, float HoleEndDist, AActor* Holer)
{
	// Clamp to wall bounds
	HoleBeginDist = FMath::Max(HoleBeginDist, BeginDist);
	HoleEndDist = FMath::Min(HoleEndDist, EndDist);

	if (HoleEndDist <= HoleBeginDist)
		return;

	float CurrentTime = GetWorld()->GetTimeSeconds();

	// Find where to insert hole segments
	int32 BeginIndex = FindSegmentIndexByPos(HoleBeginDist);
	
	// Insert hole start segment
	FArmaWallSegment HoleStart(HoleBeginDist, CurrentTime, false);
	HoleStart.Holer = Holer;
	
	// Insert hole end segment (back to dangerous)
	FArmaWallSegment HoleEnd(HoleEndDist, CurrentTime, true);

	// Insert into array (simplified - full implementation would handle overlapping holes)
	if (BeginIndex < 0)
		BeginIndex = 0;

	Segments.Insert(HoleStart, BeginIndex + 1);
	Segments.Insert(HoleEnd, BeginIndex + 2);

	// Regenerate mesh
	UpdateMesh();

	// Broadcast event
	OnHoleCreated.Broadcast(HoleBeginDist, HoleEndDist);
}

int32 AArmaWall::FindSegmentIndex(float Alpha) const
{
	float Pos = GetPosAtAlpha(Alpha);
	return FindSegmentIndexByPos(Pos);
}

int32 AArmaWall::FindSegmentIndexByPos(float Pos) const
{
	for (int32 i = Segments.Num() - 1; i >= 0; i--)
	{
		if (Segments[i].Pos <= Pos)
			return i;
	}
	return 0;
}

void AArmaWall::GenerateMesh()
{
	UpdateMesh();
}

void AArmaWall::UpdateMesh()
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;

	// Generate wall geometry
	FArmaCoord WallVec = EndPoint - BeginPoint;
	float Length = WallVec.Norm();

	if (Length < KINDA_SMALL_NUMBER)
		return;

	// Generate main wall quad
	GenerateWallQuad(Vertices, Triangles, Normals, UVs, Colors, BeginPoint, EndPoint, WallHeight, WallThickness);

	// Apply to procedural mesh
	// Following the original gWall.cpp approach which uses glColor4f for wall coloring
	WallMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), true);
	
	// Enable collision for wall interaction
	WallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	// Ensure the mesh is visible - set a simple color if no material
	if (!WallMaterial)
	{
		// Create a colored material on the fly using BasicShapeMaterial
		UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, 
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!BaseMat)
		{
			BaseMat = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		if (BaseMat)
		{
			WallMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
			WallMesh->SetMaterial(0, WallMaterial);
		}
	}
	
	// Apply wall color to material - use "Color" parameter for BasicShapeMaterial
	if (WallMaterial)
	{
		FLinearColor WallLinearColor = WallColor.ToLinearColor();
		WallMaterial->SetVectorParameterValue(TEXT("Color"), WallLinearColor);
		WallMaterial->SetVectorParameterValue(TEXT("BaseColor"), WallLinearColor);
	}

	// Generate top glow strip
	TArray<FVector> GlowVerts;
	TArray<int32> GlowTris;
	TArray<FVector> GlowNormals;
	TArray<FVector2D> GlowUVs;
	TArray<FLinearColor> GlowColors;

	// Top glow is a thin strip on top of the wall
	FArmaCoord DirNorm = WallVec.Normalized();
	FArmaCoord Perp = DirNorm.Turn(1) * (WallThickness * 0.5f);

	FVector TopLeft(BeginPoint.X + Perp.X, BeginPoint.Y + Perp.Y, WallHeight + 0.01f);
	FVector TopRight(BeginPoint.X - Perp.X, BeginPoint.Y - Perp.Y, WallHeight + 0.01f);
	FVector TopLeftEnd(EndPoint.X + Perp.X, EndPoint.Y + Perp.Y, WallHeight + 0.01f);
	FVector TopRightEnd(EndPoint.X - Perp.X, EndPoint.Y - Perp.Y, WallHeight + 0.01f);

	GlowVerts.Add(TopLeft);
	GlowVerts.Add(TopRight);
	GlowVerts.Add(TopLeftEnd);
	GlowVerts.Add(TopRightEnd);

	GlowTris.Add(0); GlowTris.Add(2); GlowTris.Add(1);
	GlowTris.Add(1); GlowTris.Add(2); GlowTris.Add(3);

	GlowNormals.Add(FVector::UpVector);
	GlowNormals.Add(FVector::UpVector);
	GlowNormals.Add(FVector::UpVector);
	GlowNormals.Add(FVector::UpVector);

	GlowUVs.Add(FVector2D(0, 0));
	GlowUVs.Add(FVector2D(1, 0));
	GlowUVs.Add(FVector2D(0, 1));
	GlowUVs.Add(FVector2D(1, 1));

	FLinearColor GlowColor = WallColor.ToLinearColor() * 2.0f;  // Bright glow
	GlowColors.Add(GlowColor);
	GlowColors.Add(GlowColor);
	GlowColors.Add(GlowColor);
	GlowColors.Add(GlowColor);

	TopGlowMesh->CreateMeshSection_LinearColor(0, GlowVerts, GlowTris, GlowNormals, GlowUVs, GlowColors, TArray<FProcMeshTangent>(), false);
}

void AArmaWall::GenerateWallQuad(
	TArray<FVector>& Vertices,
	TArray<int32>& Triangles,
	TArray<FVector>& Normals,
	TArray<FVector2D>& UVs,
	TArray<FLinearColor>& Colors,
	const FArmaCoord& Start,
	const FArmaCoord& End,
	float Height,
	float Thickness)
{
	FArmaCoord WallVec = End - Start;
	float Length = WallVec.Norm();
	FArmaCoord DirNorm = WallVec.Normalized();
	FArmaCoord Perp = DirNorm.Turn(1) * (Thickness * 0.5f);

	int32 BaseIndex = Vertices.Num();

	// Front face vertices (facing perpendicular to wall direction)
	FVector BL(Start.X + Perp.X, Start.Y + Perp.Y, 0.0f);
	FVector BR(Start.X - Perp.X, Start.Y - Perp.Y, 0.0f);
	FVector TL(Start.X + Perp.X, Start.Y + Perp.Y, Height);
	FVector TR(Start.X - Perp.X, Start.Y - Perp.Y, Height);

	FVector BLEnd(End.X + Perp.X, End.Y + Perp.Y, 0.0f);
	FVector BREnd(End.X - Perp.X, End.Y - Perp.Y, 0.0f);
	FVector TLEnd(End.X + Perp.X, End.Y + Perp.Y, Height);
	FVector TREnd(End.X - Perp.X, End.Y - Perp.Y, Height);

	// Side 1 (left side of wall)
	Vertices.Add(BL);  // 0
	Vertices.Add(TL);  // 1
	Vertices.Add(BLEnd);  // 2
	Vertices.Add(TLEnd);  // 3

	FVector NormalLeft(Perp.X, Perp.Y, 0.0f);
	NormalLeft.Normalize();
	Normals.Add(NormalLeft);
	Normals.Add(NormalLeft);
	Normals.Add(NormalLeft);
	Normals.Add(NormalLeft);

	// Side 2 (right side of wall)
	Vertices.Add(BR);  // 4
	Vertices.Add(TR);  // 5
	Vertices.Add(BREnd);  // 6
	Vertices.Add(TREnd);  // 7

	FVector NormalRight(-Perp.X, -Perp.Y, 0.0f);
	NormalRight.Normalize();
	Normals.Add(NormalRight);
	Normals.Add(NormalRight);
	Normals.Add(NormalRight);
	Normals.Add(NormalRight);

	// UVs
	float UVLength = Length / Height;
	UVs.Add(FVector2D(0, 1));
	UVs.Add(FVector2D(0, 0));
	UVs.Add(FVector2D(UVLength, 1));
	UVs.Add(FVector2D(UVLength, 0));
	UVs.Add(FVector2D(0, 1));
	UVs.Add(FVector2D(0, 0));
	UVs.Add(FVector2D(UVLength, 1));
	UVs.Add(FVector2D(UVLength, 0));

	// Colors
	FLinearColor WallLinearColor = WallColor.ToLinearColor();
	for (int32 i = 0; i < 8; i++)
	{
		Colors.Add(WallLinearColor);
	}

	// Triangles for side 1
	Triangles.Add(BaseIndex + 0);
	Triangles.Add(BaseIndex + 1);
	Triangles.Add(BaseIndex + 2);
	Triangles.Add(BaseIndex + 2);
	Triangles.Add(BaseIndex + 1);
	Triangles.Add(BaseIndex + 3);

	// Triangles for side 2
	Triangles.Add(BaseIndex + 4);
	Triangles.Add(BaseIndex + 6);
	Triangles.Add(BaseIndex + 5);
	Triangles.Add(BaseIndex + 5);
	Triangles.Add(BaseIndex + 6);
	Triangles.Add(BaseIndex + 7);
}

//////////////////////////////////////////////////////////////////////////
// AArmaWallRim Implementation
//////////////////////////////////////////////////////////////////////////

AArmaWallRim::AArmaWallRim()
{
	PrimaryActorTick.bCanEverTick = false;

	RimMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RimMesh"));
	RootComponent = RimMesh;
	RimMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RimMesh->SetCollisionProfileName(TEXT("BlockAll"));

	RimHeight = 10000.0f;
	TextureBegin = 0.0f;
	TextureEnd = 1.0f;
	RimMaterial = nullptr;
}

void AArmaWallRim::BeginPlay()
{
	Super::BeginPlay();
	
	// Create material for rim mesh using BasicShapeMaterial
	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, 
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!BaseMaterial)
	{
		BaseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	if (BaseMaterial)
	{
		RimMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (RimMaterial)
		{
			// Dark rim color matching the vertex colors
			RimMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.1f, 0.15f, 1.0f));
			RimMesh->SetMaterial(0, RimMaterial);
		}
	}
}

void AArmaWallRim::Initialize(const FArmaCoord& Start, const FArmaCoord& End, float Height)
{
	StartPoint = Start;
	EndPoint = End;
	RimHeight = Height;

	GenerateRimMesh();
}

void AArmaWallRim::GenerateRimMesh()
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;

	FArmaCoord WallVec = EndPoint - StartPoint;
	float Length = WallVec.Norm();
	FArmaCoord DirNorm = WallVec.Normalized();
	FArmaCoord Normal2D = DirNorm.Turn(1);  // Pointing inward

	// Simple quad for rim wall
	Vertices.Add(FVector(StartPoint.X, StartPoint.Y, 0.0f));
	Vertices.Add(FVector(StartPoint.X, StartPoint.Y, RimHeight));
	Vertices.Add(FVector(EndPoint.X, EndPoint.Y, 0.0f));
	Vertices.Add(FVector(EndPoint.X, EndPoint.Y, RimHeight));

	FVector Normal3D(Normal2D.X, Normal2D.Y, 0.0f);
	Normals.Add(Normal3D);
	Normals.Add(Normal3D);
	Normals.Add(Normal3D);
	Normals.Add(Normal3D);

	float UVLength = Length / 100.0f;  // Tile every 100 units
	UVs.Add(FVector2D(TextureBegin, 1));
	UVs.Add(FVector2D(TextureBegin, 0));
	UVs.Add(FVector2D(TextureEnd, 1));
	UVs.Add(FVector2D(TextureEnd, 0));

	// Dark rim color
	FLinearColor RimColor(0.1f, 0.1f, 0.15f, 1.0f);
	Colors.Add(RimColor);
	Colors.Add(RimColor);
	Colors.Add(RimColor);
	Colors.Add(RimColor);

	Triangles.Add(0);
	Triangles.Add(1);
	Triangles.Add(2);
	Triangles.Add(2);
	Triangles.Add(1);
	Triangles.Add(3);

	RimMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), true);
}

