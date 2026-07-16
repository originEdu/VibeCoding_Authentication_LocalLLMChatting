#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AuthScreenWidget.generated.h"

class UAuthSubsystem;

/**
 * 인증 화면 위젯 공통 베이스.
 *
 * 화면 전환(SwapTo)과 인증 서브시스템 접근(GetAuth)을 제공한다.
 * 직접 사용하지 않고 로그인/회원가입/프로필 위젯이 상속한다.
 */
UCLASS(Abstract)
class AUTHCLIENT_API UAuthScreenWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** 게임 인스턴스의 인증 서브시스템. 없으면 nullptr. */
	UAuthSubsystem* GetAuth() const;

	/** 이 위젯을 뷰포트에서 제거하고 NextClass 위젯을 대신 띄운다. NextClass 가 없으면 무시. */
	void SwapTo(TSubclassOf<UUserWidget> NextClass);
};
