#pragma once

#include "CoreMinimal.h"
#include "AuthScreenWidget.h"
#include "AuthSignupWidget.generated.h"

class UEditableTextBox;
class UButton;
class UTextBlock;

/**
 * 회원가입 화면 베이스 위젯.
 *
 * 에디터에서 이 클래스를 부모로 하는 위젯 블루프린트(WBP_Signup)를 만들고,
 * 아래 BindWidget 이름과 동일한 이름의 위젯을 배치한다:
 *   EmailBox, UsernameBox, PasswordBox (EditableTextBox)
 *   SignupButton (Button), BackButton (Button, 선택)
 *   StatusText (TextBlock)
 * 그리고 디테일 패널에서 LoginWidgetClass 를 지정한다.
 */
UCLASS()
class AUTHCLIENT_API UAuthSignupWidget : public UAuthScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* EmailBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* UsernameBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* PasswordBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* SignupButton = nullptr;

	/** 취소하고 로그인 화면으로 돌아가는 버튼(선택). */
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* BackButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* StatusText = nullptr;

	/** 가입 성공 또는 뒤로가기 시 돌아갈 위젯(WBP_Login). */
	UPROPERTY(EditAnywhere, Category = "Auth|Navigation")
	TSubclassOf<UUserWidget> LoginWidgetClass;

private:
	UFUNCTION()
	void OnSignupClicked();

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnSignupResult(bool bSuccess, const FString& Message);

	void SetStatus(const FString& Message);
};
