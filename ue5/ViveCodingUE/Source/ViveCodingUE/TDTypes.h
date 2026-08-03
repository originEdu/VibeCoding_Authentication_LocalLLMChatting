// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "TDTypes.generated.h"

/** 타워 디펜스 전용 로그 카테고리. 멀티플레이 검증 로그가 전부 여기로 나간다. */
VIVECODINGUE_API DECLARE_LOG_CATEGORY_EXTERN(LogTD, Log, All);

/**
 * 매치 진행 단계.
 *
 * Build 로 시작해서 방장이 Start 를 누를 때까지 무한 대기한다(카운트다운 없음).
 * 웨이브가 전부 정리되면 다시 Build 로 돌아오고, 웨이브 테이블을 소진하면 Victory.
 */
UENUM(BlueprintType)
enum class ETDPhase : uint8
{
	/** 타워 배치 가능. 방장의 Start 를 기다린다. */
	Build,
	/** 풍선 스폰/진행 중. 타워 배치 불가. */
	Wave,
	/** 코어 HP 0. */
	Defeat,
	/** 웨이브 테이블 소진. */
	Victory
};

/** 타워 배치 요청 결과. 클라 고스트 색상과 서버 거부 사유가 같은 값을 쓴다. */
UENUM(BlueprintType)
enum class ETDPlaceResult : uint8
{
	Allowed,
	/** Build 페이즈가 아님. */
	WrongPhase,
	/** GameMode 의 AllowedTowerClasses 에 없는 클래스. */
	IllegalClass,
	NotEnoughGold,
	/** 맵 경계 밖. */
	OutOfBounds,
	/** 레인(도로) 위. */
	OnLane,
	/** 코어 위. */
	OnCore,
	/** 이미 다른 액터가 점유한 칸. */
	Occupied,
	/** 요청 레이트 리밋. */
	TooFast
};

/**
 * 웨이브 한 줄의 정의. DT_Waves DataTable 의 행 구조체다.
 *
 * 풍선은 단일 타입이고 웨이브마다 HP/속도/보상/코어 피해만 스케일한다.
 * Tier 는 순수 시각용(머티리얼 색상 선택)이며 게임플레이에 영향을 주지 않는다.
 */
USTRUCT(BlueprintType)
struct FTDWaveRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 이 웨이브에서 레인 하나당 스폰할 풍선 수. 실제 총합은 이 값 × 레인 수. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Wave")
	int32 CountPerLane = 5;

	/** 같은 레인에서 풍선 사이의 스폰 간격(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Wave")
	float SpawnInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Wave")
	float BalloonHealth = 10.0f;

	/** 스플라인을 따라가는 속도(cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Wave")
	float BalloonSpeed = 300.0f;

	/** 풍선 하나를 처치했을 때 마지막 타격 타워 소유자가 받는 골드. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Wave")
	int32 Reward = 5;

	/** 풍선 하나가 코어에 도달했을 때 깎이는 코어 HP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Wave")
	int32 CoreDamage = 1;

	/** 시각 구분용 티어(0~4). 풍선 BP 의 머티리얼 배열 인덱스로 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TD|Wave")
	uint8 Tier = 0;
};
