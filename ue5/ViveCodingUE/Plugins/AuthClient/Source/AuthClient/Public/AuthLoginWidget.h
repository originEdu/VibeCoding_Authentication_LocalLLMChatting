#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AuthLoginWidget.generated.h"

class UEditableTextBox;
class UButton;
class UTextBlock;

/**
 * 간단한 로그인/회원가입 UI 베이스 위젯.
 *
 * 에디터에서 이 클래스를 부모로 하는 위젯 블루프린트(WBP_Login)를 만들고,
 * 아래 BindWidget 이름과 동일한 이름의 위젯을 배치하면 자동으로 연결된다:
 *   EmailBox, PasswordBox, UsernameBox (EditableTextBox)
 *   LoginButton, SignupButton (Button)
 *   StatusText (TextBlock)
 */
UCLASS()
class AUTHCLIENT_API UAuthLoginWidget : public UUserWidget
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
	UEditableTextBox* UsernameBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* LoginButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* SignupButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* StatusText = nullptr;

private:
	UFUNCTION()
	void OnLoginClicked();

	UFUNCTION()
	void OnSignupClicked();

	UFUNCTION()
	void OnAuthResult(bool bSuccess, const FString& Message);

	void SetStatus(const FString& Message);
	class UAuthSubsystem* GetAuth() const;
};
