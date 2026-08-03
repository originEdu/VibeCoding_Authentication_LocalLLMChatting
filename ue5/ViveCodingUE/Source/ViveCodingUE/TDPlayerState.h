// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TDPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDGoldChanged, int32, NewGold);

/**
 * 플레이어별 상태. 골드는 공유 풀이 아니라 개인 지갑이다.
 *
 * 골드 획득은 라스트힛 귀속: 풍선을 처치한 타워의 OwningPlayerState 가 보상을 받는다.
 */
UCLASS()
class VIVECODINGUE_API ATDPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ATDPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "TD")
	int32 GetGold() const { return Gold; }

	UFUNCTION(BlueprintPure, Category = "TD")
	bool IsHost() const { return bIsHost; }

	UPROPERTY(BlueprintAssignable, Category = "TD")
	FTDGoldChanged OnGoldChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Gold)
	int32 Gold = 0;

	/** 방장(리슨 서버 로컬 플레이어)인지. PostLogin 에서 한 번만 정해진다. */
	UPROPERTY(Replicated)
	bool bIsHost = false;

	UFUNCTION()
	void OnRep_Gold();

private:
	/** 서버 전용 쓰기 경로. */
	friend class ATDGameMode;
	friend class ATDPlayerController;

	void SetGold(int32 InGold);
	void AddGold(int32 Delta);
	void SetIsHost(bool bInIsHost) { bIsHost = bInIsHost; }
};
