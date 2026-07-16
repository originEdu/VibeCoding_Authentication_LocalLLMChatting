#pragma once

#include "CoreMinimal.h"
#include "AuthScreenWidget.h"
#include "AuthLoginWidget.generated.h"

class UEditableTextBox;
class UButton;
class UTextBlock;

/**
 * 로그인 화면 베이스 위젯.
 *
 * 에디터에서 이 클래스를 부모로 하는 위젯 블루프린트(WBP_Login)를 만들고,
 * 아래 BindWidget 이름과 동일한 이름의 위젯을 배치하면 자동으로 연결된다:
 *   EmailBox, PasswordBox (EditableTextBox)
 *   LoginButton, SignupButton (Button)
 *   StatusText (TextBlock)
 * 그리고 디테일 패널에서 SignupWidgetClass, ChatWidgetClass 를 지정한다.
 */
UCLASS()
class AUTHCLIENT_API UAuthLoginWidget : public UAuthScreenWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* EmailBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* PasswordBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* LoginButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* SignupButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* StatusText = nullptr;

	/** 회원가입 버튼을 누르면 열 위젯(WBP_Signup). */
	UPROPERTY(EditAnywhere, Category = "Auth|Navigation")
	TSubclassOf<UUserWidget> SignupWidgetClass;

	/** 로그인 성공 시 이동할 위젯(WBP_Chat). */
	UPROPERTY(EditAnywhere, Category = "Auth|Navigation")
	TSubclassOf<UUserWidget> ChatWidgetClass;

private:
	UFUNCTION()
	void OnLoginClicked();

	UFUNCTION()
	void OnSignupClicked();

	UFUNCTION()
	void OnLoginResult(bool bSuccess, const FString& Message);

	void SetStatus(const FString& Message);
};
