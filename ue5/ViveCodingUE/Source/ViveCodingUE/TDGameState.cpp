// Fill out your copyright notice in the Description page of Project Settings.

#include "TDGameState.h"
#include "TDCore.h"
#include "TDLane.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ATDGameState::ATDGameState()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATDGameState::BeginPlay()
{
	Super::BeginPlay();

	// 레벨 액터는 어떤 BeginPlay 보다 먼저 전부 스폰되고, 클라에서는 레벨 로드 후에 GameState 가
	// 도착하므로 여기서 한 번 훑는 것으로 충분하다.
	for (TActorIterator<ATDLane> It(GetWorld()); It; ++It)
	{
		Lanes.Add(*It);
	}
	Lanes.Sort([](const ATDLane& A, const ATDLane& B) { return A.LaneIndex < B.LaneIndex; });

	for (TActorIterator<ATDCore> It(GetWorld()); It; ++It)
	{
		Core = *It;
		break;
	}

	UE_LOG(LogTD, Log, TEXT("[GameState] lanes=%d core=%s netmode=%d"),
		Lanes.Num(), Core ? TEXT("yes") : TEXT("MISSING"), static_cast<int32>(GetNetMode()));
}

void ATDGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATDGameState, Phase);
	DOREPLIFETIME(ATDGameState, WaveNumber);
	DOREPLIFETIME(ATDGameState, CoreHealth);
	DOREPLIFETIME_CONDITION(ATDGameState, CoreMaxHealth, COND_InitialOnly);
	DOREPLIFETIME(ATDGameState, BalloonsRemaining);
}

void ATDGameState::OnRep_Phase()
{
	OnPhaseChanged.Broadcast(Phase);
}

void ATDGameState::OnRep_CoreHealth()
{
	OnCoreHealthChanged.Broadcast(CoreHealth, CoreMaxHealth);
}

void ATDGameState::SetPhase(ETDPhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}

	Phase = NewPhase;
	ForceNetUpdate();
	OnRep_Phase();
}

void ATDGameState::SetWaveNumber(int32 InWaveNumber)
{
	WaveNumber = InWaveNumber;
	ForceNetUpdate();
}

void ATDGameState::InitCoreHealth(int32 InMaxHealth)
{
	CoreMaxHealth = InMaxHealth;
	CoreHealth = InMaxHealth;
	OnRep_CoreHealth();
}

void ATDGameState::SetCoreHealth(int32 InHealth)
{
	const int32 Clamped = FMath::Max(0, InHealth);
	if (CoreHealth == Clamped)
	{
		return;
	}

	CoreHealth = Clamped;
	ForceNetUpdate();
	OnRep_CoreHealth();
}

void ATDGameState::SetBalloonsRemaining(int32 InRemaining)
{
	if (BalloonsRemaining == InRemaining)
	{
		return;
	}

	BalloonsRemaining = InRemaining;
}
