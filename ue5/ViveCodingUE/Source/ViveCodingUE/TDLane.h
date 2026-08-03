// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDLane.generated.h"

class USplineComponent;
class USplineMeshComponent;

/**
 * 풍선이 지나가는 레인 하나. 동서남북 네 개를 레벨에 배치하고 마지막 스플라인 포인트를 코어에 맞춘다.
 *
 * 복제하지 않는다. 레벨에 배치된 액터라 모든 머신에서 동일하고, GameState 가 BeginPlay 에서
 * 로컬로 수집해 LaneIndex 순으로 정렬한다. 풍선은 uint8 LaneIndex 만 복제하고 그 배열로 역참조한다.
 */
UCLASS()
class VIVECODINGUE_API ATDLane : public AActor
{
	GENERATED_BODY()

public:
	ATDLane();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** 0=북 1=동 2=남 3=서. GameState 의 Lanes 배열 인덱스가 된다. */
	UPROPERTY(EditAnywhere, Category = "TD|Lane")
	int32 LaneIndex = 0;

	/** 스플라인 전체 길이(cm). 풍선이 이 값에 도달하면 코어에 닿은 것으로 본다. */
	UFUNCTION(BlueprintPure, Category = "TD|Lane")
	float GetLength() const;

	/**
	 * 스플라인 시작점에서 Distance(cm) 만큼 떨어진 월드 위치.
	 * FTransform 이 아니라 FVector 를 돌려주는 건 풍선이 둥글어 회전이 필요 없기 때문이다.
	 * 서버에서 풍선마다 매 프레임 호출되는 가장 뜨거운 경로다.
	 */
	UFUNCTION(BlueprintPure, Category = "TD|Lane")
	FVector GetLocationAtDistance(float Distance) const;

	/** 월드 위치에서 이 레인 중심선까지의 XY 거리. 타워 배치 가능 판정에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "TD|Lane")
	float GetDistanceToSpline(const FVector& WorldLocation) const;

	/** 도로 폭(cm). 타워는 이 폭의 절반 + 타워 반경만큼 떨어져야 배치할 수 있다. */
	UPROPERTY(EditAnywhere, Category = "TD|Lane")
	float RoadWidth = 300.0f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "TD|Lane")
	TObjectPtr<USplineComponent> LaneSpline;

	UPROPERTY(EditAnywhere, Category = "TD|Lane")
	float RoadThickness = 20.0f;

	/** 도로를 그릴 메시. 비워두면 시각적 도로 없이 경로만 존재한다. */
	UPROPERTY(EditAnywhere, Category = "TD|Lane")
	TObjectPtr<UStaticMesh> RoadMesh;

	UPROPERTY(EditAnywhere, Category = "TD|Lane")
	TObjectPtr<UMaterialInterface> RoadMaterial;
};
