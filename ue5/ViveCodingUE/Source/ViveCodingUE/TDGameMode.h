// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TDTypes.h"
#include "TDGameMode.generated.h"

class ATDBalloon;
class ATDGameState;
class ATDPlayerController;
class ATDPlayerState;
class ATDTower;
class UDataTable;

/**
 * 타워 디펜스 매치의 서버 두뇌. 웨이브 진행, 스폰, 페이즈 전환, 모든 권위 검증이 여기 있다.
 *
 * ABaseGM 을 확장하지 않고 형제로 둔 이유: ABaseGM 은 로그인 GameMode 인 BP_GM 의 조상이라,
 * 여기에 웨이브 로직을 넣으면 로그인 흐름까지 오염된다.
 *
 * AGameMode 가 아니라 AGameModeBase 인 이유: 자체 페이즈 머신을 쓰므로 MatchState 는
 * 경쟁하는 두 번째 상태 머신이 된다.
 */
UCLASS()
class VIVECODINGUE_API ATDGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATDGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/**
	 * 방장의 웨이브 시작 요청. ATDPlayerController 의 Server RPC 에서만 불린다.
	 * RPC 가 컨트롤러에 있으므로 UE 소유권 규칙상 클라는 자기 컨트롤러로만 호출할 수 있고,
	 * 신원 위조는 구조적으로 불가능하다. 남은 건 방장 여부 확인뿐이다.
	 */
	void RequestStartWave(ATDPlayerController* Requester);

	/**
	 * 타워 배치 요청. 서버 검증 전체가 여기 있다. 결과를 돌려주면 컨트롤러가 클라에 통지한다.
	 */
	ETDPlaceResult RequestPlaceTower(ATDPlayerController* Requester, TSubclassOf<ATDTower> TowerClass, const FVector& RequestedLocation);

	/**
	 * 풍선이 사라지는 유일한 경로. EndPlay/OnDestroyed 는 레벨 teardown 에도 발화하므로 쓰지 않는다.
	 * bReachedCore 면 코어 HP 를 깎고, Killer 가 있으면 그 플레이어에게 보상 골드를 준다.
	 */
	void OnBalloonRemoved(ATDBalloon* Balloon, bool bReachedCore, ATDPlayerState* Killer);

	/** 타워 타겟팅이 재사용하는 살아있는 풍선 목록. */
	const TArray<TObjectPtr<ATDBalloon>>& GetActiveBalloons() const { return ActiveBalloons; }

	int32 GetTowerCount() const { return Towers.Num(); }

protected:
	/** 웨이브 정의 테이블. 행 구조체는 FTDWaveRow. */
	UPROPERTY(EditDefaultsOnly, Category = "TD")
	TObjectPtr<UDataTable> WaveTable;

	UPROPERTY(EditDefaultsOnly, Category = "TD")
	TSubclassOf<ATDBalloon> BalloonClass;

	/**
	 * 배치를 허용하는 타워 클래스 목록.
	 * TSubclassOf 는 공격자가 통제하는 값이라 이 allowlist 가 없으면 클라가 프로젝트 안의
	 * 아무 ATDTower 자식이나 스폰할 수 있다. 배치 검증에서 가장 중요한 항목이다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "TD")
	TArray<TSubclassOf<ATDTower>> AllowedTowerClasses;

	UPROPERTY(EditDefaultsOnly, Category = "TD")
	int32 StartingGold = 200;

private:
	UPROPERTY(Transient)
	TObjectPtr<ATDGameState> TDGameState;

	UPROPERTY(Transient)
	TObjectPtr<ATDPlayerState> HostPlayerState;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATDBalloon>> ActiveBalloons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ATDTower>> Towers;

	/** 현재 웨이브에서 아직 스폰하지 않은 풍선 수(전체 레인 합). */
	int32 PendingSpawns = 0;

	/** 레인마다 몇 번 더 스폰해야 하는지. */
	int32 RemainingSpawnRounds = 0;

	FTDWaveRow CurrentWave;
	FTimerHandle SpawnTimer;

	void BeginBuildPhase();
	void StartWave();
	void SpawnRound();
	void FinishWave();
	void EnterDefeat();
	void EnterVictory();

	/** 1-based 웨이브 번호에 해당하는 행을 읽는다. 테이블 끝을 지나면 false. */
	bool GetWaveRow(int32 WaveNumber, FTDWaveRow& OutRow) const;

	void UpdateBalloonsRemaining();
};
