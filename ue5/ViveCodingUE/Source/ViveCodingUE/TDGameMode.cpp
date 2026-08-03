// Fill out your copyright notice in the Description page of Project Settings.

#include "TDGameMode.h"
#include "TDBalloon.h"
#include "TDCore.h"
#include "TDGameState.h"
#include "TDLane.h"
#include "TDPlayerController.h"
#include "TDPlayerState.h"
#include "TDTower.h"
#include "Engine/DataTable.h"

ATDGameMode::ATDGameMode()
{
	PrimaryActorTick.bCanEverTick = false;

	GameStateClass = ATDGameState::StaticClass();
	PlayerStateClass = ATDPlayerState::StaticClass();
	PlayerControllerClass = ATDPlayerController::StaticClass();
}

void ATDGameMode::BeginPlay()
{
	Super::BeginPlay();

	TDGameState = GetGameState<ATDGameState>();
	if (!TDGameState)
	{
		UE_LOG(LogTD, Error, TEXT("[GameMode] GameStateClass is not ATDGameState - match cannot run"));
		return;
	}

	if (const ATDCore* Core = TDGameState->GetCore())
	{
		TDGameState->InitCoreHealth(Core->MaxHealth);
	}
	else
	{
		UE_LOG(LogTD, Error, TEXT("[GameMode] no ATDCore placed in the level"));
	}

	// 첫 웨이브도 방장의 Start 를 기다린다. 카운트다운은 없다.
	TDGameState->SetPhase(ETDPhase::Build);
	TDGameState->SetWaveNumber(0);

	UE_LOG(LogTD, Log, TEXT("[GameMode] begin: phase=Build coreHP=%d lanes=%d"),
		TDGameState->GetCoreHealth(), TDGameState->GetLanes().Num());
}

void ATDGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ATDPlayerState* PS = NewPlayer ? NewPlayer->GetPlayerState<ATDPlayerState>() : nullptr;
	if (!PS)
	{
		return;
	}

	PS->SetGold(StartingGold);

	// 리슨 서버에서 원격 클라의 컨트롤러는 절대 로컬이 아니므로 이 판정은 명확하다.
	if (!HostPlayerState && NewPlayer->IsLocalPlayerController())
	{
		PS->SetIsHost(true);
		HostPlayerState = PS;
	}

	UE_LOG(LogTD, Log, TEXT("[GameMode] PostLogin %s host=%d gold=%d"),
		*PS->GetPlayerName(), PS->IsHost() ? 1 : 0, PS->GetGold());
}

void ATDGameMode::Logout(AController* Exiting)
{
	if (const APlayerController* PC = Cast<APlayerController>(Exiting))
	{
		if (ATDPlayerState* PS = PC->GetPlayerState<ATDPlayerState>())
		{
			if (PS == HostPlayerState)
			{
				UE_LOG(LogTD, Warning, TEXT("[GameMode] host %s left - no one can start waves"), *PS->GetPlayerName());
				HostPlayerState = nullptr;
			}
		}
	}

	Super::Logout(Exiting);
}

bool ATDGameMode::GetWaveRow(int32 WaveNumber, FTDWaveRow& OutRow) const
{
	if (!WaveTable)
	{
		return false;
	}

	TArray<FTDWaveRow*> Rows;
	WaveTable->GetAllRows<FTDWaveRow>(TEXT("TDGameMode::GetWaveRow"), Rows);
	if (!Rows.IsValidIndex(WaveNumber - 1) || !Rows[WaveNumber - 1])
	{
		return false;
	}

	OutRow = *Rows[WaveNumber - 1];
	return true;
}

void ATDGameMode::RequestStartWave(ATDPlayerController* Requester)
{
	ATDPlayerState* PS = Requester ? Requester->GetPlayerState<ATDPlayerState>() : nullptr;
	const FString Name = PS ? PS->GetPlayerName() : TEXT("<unknown>");

	if (!TDGameState || TDGameState->GetPhase() != ETDPhase::Build)
	{
		UE_LOG(LogTD, Warning, TEXT("[GameMode] start denied (wrong phase) by %s"), *Name);
		return;
	}

	if (!PS || PS != HostPlayerState)
	{
		UE_LOG(LogTD, Warning, TEXT("[GameMode] start denied (not host) by %s"), *Name);
		return;
	}

	StartWave();
}

