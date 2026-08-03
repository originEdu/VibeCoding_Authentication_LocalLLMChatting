// Fill out your copyright notice in the Description page of Project Settings.

#include "TDLane.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"

ATDLane::ATDLane()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	LaneSpline = CreateDefaultSubobject<USplineComponent>(TEXT("LaneSpline"));
	SetRootComponent(LaneSpline);
}

void ATDLane::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!RoadMesh)
	{
		return;
	}

	// 컨스트럭션 스크립트로 만든 컴포넌트는 재실행 시 엔진이 자동으로 파괴/재생성하므로
	// 수동 정리가 필요 없다.
	const int32 SegmentCount = LaneSpline->GetNumberOfSplinePoints() - 1;
	for (int32 i = 0; i < SegmentCount; ++i)
	{
		USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(this);
		Segment->SetStaticMesh(RoadMesh);
		if (RoadMaterial)
		{
			Segment->SetMaterial(0, RoadMaterial);
		}
		Segment->SetMobility(EComponentMobility::Movable);
		Segment->SetForwardAxis(ESplineMeshAxis::X);
		Segment->AttachToComponent(LaneSpline, FAttachmentTransformRules::KeepRelativeTransform);
		Segment->RegisterComponent();

		FVector StartPos, StartTangent, EndPos, EndTangent;
		LaneSpline->GetLocationAndTangentAtSplinePoint(i, StartPos, StartTangent, ESplineCoordinateSpace::Local);
		LaneSpline->GetLocationAndTangentAtSplinePoint(i + 1, EndPos, EndTangent, ESplineCoordinateSpace::Local);
		Segment->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent);

		const FVector2D RoadScale(RoadWidth * 0.5f, RoadThickness * 0.5f);
		Segment->SetStartScale(RoadScale);
		Segment->SetEndScale(RoadScale);

		// 도로는 순전히 시각용이다. 콜리전을 켜두면 타워 배치의 Occupied 검사에 걸린다.
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

float ATDLane::GetLength() const
{
	return LaneSpline->GetSplineLength();
}

FVector ATDLane::GetLocationAtDistance(float Distance) const
{
	return LaneSpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
}

float ATDLane::GetDistanceToSpline(const FVector& WorldLocation) const
{
	const FVector Closest = LaneSpline->FindLocationClosestToWorldLocation(WorldLocation, ESplineCoordinateSpace::World);
	return FVector::Dist2D(Closest, WorldLocation);
}
