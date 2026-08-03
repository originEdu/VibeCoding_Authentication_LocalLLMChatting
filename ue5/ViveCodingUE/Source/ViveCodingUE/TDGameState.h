// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TDTypes.h"
#include "TDGameState.generated.h"

class ATDCore;
class ATDLane;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDPhaseChanged, ETDPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDCoreHealthChanged, int32, Current, int32, Max);

/**
 * 매치의 복제 스냅샷. HUD 가 읽는 유일한 출처다.
 *
 * 코어 HP 를 ATDCore 가 아니라 여기 두는 이유: 진실이 둘이 되는 걸 막고,
 * GameState 는 모든 커넥션에 항상 relevant 이므로 relevancy 처리가 따로 필요 없다.
 * 서버 권위이며, 쓰기는 전부 ATDGameMode 를 통해서만 한다.
 */
UCLASS()
class VIVECODINGUE_API ATDGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ATDGameState();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * LaneIndex 순으로 정렬된 레인 목록. 복제하지 않고 서버와 클라가 각자 로컬로 수집한다.
	 * 레벨 배치 액터라 모든 머신에서 동일하므로 복제할 이유가 없다.
	 */
	const TArray<TObjectPtr<ATDLane>>& GetLanes() const { return Lanes; }

	/** 레벨에 배치된 코어. 레인과 같은 이유로 복제하지 않고 각자 로컬로 찾는다. */
	ATDCore* GetCore() const { return Core; }

	UFUNCTION(BlueprintPure, Category = "TD")
	ETDPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "TD")
	int32 GetWaveNumber() const { return WaveNumber; }

	UFUNCTION(BlueprintPure, Category = "TD")
	int32 GetCoreHealth() const { return CoreHealth; }

	UFUNCTION(BlueprintPure, Category = "TD")
	int32 GetCoreMaxHealth() const { return CoreMaxHealth; }

	UFUNCTION(BlueprintPure, Category = "TD")
	int32 GetBalloonsRemaining() const { return BalloonsRemaining; }

	// --- HUD 통지 델리게이트 ---
	UPROPERTY(BlueprintAssignable, Category = "TD")
	FTDPhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "TD")
	FTDCoreHealthChanged OnCoreHealthChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	ETDPhase Phase = ETDPhase::Build;

	UPROPERTY(Replicated)
	int32 WaveNumber = 0;

	UPROPERTY(ReplicatedUsing = OnRep_CoreHealth)
	int32 CoreHealth = 0;

	UPROPERTY(Replicated)
	int32 CoreMaxHealth = 0;

	UPROPERTY(Replicated)
	int32 BalloonsRemaining = 0;

	UFUNCTION()
	void OnRep_Phase();

	UFUNCTION()
	void OnRep_CoreHealth();

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATDLane>> Lanes;

	UPROPERTY(Transient)
	TObjectPtr<ATDCore> Core;

private:
	/** 서버 전용 쓰기 경로. 리슨 서버에서는 OnRep 이 돌지 않으므로 여기서 직접 브로드캐스트한다. */
	friend class ATDGameMode;

	void SetPhase(ETDPhase NewPhase);
	void SetWaveNumber(int32 InWaveNumber);
	void InitCoreHealth(int32 InMaxHealth);
	void SetCoreHealth(int32 InHealth);
	void SetBalloonsRemaining(int32 InRemaining);
};
