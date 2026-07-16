#include "AuthLoginWidget.h"

#include "AuthSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

void UAuthLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (LoginButton)
	{
		LoginButton->OnClicked.AddDynamic(this, &UAuthLoginWidget::OnLoginClicked);
	}
	if (SignupButton)
	{
		SignupButton->OnClicked.AddDynamic(this, &UAuthLoginWidget::OnSignupClicked);
	}

	if (UAuthSubsystem* Auth = GetAuth())
	{
		Auth->OnLoginCompleted.AddDynamic(this, &UAuthLoginWidget::OnLoginResult);

		// 저장된 refresh 토큰으로 자동 로그인이 이미 끝난 상태라면 바로 채팅으로 이동.
		if (Auth->IsLoggedIn())
		{
			SwapTo(ChatWidgetClass);
		}
	}
}

void UAuthLoginWidget::NativeDestruct()
{
	if (UAuthSubsystem* Auth = GetAuth())
	{
		Auth->OnLoginCompleted.RemoveDynamic(this, &UAuthLoginWidget::OnLoginResult);
	}
	Super::NativeDestruct();
}

void UAuthLoginWidget::OnLoginClicked()
{
	UAuthSubsystem* Auth = GetAuth();
	if (!Auth || !EmailBox || !PasswordBox)
	{
		return;
	}

	const FString Email = EmailBox->GetText().ToString().TrimStartAndEnd();
	const FString Password = PasswordBox->GetText().ToString();
	if (Email.IsEmpty() || Password.IsEmpty())
	{
		SetStatus(TEXT("이메일과 비밀번호를 입력해 주세요."));
		return;
	}

	SetStatus(TEXT("로그인 중..."));
	Auth->Login(Email, Password);
}

void UAuthLoginWidget::OnSignupClicked()
{
	// 회원가입은 별도 화면에서 진행.
	SwapTo(SignupWidgetClass);
}

void UAuthLoginWidget::OnLoginResult(bool bSuccess, const FString& Message)
{
	if (bSuccess)
	{
		SwapTo(ChatWidgetClass);
	}
	else
	{
		SetStatus(Message);
	}
}

void UAuthLoginWidget::SetStatus(const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
	}
}
