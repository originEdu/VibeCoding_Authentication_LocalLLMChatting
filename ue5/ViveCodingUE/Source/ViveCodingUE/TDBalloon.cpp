// Fill out your copyright notice in the Description page of Project Settings.

#include "TDBalloon.h"
#include "TDGameMode.h"
#include "TDGameState.h"
#include "TDLane.h"
#include "TDTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** 이 이상 벌어지면 이징 대신 즉시 스냅한다(cm). */
	constexpr float BalloonSnapThreshold = 50.0f;

	/** 보정 이징 시간(초). */
	constexpr float BalloonCorrectionTime = 0.2f;
}

ATDBalloon::ATDBalloon()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(false);

	// 연결 4개, 맵 ±4000 규모에서는 거리 relevancy 검사가 아끼는 대역폭보다 CPU 를 더 쓴다.
	// 탑다운 뷰 타깃이 보고 있는 풍선과 멀 수도 있어서 거리 컬링은 오히려 위험하다.
	// 맵이 커지면 여기가 제일 먼저 재검토할 지점이다.
	bAlwaysRelevant = true;

	SetNetUpdateFrequency(1.0f);
	SetMinNetUpdateFrequency(1.0f);

	BalloonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BalloonMesh"));
	SetRootComponent(BalloonMesh);
	BalloonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATDBalloon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ATDBalloon, LaneIndex, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATDBalloon, Tier, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATDBalloon, Speed, COND_InitialOnly);
	DOREPLIFETIME(ATDBalloon, DistanceAlongPath);
}

void ATDBalloon::BeginPlay()
{
	Super::BeginPlay();

	ResolveLane();
	ApplyTierMaterial();
	LocalDistance = DistanceAlongPath;
}

void ATDBalloon::InitFromWave(uint8 InLaneIndex, uint8 InTier, float InSpeed, float InHealth)
{
	LaneIndex = InLaneIndex;
	Tier = InTier;
	Speed = InSpeed;
	Health = InHealth;
}

void ATDBalloon::ResolveLane()
{
	const ATDGameState* GS = GetWorld()->GetGameState<ATDGameState>();
	if (!GS)
	{
		UE_LOG(LogTD, Error, TEXT("[Balloon] no ATDGameState - balloon cannot resolve its lane"));
		return;
	}

	const TArray<TObjectPtr<ATDLane>>& Lanes = GS->GetLanes();
	if (!Lanes.IsValidIndex(LaneIndex))
	{
		UE_LOG(LogTD, Error, TEXT("[Balloon] LaneIndex %d out of range (lanes=%d)"), LaneIndex, Lanes.Num());
		return;
	}

	Lane = Lanes[LaneIndex];
}

void ATDBalloon::ApplyTierMaterial()
{
	if (TierMaterials.IsValidIndex(Tier) && TierMaterials[Tier])
	{
		BalloonMesh->SetMaterial(0, TierMaterials[Tier]);
	}
}

void ATDBalloon::OnRep_Tier()
{
	ApplyTierMaterial();
}

void ATDBalloon::OnRep_Distance()
{
	// 큰 차이는 즉시 스냅, 작은 차이는 오차로 쌓아두고 Tick 이 조금씩 흡수한다.
	// 속도 블렌딩은 하지 않는다 - 자유도가 1인 이동에 예측기를 얹을 이유가 없다.
	const float Error = DistanceAlongPath - LocalDistance;
	if (FMath::Abs(Error) > BalloonSnapThreshold)
	{
		LocalDistance = DistanceAlongPath;
		PendingError = 0.0f;
	}
	else
	{
		PendingError = Error;
	}
}

void ATDBalloon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Lane)
	{
		return;
	}

	if (HasAuthority())
	{
		DistanceAlongPath += Speed * DeltaSeconds;

		if (DistanceAlongPath >= Lane->GetLength())
		{
			if (ATDGameMode* GM = GetWorld()->GetAuthGameMode<ATDGameMode>())
			{
				GM->OnBalloonRemoved(this, /*bReachedCore=*/true, /*Killer=*/nullptr);
			}
			Destroy();
			return;
		}

		SetActorLocation(Lane->GetLocationAtDistance(DistanceAlongPath), /*bSweep=*/false);
		return;
	}

	// 시뮬레이티드 프록시: 서버와 같은 식을 로컬 적분하고, OnRep 이 남긴 오차만 나눠서 흡수한다.
	LocalDistance += Speed * DeltaSeconds;

	if (!FMath::IsNearlyZero(PendingError))
	{
		const float Step = PendingError * FMath::Min(1.0f, DeltaSeconds / BalloonCorrectionTime);
		LocalDistance += Step;
		PendingError -= Step;
	}

	SetActorLocation(Lane->GetLocationAtDistance(LocalDistance), /*bSweep=*/false);
}

void ATDBalloon::ApplyDamage(float Amount, ATDPlayerState* Killer)
{
	if (!HasAuthority())
	{
		return;
	}

	Health -= Amount;
	if (Health > 0.0f)
	{
		return;
	}

	if (ATDGameMode* GM = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		GM->OnBalloonRemoved(this, /*bReachedCore=*/false, Instigator);
	}
	Destroy();
}
