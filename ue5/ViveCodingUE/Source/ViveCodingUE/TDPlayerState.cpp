// Fill out your copyright notice in the Description page of Project Settings.

#include "TDPlayerState.h"
#include "Net/UnrealNetwork.h"

ATDPlayerState::ATDPlayerState()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATDPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATDPlayerState, Gold);
	DOREPLIFETIME(ATDPlayerState, bIsHost);
}

void ATDPlayerState::OnRep_Gold()
{
	OnGoldChanged.Broadcast(Gold);
}

void ATDPlayerState::SetGold(int32 InGold)
{
	const int32 Clamped = FMath::Max(0, InGold);
	if (Gold == Clamped)
	{
		return;
	}

	Gold = Clamped;
	ForceNetUpdate();
	OnRep_Gold();
}

void ATDPlayerState::AddGold(int32 Delta)
{
	SetGold(Gold + Delta);
}
