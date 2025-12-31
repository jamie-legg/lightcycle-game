// ArmaGrid.cpp - Implementation of grid and arena systems

#include "ArmaGrid.h"
#include "Game/ArmaWall.h"
#include "Game/ArmaCycle.h"
#include "Kismet/GameplayStatics.h"

// Static member initialization
float AArmaArena::SizeMultiplier = 1.0f;

//////////////////////////////////////////////////////////////////////////
// UArmaGridSubsystem Implementation
//////////////////////////////////////////////////////////////////////////

UArmaGridSubsystem::UArmaGridSubsystem()
	: GridSize(ArmaPhysics::DefaultArenaSize)
{
}

void UArmaGridSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Grid will be created when arena is prepared
}

void UArmaGridSubsystem::Deinitialize()
{
	ClearGrid();
	Super::Deinitialize();
}

void UArmaGridSubsystem::CreateGrid(float Size)
{
	ClearGrid();
	GridSize = Size;

	// Create the initial triangulated grid covering the arena
	// This is a simplified version - original uses Delaunay triangulation
	
	// Create corner points
	float HalfSize = Size * 0.5f;
	int32 P0 = AddPoint(FArmaCoord(-HalfSize, -HalfSize));
	int32 P1 = AddPoint(FArmaCoord(HalfSize, -HalfSize));
	int32 P2 = AddPoint(FArmaCoord(HalfSize, HalfSize));
	int32 P3 = AddPoint(FArmaCoord(-HalfSize, HalfSize));

	// Create edges for the perimeter
	int32 E01 = AddEdgePair(P0, P1);
	int32 E12 = AddEdgePair(P1, P2);
	int32 E23 = AddEdgePair(P2, P3);
	int32 E30 = AddEdgePair(P3, P0);

	// Create diagonal
	int32 E02 = AddEdgePair(P0, P2);

	// Create two triangular faces
	// Face 1: P0-P1-P2
	ConnectEdges(E01, E12);
	ConnectEdges(E12, Edges[E02].TwinID);
	ConnectEdges(Edges[E02].TwinID, E01);
	int32 F0 = AddFace(E01);

	// Face 2: P0-P2-P3
	ConnectEdges(E02, E23);
	ConnectEdges(E23, E30);
	ConnectEdges(E30, E02);
	int32 F1 = AddFace(E02);
}

void UArmaGridSubsystem::ClearGrid()
{
	Points.Empty();
	Edges.Empty();
	Faces.Empty();
}

int32 UArmaGridSubsystem::FindSurroundingFace(const FArmaCoord& Coord, int32 StartFaceID) const
{
	// If no start face provided, use first face or return -1 if no faces
	if (Faces.Num() == 0)
		return -1;

	int32 CurrentFace = (StartFaceID >= 0 && StartFaceID < Faces.Num()) ? StartFaceID : 0;

	// Walk through faces to find containing face
	// This is a simplified version of the original's walking algorithm
	const int32 MaxIterations = Faces.Num() * 2;
	for (int32 i = 0; i < MaxIterations; i++)
	{
		if (IsPointInFace(Coord, CurrentFace))
			return CurrentFace;

		// Find which edge to cross
		const FArmaGridFace& Face = Faces[CurrentFace];
		int32 EdgeID = Face.EdgeID;
		int32 FirstEdge = EdgeID;
		
		bool FoundCrossing = false;
		do
		{
			const FArmaGridHalfEdge& Edge = Edges[EdgeID];
			const FArmaGridPoint& P1 = Points[Edge.PointID];
			const FArmaGridHalfEdge& NextEdge = Edges[Edge.NextID];
			const FArmaGridPoint& P2 = Points[NextEdge.PointID];

			// Check if point is on the outside of this edge
			FArmaCoord EdgeVec = P2.Position - P1.Position;
			FArmaCoord ToPoint = Coord - P1.Position;
			
			if (EdgeVec.Cross(ToPoint) < 0)
			{
				// Cross this edge to adjacent face
				int32 TwinID = Edge.TwinID;
				if (TwinID >= 0 && Edges[TwinID].FaceID >= 0)
				{
					CurrentFace = Edges[TwinID].FaceID;
					FoundCrossing = true;
					break;
				}
			}

			EdgeID = Edge.NextID;
		}
		while (EdgeID != FirstEdge);

		if (!FoundCrossing)
		{
			// Couldn't find crossing, return current best guess
			return CurrentFace;
		}
	}

	return CurrentFace;
}

