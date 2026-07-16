#pragma once

#include "CoreMinimal.h"
#include "AuthScreenWidget.h"
#include "AuthProfileWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * 로그인 후 내 정보 화면 베이스 위젯.
 *
 * 채팅 화면 위에 오버레이로 얹혀 표시된다. '돌아가기'는 자기 자신을 뷰포트에서 제거해
 * 아래의 채팅 화면을 다시 드러낸다. 로그아웃 시에는 아래 채팅 화면이 로그인으로 복귀를
 * 담당하므로, 이 위젯에는 별도의 화면 전환 대상(클래스)이 필요 없다.
 *
 * 에디터에서 이 클래스를 부모로 하는 위젯 블루프린트(WBP_Profile)를 만들고,
 * 아래 BindWidget 이름과 동일한 이름의 위젯을 배치한다:
 *   InfoText (TextBlock) - 내 정보 표시
 *   LogoutButton (Button)
 *   BackButton (Button, 선택) - 채팅 화면으로 돌아가기
 *
 * 생성 시 서버에서 내 정보(GetMe)를 조회해 InfoText 에 표시한다.
 */
UCLASS()
class AUTHCLIENT_API UAuthProfileWidget : public UAuthScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* InfoText = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* LogoutButton = nullptr;

	/** 채팅 화면으로 돌아가는 버튼. 배치하지 않아도 무방(선택). */
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* BackButton = nullptr;

private:
	UFUNCTION()
	void OnLogoutClicked();

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnMeResult(bool bSuccess, const FString& Message);

	UFUNCTION()
	void OnLogoutResult(bool bSuccess, const FString& Message);
};
