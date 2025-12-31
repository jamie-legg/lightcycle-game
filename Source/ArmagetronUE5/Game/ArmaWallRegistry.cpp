// ArmaWallRegistry.cpp - Global wall registry implementation

#include "ArmaWallRegistry.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

UArmaWallRegistry* UArmaWallRegistry::Get(UWorld* World)
{
	if (!World) return nullptr;
	return World->GetSubsystem<UArmaWallRegistry>();
}

void UArmaWallRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Warning, TEXT("ArmaWallRegistry: Initialized"));
}

void UArmaWallRegistry::Deinitialize()
{
	ClearAllWalls();
	Super::Deinitialize();
}

int32 UArmaWallRegistry::RegisterWall(FVector2D Start, FVector2D End, EArmaWallType WallType, AActor* Owner, AActor* VisualActor)
{
	float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	int32 ID = NextWallID++;
	
	FArmaRegisteredWall NewWall(Start, End, WallType, Owner, VisualActor, CurrentTime, ID);
	Walls.Add(NewWall);
	
	UE_LOG(LogTemp, Display, TEXT("Wall %d registered: (%.0f,%.0f)-(%.0f,%.0f) Type=%s Owner=%s"),
		ID, Start.X, Start.Y, End.X, End.Y,
		WallType == EArmaWallType::Rim ? TEXT("RIM") : TEXT("CYCLE"),
		Owner ? *Owner->GetName() : TEXT("None"));
	
	return ID;
}

void UArmaWallRegistry::UpdateWallEnd(int32 WallID, FVector2D NewEnd)
{
	for (FArmaRegisteredWall& Wall : Walls)
	{
		if (Wall.WallID == WallID)
		{
			// Only log significant changes (> 10 units) to avoid spam
			float Delta = (NewEnd - Wall.End).Size();
			if (Delta > 100.0f)
			{
				UE_LOG(LogTemp, Display, TEXT("Wall %d updated: (%.0f,%.0f)-(%.0f,%.0f) len=%.1f"),
					WallID, Wall.Start.X, Wall.Start.Y, NewEnd.X, NewEnd.Y,
					(NewEnd - Wall.Start).Size());
			}
			Wall.End = NewEnd;
			return;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("UpdateWallEnd: Wall ID %d not found!"), WallID);
}

void UArmaWallRegistry::RemoveWallsByOwner(AActor* Owner)
{
	for (int32 i = Walls.Num() - 1; i >= 0; i--)
	{
		if (Walls[i].OwnerActor == Owner)
		{
			if (Walls[i].VisualActor)
			{
				Walls[i].VisualActor->Destroy();
			}
			Walls.RemoveAt(i);
		}
	}
}

void UArmaWallRegistry::RemoveWall(int32 WallID)
{
	for (int32 i = 0; i < Walls.Num(); i++)
	{
		if (Walls[i].WallID == WallID)
		{
			if (Walls[i].VisualActor)
			{
				Walls[i].VisualActor->Destroy();
			}
			Walls.RemoveAt(i);
			return;
		}
	}
}

void UArmaWallRegistry::ClearAllWalls()
{
	for (FArmaRegisteredWall& Wall : Walls)
	{
		if (Wall.VisualActor)
		{
			Wall.VisualActor->Destroy();
		}
	}
	Walls.Empty();
	NextWallID = 1;
	UE_LOG(LogTemp, Warning, TEXT("ArmaWallRegistry: All walls cleared"));
}

void UArmaWallRegistry::SpawnArenaRim(float HalfWidth, float HalfHeight, float WallHeight)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Load cube mesh for walls
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!CubeMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("ArmaWallRegistry: Could not load cube mesh for rim walls"));
		return;
	}

	// Create glowing material for rim
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));

	// Define the 4 corners of the arena
	FVector2D Corners[4] = {
		FVector2D(-HalfWidth, -HalfHeight),  // Bottom-left
		FVector2D(HalfWidth, -HalfHeight),   // Bottom-right
		FVector2D(HalfWidth, HalfHeight),    // Top-right
		FVector2D(-HalfWidth, HalfHeight)    // Top-left
	};

	// Spawn 4 rim wall segments
	for (int32 i = 0; i < 4; i++)
	{
		FVector2D Start = Corners[i];
		FVector2D End = Corners[(i + 1) % 4];

		// Calculate wall visual properties
		FVector2D MidPoint = (Start + End) * 0.5f;
		float Length = (End - Start).Size();
		FVector2D Direction = (End - Start).GetSafeNormal();
		float Angle = FMath::Atan2(Direction.Y, Direction.X) * (180.0f / PI);

		// Spawn wall mesh actor
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		
		AStaticMeshActor* WallActor = World->SpawnActor<AStaticMeshActor>(
			FVector(MidPoint.X, MidPoint.Y, WallHeight * 0.5f),
			FRotator(0, Angle, 0),
			SpawnParams);

		if (WallActor)
		{
			UStaticMeshComponent* MeshComp = WallActor->GetStaticMeshComponent();
			MeshComp->SetStaticMesh(CubeMesh);
			
			// Scale: length in X, thin in Y, tall in Z
			// Cube is 100 units, so scale by length/100
			float WallThickness = 20.0f;
			MeshComp->SetWorldScale3D(FVector(Length / 100.0f, WallThickness / 100.0f, WallHeight / 100.0f));
			
			// Apply glowing red material for rim
			if (BaseMat)
			{
				UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, WallActor);
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(1.0f, 0.2f, 0.2f)); // Red tint
				MeshComp->SetMaterial(0, DynMat);
			}
			
			// No physics collision - we use 2D line checks
			MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			
