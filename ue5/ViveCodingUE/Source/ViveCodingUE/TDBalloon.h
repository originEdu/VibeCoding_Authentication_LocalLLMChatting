// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDBalloon.generated.h"

class ATDLane;
class ATDPlayerState;
class UStaticMeshComponent;

/**
 * 레인을 따라 코어로 향하는 풍선.
 *
 * 서버가 권위를 갖고 스플라인 거리를 전진시키며, 위치가 아니라 DistanceAlongPath float 하나만
 * 1Hz 로 복제한다. 자유도가 1뿐이고 나머지(LaneIndex + 로컬 스플라인)로 전부 재구성되기 때문이다.
 * bReplicateMovement 는 명시적으로 끈다. 기본값이 true 이고 켜두면 FRepMovement 가 100Hz 로 나가서
 * 설계 대비 약 200배 대역폭을 먹는데, 게임은 정상 동작해서 버그로 보이지 않는다.
 *
 * 클라는 서버와 같은 식을 로컬 적분하고 OnRep 은 느린 보정 채널로만 쓴다.
 *
 * 업그레이드 경로: 슬로우/스턴 같은 속도 변경이 없다면 SpawnServerTime + Speed 만 COND_InitialOnly
 * 로 보내고 거리는 영영 안 보내는 완전 결정론 방식으로 바꿀 수 있다. 1Hz 보정 채널은 그 대신
 * 나중에 속도 변경을 넣을 여지를 남겨둔 것이다.
 */
UCLASS()
class VIVECODINGUE_API ATDBalloon : public AActor
{
	GENERATED_BODY()

public:
	ATDBalloon();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버 전용. 스폰 직후 GameMode 가 웨이브 데이터로 초기화한다. */
	void InitFromWave(uint8 InLaneIndex, uint8 InTier, float InSpeed, float InHealth);

	/**
	 * 서버 전용 피해 적용. 체력이 0 이하가 되면 GameMode 에 처치를 통지하고 스스로 파괴한다.
	 * Killer 는 골드 보상을 받을 타워 소유자다(라스트힛 귀속).
	 */
	void ApplyDamage(float Amount, ATDPlayerState* Killer);

	float GetDistanceAlongPath() const { return DistanceAlongPath; }
	uint8 GetLaneIndex() const { return LaneIndex; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "TD|Balloon")
	TObjectPtr<UStaticMeshComponent> BalloonMesh;

	/** 티어별 머티리얼. Tier 를 인덱스로 사용하며, 범위를 벗어나면 메시 기본 머티리얼을 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Balloon")
	TArray<TObjectPtr<UMaterialInterface>> TierMaterials;

	UPROPERTY(Replicated)
	uint8 LaneIndex = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Tier)
	uint8 Tier = 0;

	UPROPERTY(Replicated)
	float Speed = 300.0f;

	UPROPERTY(ReplicatedUsing = OnRep_Distance)
	float DistanceAlongPath = 0.0f;

	UFUNCTION()
	void OnRep_Distance();

	UFUNCTION()
	void OnRep_Tier();

private:
	/** 서버 전용. 복제하지 않는다 - HP 바가 없으므로 클라가 알 필요가 없다. */
	float Health = 10.0f;

	/** BeginPlay 에서 GameState 의 레인 배열로 해석한 캐시. */
	UPROPERTY(Transient)
	TObjectPtr<ATDLane> Lane;

	/** 클라 로컬 적분값. 실제로 렌더되는 거리다. */
	float LocalDistance = 0.0f;

	/** 아직 흡수하지 않은 서버-로컬 오차. OnRep 이 채우고 Tick 이 조금씩 소비한다. */
	float PendingError = 0.0f;

	void ResolveLane();
	void ApplyTierMaterial();
};