int32 UArmaGridSubsystem::DrawLine(int32 StartPointID, const FArmaCoord& End, AArmaWall* Wall)
{
	if (!Points.IsValidIndex(StartPointID))
		return -1;

	// Insert end point
	int32 EndPointID = InsertPoint(End);
	if (EndPointID < 0)
		return -1;

	// Create edge between start and end
	int32 NewEdgeID = AddEdgePair(StartPointID, EndPointID);

	// Associate wall with edge
	if (Wall && Edges.IsValidIndex(NewEdgeID))
	{
		Edges[NewEdgeID].Wall = Wall;
		if (Edges[NewEdgeID].TwinID >= 0)
		{
			Edges[Edges[NewEdgeID].TwinID].Wall = Wall;
		}
	}

	// TODO: Full implementation would split existing edges and update face connectivity
	// This simplified version just adds the edge without proper triangulation

	return EndPointID;
}

int32 UArmaGridSubsystem::InsertPoint(const FArmaCoord& Coord, int32 GuessFaceID)
{
	// Find containing face
	int32 FaceID = FindSurroundingFace(Coord, GuessFaceID);
	
	// Add the point
	int32 NewPointID = AddPoint(Coord);

	// TODO: Full implementation would split face into triangles around new point
	// This is a simplified version

	return NewPointID;
}

void UArmaGridSubsystem::ProcessWallsInRange(const FArmaCoord& Pos, float Range, TFunctionRef<void(AArmaWall*)> Processor)
{
	float RangeSq = Range * Range;

	for (const FArmaGridHalfEdge& Edge : Edges)
	{
		if (Edge.Wall.IsValid())
		{
			// Check if edge is within range (simplified - checks midpoint)
			if (Edge.PointID >= 0 && Edge.TwinID >= 0)
			{
				const FArmaGridHalfEdge& Twin = Edges[Edge.TwinID];
				if (Points.IsValidIndex(Edge.PointID) && Points.IsValidIndex(Twin.PointID))
				{
					FArmaCoord P1 = Points[Edge.PointID].Position;
					FArmaCoord P2 = Points[Twin.PointID].Position;
					FArmaCoord Mid = (P1 + P2) * 0.5f;
					
					if ((Mid - Pos).NormSquared() < RangeSq)
					{
						Processor(Edge.Wall.Get());
					}
				}
			}
		}
	}
}

FArmaGridPoint* UArmaGridSubsystem::GetPoint(int32 ID)
{
	return Points.IsValidIndex(ID) ? &Points[ID] : nullptr;
}

FArmaGridHalfEdge* UArmaGridSubsystem::GetEdge(int32 ID)
{
	return Edges.IsValidIndex(ID) ? &Edges[ID] : nullptr;
}

FArmaGridFace* UArmaGridSubsystem::GetFace(int32 ID)
{
	return Faces.IsValidIndex(ID) ? &Faces[ID] : nullptr;
}

bool UArmaGridSubsystem::IsPointInFace(const FArmaCoord& Point, int32 FaceID) const
{
	if (!Faces.IsValidIndex(FaceID))
		return false;

	const FArmaGridFace& Face = Faces[FaceID];
	int32 EdgeID = Face.EdgeID;
	if (EdgeID < 0)
		return false;

	int32 FirstEdge = EdgeID;
	do
	{
		const FArmaGridHalfEdge& Edge = Edges[EdgeID];
		const FArmaGridPoint& P1 = Points[Edge.PointID];
		const FArmaGridHalfEdge& NextEdge = Edges[Edge.NextID];
		const FArmaGridPoint& P2 = Points[NextEdge.PointID];

		FArmaCoord EdgeVec = P2.Position - P1.Position;
		FArmaCoord ToPoint = Point - P1.Position;

		// If point is on the wrong side of any edge, it's not in the face
		if (EdgeVec.Cross(ToPoint) < -KINDA_SMALL_NUMBER)
			return false;

		EdgeID = Edge.NextID;
	}
	while (EdgeID != FirstEdge && EdgeID >= 0);

	return true;
}