#if WITH_EDITOR
			WallActor->SetActorLabel(FString::Printf(TEXT("RimWall_%d"), i));
#endif
		}

		// Register in our 2D wall system
		RegisterWall(Start, End, EArmaWallType::Rim, nullptr, WallActor);
	}

	UE_LOG(LogTemp, Warning, TEXT("ArmaWallRegistry: Spawned arena rim (%.0fx%.0f)"), HalfWidth * 2, HalfHeight * 2);
}

float UArmaWallRegistry::RaycastWalls(FVector2D Origin, FVector2D Direction, float MaxDistance,
	AActor* IgnoreOwner, float GraceTime, FArmaRegisteredWall& OutHitWall) const
{
	float ClosestDist = MAX_FLT;
	float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	
	// Normalize direction
	FVector2D NormDir = Direction.GetSafeNormal();
	
	static int DebugCounter = 0;
	bool bLogThisFrame = (DebugCounter++ % 120 == 0);  // Log every 120 calls

	if (bLogThisFrame)
	{
		UE_LOG(LogTemp, Display, TEXT("RAYCAST: Origin=(%.1f,%.1f) Dir=(%.3f,%.3f) MaxDist=%.1f NumWalls=%d"),
			Origin.X, Origin.Y, NormDir.X, NormDir.Y, MaxDistance, Walls.Num());
	}

	for (const FArmaRegisteredWall& Wall : Walls)
	{
		// Skip walls owned by the querying actor that are too new
		if (Wall.OwnerActor == IgnoreOwner && (CurrentTime - Wall.CreationTime) < GraceTime)
		{
			continue;
		}

		FVector2D SegVec = Wall.End - Wall.Start;
		float SegLength = SegVec.Size();
		
		// Skip zero-length or very short walls (less than 1 unit)
		if (SegLength < 1.0f)
		{
			continue;
		}
		
		// Wall segment direction and normal
		FVector2D WallDir = SegVec / SegLength;
		FVector2D WallNormal(-WallDir.Y, WallDir.X);  // Perpendicular
		
		// Check if we're on the "front" side of the wall (approaching it)
		// This is crucial for Armagetron-style walls where you can only collide from one side
		FVector2D ToWallStart = Wall.Start - Origin;
		float SideCheck = FVector2D::DotProduct(ToWallStart, WallNormal);
		
		// Standard line-line intersection
		// Ray: Origin + t * NormDir
		// Segment: Wall.Start + u * (Wall.End - Wall.Start)
		
		float Cross = NormDir.X * SegVec.Y - NormDir.Y * SegVec.X;
		
		if (FMath::Abs(Cross) < 0.0001f) 
		{
			// Parallel lines - check if collinear and overlapping
			// This handles the case where the ray travels along a wall
			FVector2D ToStart = Wall.Start - Origin;
			float PerpDist = FMath::Abs(ToStart.X * NormDir.Y - ToStart.Y * NormDir.X);
			if (PerpDist < 5.0f)  // Very close to the wall line
			{
				// Check if start of wall is ahead of us
				float DotToStart = FVector2D::DotProduct(ToStart, NormDir);
				if (DotToStart > 0.001f && DotToStart < MaxDistance && DotToStart < ClosestDist)
				{
					if (bLogThisFrame)
					{
						UE_LOG(LogTemp, Warning, TEXT("  HIT PARALLEL Wall %d at dist=%.1f"), Wall.WallID, DotToStart);
					}
					ClosestDist = DotToStart;
					OutHitWall = Wall;
				}
			}
			continue;
		}
		
		// Solve for t and u
		FVector2D ToStart = Wall.Start - Origin;
		float t = (ToStart.X * SegVec.Y - ToStart.Y * SegVec.X) / Cross;
		float u = (ToStart.X * NormDir.Y - ToStart.Y * NormDir.X) / Cross;
		
		// Valid intersection:
		// t > 0: intersection is in front of us
		// u in [0,1]: intersection is on the wall segment
		if (t > 0.001f && u >= 0.0f && u <= 1.0f && t < MaxDistance)
		{
			if (bLogThisFrame)
			{
				UE_LOG(LogTemp, Warning, TEXT("  HIT Wall %d at dist=%.1f (type=%s, side=%.1f, len=%.1f)"), 
					Wall.WallID, t, Wall.WallType == EArmaWallType::Rim ? TEXT("RIM") : TEXT("CYCLE"), SideCheck, SegLength);
			}
			
			if (t < ClosestDist)
			{
				ClosestDist = t;
				OutHitWall = Wall;
			}
		}
	}

	if (bLogThisFrame && ClosestDist < MAX_FLT)
	{
		UE_LOG(LogTemp, Display, TEXT("RAYCAST RESULT: ClosestDist=%.1f"), ClosestDist);
	}

	return ClosestDist;
}

