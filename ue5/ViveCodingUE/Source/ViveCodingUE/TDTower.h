// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDTypes.h"
#include "TDTower.generated.h"

class ATDBalloon;
class ATDPlayerState;
class UStaticMeshComponent;

/**
 * 플레이어가 배치하는 타워.
 *
 * 사격 판정은 전부 서버에서 돈다. 클라에는 CurrentTarget 만 복제해서 포탑 메시를 로컬로 조준시킨다 -
 * 발사마다 NetMulticast 를 쏘는 것보다 훨씬 싸다.
 * 타겟팅은 콜리전 오버랩이 아니라 GameMode 의 ActiveBalloons 배열 거리 순회다.
 * 풍선 100 × 타워 20 = 프레임당 2000회 float 비교로 사실상 공짜이고, 커스텀 콜리전 채널 세팅이
 * 통째로 필요 없어진다.
 */
UCLASS()
class VIVECODINGUE_API ATDTower : public AActor
{
	GENERATED_BODY()

public:
	ATDTower();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 타워 배치 그리드 한 칸의 크기(cm). 클라 프리뷰와 서버가 같은 값으로 스냅한다. */
	static constexpr float GridSize = 200.0f;

	/** 배치 가능 영역 반쪽 크기(cm). 기존 TDMap 의 Floor 크기와 맞춘 값. */
	static constexpr float PlayableHalfExtent = 4000.0f;

	/** 월드 좌표를 배치 그리드에 스냅한다. 서버는 클라가 보낸 좌표를 믿지 않고 이걸 다시 돌린다. */
	static FVector SnapToGrid(const FVector& WorldLocation);

	/**
	 * 배치 가능 여부. 클라 고스트 프리뷰와 서버 RPC 가 완전히 같은 코드를 돈다.
	 * 초록 고스트인데 서버가 거부하는 건 최악의 버그라, 공유가 이 함수의 존재 이유다.
	 * 골드/페이즈 검사는 여기 없다 - 그건 서버만 알 수 있는 상태다.
	 */
	static ETDPlaceResult CheckPlacement(UWorld* World, const FVector& SnappedLocation, float TowerRadius);

	// --- 클래스 기본값으로 잡는 스탯. 타워가 1종뿐이라 DataTable 을 쓰지 않는다. ---
	UPROPERTY(EditDefaultsOnly, Category = "TD|Tower")
	int32 Cost = 50;

	UPROPERTY(EditDefaultsOnly, Category = "TD|Tower")
	float Range = 1200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TD|Tower")
	float Damage = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "TD|Tower")
	float FireInterval = 0.5f;

	/** 배치 시 다른 액터와의 배제 반경(cm). */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Tower")
	float PlacementRadius = 90.0f;

	/** 서버 전용. 배치 직후 GameMode 가 설정한다. 골드 보상이 여기로 간다. */
	void SetOwningPlayerState(ATDPlayerState* InOwner) { OwningPlayerState = InOwner; }

	ATDPlayerState* GetOwningPlayerState() const { return OwningPlayerState; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "TD|Tower")
	TObjectPtr<UStaticMeshComponent> TowerMesh;

	/** 조준 회전을 적용할 포탑. 없으면 조준을 생략한다. */
	UPROPERTY(VisibleAnywhere, Category = "TD|Tower")
	TObjectPtr<UStaticMeshComponent> TurretMesh;

	UPROPERTY(Replicated)
	TObjectPtr<ATDPlayerState> OwningPlayerState;

	UPROPERTY(Replicated)
	TObjectPtr<ATDBalloon> CurrentTarget;

private:
	float FireCooldown = 0.0f;

	/** 서버 전용. 사거리 안에서 경로를 가장 많이 지난 풍선을 고른다. */
	ATDBalloon* FindTarget() const;
};
