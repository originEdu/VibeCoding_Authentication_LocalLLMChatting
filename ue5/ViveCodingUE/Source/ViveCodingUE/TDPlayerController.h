// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TDTypes.h"
#include "TDPlayerController.generated.h"

class ATDTower;
class UInputAction;
class UInputMappingContext;
class UTDHUDWidget;

/**
 * 탑다운 커서 조작. 배치 모드 상태와 로컬 고스트 프리뷰를 소유한다.
 *
 * 팬/줌은 폰이, 배치 관련 입력은 여기가 받는다. 배치 좌표는 폰 위치와 무관하게 RPC 인자로만
 * 전달되므로 서버는 카메라 위치를 알 필요가 없다.
 */
UCLASS()
class VIVECODINGUE_API ATDPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATDPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerPlaceTower(TSubclassOf<ATDTower> TowerClass, FVector_NetQuantize10 Location);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartWave();

	UFUNCTION(Client, Reliable)
	void ClientPlacementResult(ETDPlaceResult Result);

	/** HUD 의 Start 버튼이 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "TD")
	void RequestStartWave() { ServerRequestStartWave(); }

	UFUNCTION(BlueprintCallable, Category = "TD")
	void ToggleBuildMode();

	UFUNCTION(BlueprintPure, Category = "TD")
	bool IsInPlacementMode() const { return bPlacementMode; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "TD|Input")
	TObjectPtr<UInputMappingContext> TDMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "TD|Input")
	TObjectPtr<UInputAction> PlaceTowerAction;

	UPROPERTY(EditDefaultsOnly, Category = "TD|Input")
	TObjectPtr<UInputAction> CancelPlacementAction;

	UPROPERTY(EditDefaultsOnly, Category = "TD|Input")
	TObjectPtr<UInputAction> ToggleBuildAction;

	/** 배치 모드에서 선택되는 타워. 서버 allowlist 와 일치해야 배치가 통과한다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Tower")
	TSubclassOf<ATDTower> SelectedTowerClass;

	/** 고스트 프리뷰 액터 클래스. 복제하지 않는 순수 로컬 액터여야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "TD|Tower")
	TSubclassOf<AActor> GhostClass;

	UPROPERTY(EditDefaultsOnly, Category = "TD|UI")
	TSubclassOf<UTDHUDWidget> HUDWidgetClass;

private:
	bool bPlacementMode = false;

	UPROPERTY(Transient)
	TObjectPtr<AActor> GhostActor;

	UPROPERTY(Transient)
	TObjectPtr<UTDHUDWidget> HUDWidget;

	/** 서버 측 레이트 리밋 기준 시각. */
	float LastPlaceTime = -100.0f;

	void HandlePlaceTower();
	void HandleCancelPlacement();

	/**
	 * 마우스 커서 아래의 지면 위치를 구한다.
	 * 라인 트레이스가 아니라 z=0 평면과의 해석적 교차라서 바닥 콜리전에 의존하지 않고 절대 빗나가지 않는다.
	 */
	bool GetCursorGroundLocation(FVector& OutLocation) const;

	void UpdateGhost();
	void DestroyGhost();
};
