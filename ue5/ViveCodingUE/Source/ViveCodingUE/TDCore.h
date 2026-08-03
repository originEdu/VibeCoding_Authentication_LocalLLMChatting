// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDCore.generated.h"

class UStaticMeshComponent;

/**
 * 지켜야 하는 중앙 건물. 레벨 원점에 하나만 배치한다.
 *
 * HP 는 여기가 아니라 ATDGameState 에 있다. GameMode 가 BeginPlay 에서 MaxHealth 를 한 번 읽어가고,
 * 이후 이 액터는 메시와 반경만 제공한다. 그래서 복제하지 않는다.
 */
UCLASS()
class VIVECODINGUE_API ATDCore : public AActor
{
	GENERATED_BODY()

public:
	ATDCore();

	/** 매치 시작 시 GameState 로 복사되는 초기 코어 HP. */
	UPROPERTY(EditAnywhere, Category = "TD|Core")
	int32 MaxHealth = 100;

	/** 타워 배치 배제 반경(cm). */
	UPROPERTY(EditAnywhere, Category = "TD|Core")
	float Radius = 400.0f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "TD|Core")
	TObjectPtr<UStaticMeshComponent> CoreMesh;
};