void ATDGameMode::StartWave()
{
	const int32 NextWave = TDGameState->GetWaveNumber() + 1;

	if (!GetWaveRow(NextWave, CurrentWave))
	{
		EnterVictory();
		return;
	}

	const int32 LaneCount = TDGameState->GetLanes().Num();
	if (LaneCount == 0 || !BalloonClass)
	{
		UE_LOG(LogTD, Error, TEXT("[GameMode] cannot start wave: lanes=%d balloonClass=%s"),
			LaneCount, BalloonClass ? TEXT("set") : TEXT("null"));
		return;
	}

	TDGameState->SetWaveNumber(NextWave);
	TDGameState->SetPhase(ETDPhase::Wave);

	RemainingSpawnRounds = CurrentWave.CountPerLane;
	PendingSpawns = CurrentWave.CountPerLane * LaneCount;
	UpdateBalloonsRemaining();

	UE_LOG(LogTD, Log, TEXT("[GameMode] wave %d start: %d balloons (%d per lane x %d lanes)"),
		NextWave, PendingSpawns, CurrentWave.CountPerLane, LaneCount);

	// 첫 라운드는 즉시, 이후는 SpawnInterval 마다.
	SpawnRound();
	if (RemainingSpawnRounds > 0)
	{
		GetWorldTimerManager().SetTimer(
			SpawnTimer, this, &ATDGameMode::SpawnRound, CurrentWave.SpawnInterval, /*bLoop=*/true);
	}
}

void ATDGameMode::SpawnRound()
{
	const TArray<TObjectPtr<ATDLane>>& Lanes = TDGameState->GetLanes();

	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		ATDLane* Lane = Lanes[LaneIndex];
		if (!Lane)
		{
			continue;
		}

		ATDBalloon* Balloon = GetWorld()->SpawnActorDeferred<ATDBalloon>(
			BalloonClass, FTransform(Lane->GetLocationAtDistance(0.0f)), nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Balloon)
		{
			continue;
		}

		Balloon->InitFromWave(
			static_cast<uint8>(LaneIndex), CurrentWave.Tier, CurrentWave.BalloonSpeed, CurrentWave.BalloonHealth);
		Balloon->FinishSpawning(FTransform(Lane->GetLocationAtDistance(0.0f)));

		ActiveBalloons.Add(Balloon);
		--PendingSpawns;
	}

	--RemainingSpawnRounds;
	UpdateBalloonsRemaining();

	if (RemainingSpawnRounds <= 0)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimer);
	}
}

void ATDGameMode::OnBalloonRemoved(ATDBalloon* Balloon, bool bReachedCore, ATDPlayerState* Killer)
{
	ActiveBalloons.Remove(Balloon);

	if (Killer)
	{
		Killer->AddGold(CurrentWave.Reward);
	}

	if (bReachedCore && TDGameState)
	{
		TDGameState->SetCoreHealth(TDGameState->GetCoreHealth() - CurrentWave.CoreDamage);
		UE_LOG(LogTD, Log, TEXT("[GameMode] core hit: HP %d/%d"),
			TDGameState->GetCoreHealth(), TDGameState->GetCoreMaxHealth());

		if (TDGameState->GetCoreHealth() <= 0)
		{
			EnterDefeat();
			return;
		}
	}

	UpdateBalloonsRemaining();

	if (TDGameState && TDGameState->GetPhase() == ETDPhase::Wave
		&& PendingSpawns == 0 && ActiveBalloons.Num() == 0)
	{
		FinishWave();
	}
}

void ATDGameMode::FinishWave()
{
	UE_LOG(LogTD, Log, TEXT("[GameMode] wave %d cleared"), TDGameState->GetWaveNumber());

	FTDWaveRow Unused;
	if (!GetWaveRow(TDGameState->GetWaveNumber() + 1, Unused))
	{
		EnterVictory();
		return;
	}

	BeginBuildPhase();
}