float UArmaWallRegistry::GetDistanceToNearestCycleWall(FVector2D Position, FVector2D Direction, float MaxDistance, AActor* IgnoreOwner) const
{
	float ClosestDist = MaxDistance;
	float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// Cast rays to left and right (for wall acceleration)
	FVector2D LeftDir(-Direction.Y, Direction.X);
	FVector2D RightDir(Direction.Y, -Direction.X);

	for (const FArmaRegisteredWall& Wall : Walls)
	{
		// Only cycle walls provide acceleration (not rim walls)
		if (Wall.WallType != EArmaWallType::Cycle) continue;
		
		// Skip own walls that are too new
		if (Wall.OwnerActor == IgnoreOwner && (CurrentTime - Wall.CreationTime) < 0.5f) continue;

		float Dist = DistanceToSegment(Position, Wall.Start, Wall.End);
		if (Dist < ClosestDist)
		{
			ClosestDist = Dist;
		}
	}

	return ClosestDist;
}

float UArmaWallRegistry::DistanceToSegment(FVector2D Point, FVector2D SegStart, FVector2D SegEnd) const
{
	FVector2D Segment = SegEnd - SegStart;
	FVector2D PointToStart = Point - SegStart;
	
	float SegLengthSq = Segment.SizeSquared();
	if (SegLengthSq < 0.0001f)
	{
		return (Point - SegStart).Size();
	}

	float T = FMath::Clamp(FVector2D::DotProduct(PointToStart, Segment) / SegLengthSq, 0.0f, 1.0f);
	FVector2D ClosestPoint = SegStart + Segment * T;
	return (Point - ClosestPoint).Size();
}

bool UArmaWallRegistry::RaySegmentIntersection(FVector2D RayOrigin, FVector2D RayDir, FVector2D SegStart, FVector2D SegEnd, float& OutT) const
{
	// Line segment P1->P2, ray P3->P4
	FVector2D P1 = SegStart;
	FVector2D P2 = SegEnd;
	FVector2D P3 = RayOrigin;
	FVector2D P4 = RayOrigin + RayDir;

	float Denom = (P1.X - P2.X) * (P3.Y - P4.Y) - (P1.Y - P2.Y) * (P3.X - P4.X);
	if (FMath::Abs(Denom) < 0.0001f)
	{
		return false;  // Parallel
	}

	float T = ((P1.X - P3.X) * (P3.Y - P4.Y) - (P1.Y - P3.Y) * (P3.X - P4.X)) / Denom;
	float U = -((P1.X - P2.X) * (P1.Y - P3.Y) - (P1.Y - P2.Y) * (P1.X - P3.X)) / Denom;

	// T must be in [0,1] (on segment), U must be > 0 (in front of ray)
	if (T >= 0 && T <= 1 && U > 0)
	{
		OutT = U;
		return true;
	}

	return false;
}

