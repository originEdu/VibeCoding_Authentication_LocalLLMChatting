#pragma once

#include "CoreMinimal.h"
#include "AuthScreenWidget.h"
#include "AuthChatWidget.generated.h"

class UEditableTextBox;
class UButton;
class UTextBlock;

/**
 * NPC 대화 화면 베이스 위젯.
 *
 * 에디터에서 이 클래스를 부모로 하는 위젯 블루프린트(WBP_Chat)를 만들고,
 * 아래 BindWidget 이름과 동일한 이름의 위젯을 배치하면 자동으로 연결된다:
 *   InputBox (EditableTextBox) - 사용자 질문 입력
 *   SendButton (Button)
 *   ChatLog (TextBlock) - 대화 로그 표시
 *   StatusText (TextBlock) - 진행/오류 표시
 *   ProfileButton (Button, 선택) - 프로필 화면을 이 위에 얹어 표시
 * 로그인 성공 후 진입하는 메인 화면이며, 디테일 패널에서
 * ProfileWidgetClass, LoginWidgetClass 를 지정한다.
 * 프로필은 SwapTo 로 교체하지 않고 오버레이로 얹으므로, 프로필을 닫으면 이 화면이 그대로 유지된다.
 */
UCLASS()
class AUTHCLIENT_API UAuthChatWidget : public UAuthScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* InputBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* SendButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ChatLog = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* StatusText = nullptr;

	/** 프로필 화면으로 이동하는 버튼. 배치하지 않아도 무방(선택). */
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* ProfileButton = nullptr;

	/** 프로필보기 버튼을 누르면 위에 얹을 위젯(WBP_Profile). */
	UPROPERTY(EditAnywhere, Category = "Chat|Navigation")
	TSubclassOf<UUserWidget> ProfileWidgetClass;

	/** 로그아웃 시 돌아갈 위젯(WBP_Login). */
	UPROPERTY(EditAnywhere, Category = "Chat|Navigation")
	TSubclassOf<UUserWidget> LoginWidgetClass;

private:
	UFUNCTION()
	void OnSendClicked();

	UFUNCTION()
	void OnProfileClicked();

	UFUNCTION()
	void OnChatResult(bool bSuccess, const FString& Reply);

	UFUNCTION()
	void OnLogoutResult(bool bSuccess, const FString& Message);

	void AppendLine(const FString& Line);
	void SetStatus(const FString& Message);

	/** 지금까지의 대화 로그 누적 버퍼. */
	FString LogBuffer;
};