bool UArmaGridSubsystem::RayCast(const FArmaCoord& Start, const FArmaCoord& End, FArmaCoord& HitPoint, AArmaWall*& HitWall) const
{
	HitWall = nullptr;
	float BestT = FLT_MAX;
	bool bHit = false;

	FArmaCoord RayDir = End - Start;
	float RayLength = RayDir.Norm();
	if (RayLength < KINDA_SMALL_NUMBER)
		return false;

	FArmaCoord RayDirNorm = RayDir / RayLength;

	for (const FArmaGridHalfEdge& Edge : Edges)
	{
		if (!Edge.Wall.IsValid())
			continue;

		if (Edge.PointID < 0 || Edge.TwinID < 0)
			continue;

		const FArmaGridHalfEdge& Twin = Edges[Edge.TwinID];
		if (!Points.IsValidIndex(Edge.PointID) || !Points.IsValidIndex(Twin.PointID))
			continue;

		FArmaCoord P1 = Points[Edge.PointID].Position;
		FArmaCoord P2 = Points[Twin.PointID].Position;

		// Ray-segment intersection
		FArmaCoord SegDir = P2 - P1;
		float Cross = RayDirNorm.Cross(SegDir);

		if (FMath::Abs(Cross) < KINDA_SMALL_NUMBER)
			continue;  // Parallel

		FArmaCoord StartToP1 = P1 - Start;
		float T = StartToP1.Cross(SegDir) / Cross;
		float U = StartToP1.Cross(RayDirNorm) / Cross;

		if (T >= 0 && T <= RayLength && U >= 0 && U <= 1.0f)
		{
			if (T < BestT)
			{
				BestT = T;
				HitPoint = Start + RayDirNorm * T;
				HitWall = Edge.Wall.Get();
				bHit = true;
			}
		}
	}

	return bHit;
}

int32 UArmaGridSubsystem::AddPoint(const FArmaCoord& Coord)
{
	int32 NewID = Points.Num();
	FArmaGridPoint NewPoint(NewID, Coord);
	Points.Add(NewPoint);
	return NewID;
}

int32 UArmaGridSubsystem::AddEdgePair(int32 Point1ID, int32 Point2ID)
{
	int32 Edge1ID = Edges.Num();
	int32 Edge2ID = Edge1ID + 1;

	FArmaGridHalfEdge Edge1;
	Edge1.ID = Edge1ID;
	Edge1.PointID = Point1ID;
	Edge1.TwinID = Edge2ID;

	FArmaGridHalfEdge Edge2;
	Edge2.ID = Edge2ID;
	Edge2.PointID = Point2ID;
	Edge2.TwinID = Edge1ID;

	Edges.Add(Edge1);
	Edges.Add(Edge2);

	// Add edges to point lists
	if (Points.IsValidIndex(Point1ID))
		Points[Point1ID].EdgeIDs.Add(Edge1ID);
	if (Points.IsValidIndex(Point2ID))
		Points[Point2ID].EdgeIDs.Add(Edge2ID);

	return Edge1ID;
}

int32 UArmaGridSubsystem::AddFace(int32 EdgeID)
{
	int32 NewID = Faces.Num();
	FArmaGridFace NewFace;
	NewFace.ID = NewID;
	NewFace.EdgeID = EdgeID;

	// Calculate center
	TArray<FArmaCoord> FacePoints;
	int32 CurrentEdge = EdgeID;
	int32 FirstEdge = EdgeID;
	do
	{
		if (Edges.IsValidIndex(CurrentEdge) && Points.IsValidIndex(Edges[CurrentEdge].PointID))
		{
			FacePoints.Add(Points[Edges[CurrentEdge].PointID].Position);
			Edges[CurrentEdge].FaceID = NewID;
			CurrentEdge = Edges[CurrentEdge].NextID;
		}
		else
			break;
	}
	while (CurrentEdge != FirstEdge && CurrentEdge >= 0);

	if (FacePoints.Num() > 0)
	{
		FArmaCoord Sum = FArmaCoord::Zero;
		for (const FArmaCoord& P : FacePoints)
			Sum += P;
		NewFace.Center = Sum / FacePoints.Num();
	}

	Faces.Add(NewFace);
	return NewID;
}

void UArmaGridSubsystem::SplitEdge(int32 EdgeID, int32 NewPointID)
{
	// TODO: Implement full edge splitting
}

void UArmaGridSubsystem::ConnectEdges(int32 Edge1ID, int32 Edge2ID)
{
	if (Edges.IsValidIndex(Edge1ID) && Edges.IsValidIndex(Edge2ID))
	{
		Edges[Edge1ID].NextID = Edge2ID;
		Edges[Edge2ID].PrevID = Edge1ID;
	}
}

int32 UArmaGridSubsystem::FindEdgeForPoint(const FArmaCoord& Point, int32 FaceID) const
{
	// TODO: Implement finding which edge a point lies on
	return -1;
}
//////////////////////////////////////////////////////////////////////////
// AArmaArena Implementation
//////////////////////////////////////////////////////////////////////////

