// Fill out your copyright notice in the Description page of Project Settings.

#include "TDTower.h"
#include "TDBalloon.h"
#include "TDCore.h"
#include "TDGameMode.h"
#include "TDGameState.h"
#include "TDLane.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ATDTower::ATDTower()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(5.0f);
	SetMinNetUpdateFrequency(2.0f);

	TowerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	SetRootComponent(TowerMesh);
	// 배치 시 Occupied 검사가 이 콜리전에 의존한다.
	TowerMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TowerMesh->SetCollisionObjectType(ECC_WorldStatic);

	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(TowerMesh);
	TurretMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATDTower::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ATDTower, OwningPlayerState, COND_InitialOnly);
	DOREPLIFETIME(ATDTower, CurrentTarget);
}

FVector ATDTower::SnapToGrid(const FVector& WorldLocation)
{
	return FVector(
		FMath::GridSnap(WorldLocation.X, GridSize),
		FMath::GridSnap(WorldLocation.Y, GridSize),
		0.0f);
}

ETDPlaceResult ATDTower::CheckPlacement(UWorld* World, const FVector& SnappedLocation, float TowerRadius)
{
	if (FMath::Abs(SnappedLocation.X) > PlayableHalfExtent || FMath::Abs(SnappedLocation.Y) > PlayableHalfExtent)
	{
		return ETDPlaceResult::OutOfBounds;
	}

	const ATDGameState* GS = World->GetGameState<ATDGameState>();
	if (!GS)
	{
		return ETDPlaceResult::OutOfBounds;
	}

	for (const TObjectPtr<ATDLane>& Lane : GS->GetLanes())
	{
		if (Lane && Lane->GetDistanceToSpline(SnappedLocation) < Lane->RoadWidth * 0.5f + TowerRadius)
		{
			return ETDPlaceResult::OnLane;
		}
	}

	if (const ATDCore* Core = GS->GetCore())
	{
		if (FVector::Dist2D(Core->GetActorLocation(), SnappedLocation) < Core->Radius + TowerRadius)
		{
			return ETDPlaceResult::OnCore;
		}
	}

	// 타워가 복제 액터라서 이 질의는 서버와 클라에서 동일하게 동작한다.
	// 별도의 OccupiedCells 집합을 동기화할 필요가 없다.
	if (World->OverlapAnyTestByChannel(
			SnappedLocation, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(TowerRadius)))
	{
		return ETDPlaceResult::Occupied;
	}

	return ETDPlaceResult::Allowed;
}

ATDBalloon* ATDTower::FindTarget() const
{
	const ATDGameMode* GM = GetWorld()->GetAuthGameMode<ATDGameMode>();
	if (!GM)
	{
		return nullptr;
	}

	// 사거리 안에서 경로를 가장 많이 지난 풍선을 노린다. 코어에 가장 가까운 위협부터 처리하는
	// 고전적인 TD 규칙이고, DistanceAlongPath 가 이미 있어서 추가 비용이 0이다.
	const FVector Origin = GetActorLocation();
	const float RangeSq = Range * Range;

	ATDBalloon* Best = nullptr;
	float BestDistance = -1.0f;

	for (const TObjectPtr<ATDBalloon>& Balloon : GM->GetActiveBalloons())
	{
		if (!Balloon)
		{
			continue;
		}

		if (FVector::DistSquared(Origin, Balloon->GetActorLocation()) > RangeSq)
		{
			continue;
		}

		if (Balloon->GetDistanceAlongPath() > BestDistance)
		{
			BestDistance = Balloon->GetDistanceAlongPath();
			Best = Balloon;
		}
	}

	return Best;
}

void ATDTower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		ATDBalloon* Target = FindTarget();
		if (Target != CurrentTarget)
		{
			CurrentTarget = Target;
			ForceNetUpdate();
		}

		FireCooldown = FMath::Max(0.0f, FireCooldown - DeltaSeconds);
		if (CurrentTarget && FireCooldown <= 0.0f)
		{
			FireCooldown = FireInterval;
			CurrentTarget->ApplyDamage(Damage, OwningPlayerState);
		}
	}

	// 조준은 서버/클라 모두 로컬로 한다. 복제되는 건 타겟 포인터뿐이다.
	if (TurretMesh && CurrentTarget)
	{
		const FVector ToTarget = CurrentTarget->GetActorLocation() - TurretMesh->GetComponentLocation();
		TurretMesh->SetWorldRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}
}