void ATDGameMode::BeginBuildPhase()
{
	TDGameState->SetPhase(ETDPhase::Build);
	UE_LOG(LogTD, Log, TEXT("[GameMode] phase -> Build (waiting for host start)"));
}

void ATDGameMode::EnterDefeat()
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	PendingSpawns = 0;
	RemainingSpawnRounds = 0;

	for (const TObjectPtr<ATDBalloon>& Balloon : ActiveBalloons)
	{
		if (Balloon)
		{
			Balloon->Destroy();
		}
	}
	ActiveBalloons.Reset();
	UpdateBalloonsRemaining();

	TDGameState->SetPhase(ETDPhase::Defeat);
	UE_LOG(LogTD, Log, TEXT("[GameMode] phase -> Defeat at wave %d"), TDGameState->GetWaveNumber());
}

void ATDGameMode::EnterVictory()
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	TDGameState->SetPhase(ETDPhase::Victory);
	UE_LOG(LogTD, Log, TEXT("[GameMode] phase -> Victory after wave %d"), TDGameState->GetWaveNumber());
}

void ATDGameMode::UpdateBalloonsRemaining()
{
	if (TDGameState)
	{
		TDGameState->SetBalloonsRemaining(PendingSpawns + ActiveBalloons.Num());
	}
}

ETDPlaceResult ATDGameMode::RequestPlaceTower(
	ATDPlayerController* Requester, TSubclassOf<ATDTower> TowerClass, const FVector& RequestedLocation)
{
	ATDPlayerState* PS = Requester ? Requester->GetPlayerState<ATDPlayerState>() : nullptr;
	const FString Name = PS ? PS->GetPlayerName() : TEXT("<unknown>");

	auto Deny = [&Name](ETDPlaceResult Result, const TCHAR* Reason)
	{
		UE_LOG(LogTD, Warning, TEXT("[GameMode] place denied (%s) by %s"), Reason, *Name);
		return Result;
	};

	if (!PS)
	{
		return ETDPlaceResult::WrongPhase;
	}

	if (!TDGameState || TDGameState->GetPhase() != ETDPhase::Build)
	{
		return Deny(ETDPlaceResult::WrongPhase, TEXT("WrongPhase"));
	}

	if (!AllowedTowerClasses.Contains(TowerClass))
	{
		return Deny(ETDPlaceResult::IllegalClass, TEXT("IllegalClass"));
	}

	// Cost 와 반경은 allowlist 를 통과한 클래스의 CDO 에서 읽는다. 클라가 준 값은 쓰지 않는다.
	const ATDTower* CDO = TowerClass->GetDefaultObject<ATDTower>();
	if (PS->GetGold() < CDO->Cost)
	{
		return Deny(ETDPlaceResult::NotEnoughGold, TEXT("NotEnoughGold"));
	}

	// 클라의 그리드 계산을 믿지 않고 서버에서 다시 스냅한다.
	const FVector Snapped = ATDTower::SnapToGrid(RequestedLocation);
	const ETDPlaceResult Check = ATDTower::CheckPlacement(GetWorld(), Snapped, CDO->PlacementRadius);
	if (Check != ETDPlaceResult::Allowed)
	{
		return Deny(Check, TEXT("CheckPlacement"));
	}

	FActorSpawnParameters Params;
	Params.Owner = Requester;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATDTower* Tower = GetWorld()->SpawnActor<ATDTower>(TowerClass, FTransform(Snapped), Params);
	if (!Tower)
	{
		return Deny(ETDPlaceResult::Occupied, TEXT("SpawnFailed"));
	}

	Tower->SetOwningPlayerState(PS);
	Tower->ForceNetUpdate();
	Towers.Add(Tower);

	PS->AddGold(-CDO->Cost);

	UE_LOG(LogTD, Log, TEXT("[GameMode] %s placed tower at %s, gold %d"),
		*Name, *Snapped.ToCompactString(), PS->GetGold());

	return ETDPlaceResult::Allowed;
}