AArmaArena::AArmaArena()
{
	PrimaryActorTick.bCanEverTick = false;
	ArenaSize = ArmaPhysics::DefaultArenaSize;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootSceneComponent;
}

void AArmaArena::BeginPlay()
{
	Super::BeginPlay();
}

void AArmaArena::PrepareArena(float InSizeMultiplier)
{
	SizeMultiplier = InSizeMultiplier;
	ArenaSize = ArmaPhysics::DefaultArenaSize * SizeMultiplier;

	// Get or create grid subsystem
	UArmaGridSubsystem* GridSubsystem = GetWorld()->GetSubsystem<UArmaGridSubsystem>();
	if (GridSubsystem)
	{
		GridSubsystem->CreateGrid(ArenaSize);
	}

	// Generate spawn points
	GenerateSpawnPoints(8);

	// Create rim walls
	CreateRimWalls();
}

FArmaCoord AArmaArena::GetRandomPosition(float Factor) const
{
	float HalfSize = ArenaSize * 0.5f * Factor;
	float X = FMath::RandRange(-HalfSize, HalfSize);
	float Y = FMath::RandRange(-HalfSize, HalfSize);
	return FArmaCoord(X, Y);
}

FArmaCoord AArmaArena::GetBestSpawnPoint() const
{
	if (SpawnPoints.Num() == 0)
		return FArmaCoord::Zero;

	// For now, return random spawn point
	// Full implementation would calculate danger levels
	int32 Index = FMath::RandRange(0, SpawnPoints.Num() - 1);
	return SpawnPoints[Index];
}

FArmaCoord AArmaArena::GetClosestSpawnPoint(const FArmaCoord& Location) const
{
	if (SpawnPoints.Num() == 0)
		return FArmaCoord::Zero;

	float BestDistSq = FLT_MAX;
	int32 BestIndex = 0;

	for (int32 i = 0; i < SpawnPoints.Num(); i++)
	{
		float DistSq = (SpawnPoints[i] - Location).NormSquared();
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = i;
		}
	}

	return SpawnPoints[BestIndex];
}

bool AArmaArena::IsInsideArena(const FArmaCoord& Position) const
{
	float HalfSize = ArenaSize * 0.5f;
	return FMath::Abs(Position.X) < HalfSize && FMath::Abs(Position.Y) < HalfSize;
}

FBox2D AArmaArena::GetArenaBounds() const
{
	float HalfSize = ArenaSize * 0.5f;
	return FBox2D(FVector2D(-HalfSize, -HalfSize), FVector2D(HalfSize, HalfSize));
}

void AArmaArena::CreateRimWalls()
{
	// Rim walls are created as part of the game mode setup
	// This would spawn AArmaWall actors around the perimeter
}

void AArmaArena::GenerateSpawnPoints(int32 NumPoints)
{
	SpawnPoints.Empty();
	SpawnDirections.Empty();

	float Radius = ArenaSize * 0.4f;  // Spawn slightly inside arena edge

	for (int32 i = 0; i < NumPoints; i++)
	{
		float Angle = (2.0f * PI * i) / NumPoints;
		float X = Radius * FMath::Cos(Angle);
		float Y = Radius * FMath::Sin(Angle);
		
		SpawnPoints.Add(FArmaCoord(X, Y));
		
		// Direction points toward center
		FArmaCoord Dir = FArmaCoord(-X, -Y).Normalized();
		SpawnDirections.Add(Dir);
	}
}

//////////////////////////////////////////////////////////////////////////
// FArmaSpawnPoint Implementation
//////////////////////////////////////////////////////////////////////////

void FArmaSpawnPoint::UpdateDanger(const UWorld* World)
{
	if (!World)
		return;

	DangerLevel = 0.0f;

	// Get all cycles and calculate danger from their walls
	TArray<AActor*> Cycles;
	UGameplayStatics::GetAllActorsOfClass(World, AArmaCycle::StaticClass(), Cycles);

	for (AActor* Actor : Cycles)
	{
		if (AArmaCycle* Cycle = Cast<AArmaCycle>(Actor))
		{
			// Distance to cycle
			FArmaCoord CyclePos(Cycle->GetActorLocation().X, Cycle->GetActorLocation().Y);
			float Dist = (CyclePos - Location).Norm();
			
			// Closer = more dangerous
			if (Dist < 200.0f)
			{
				DangerLevel += (200.0f - Dist) / 200.0f;
			}
		}
	}
}


