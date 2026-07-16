#include "AuthSignupWidget.h"

#include "AuthSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

void UAuthSignupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SignupButton)
	{
		SignupButton->OnClicked.AddDynamic(this, &UAuthSignupWidget::OnSignupClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddDynamic(this, &UAuthSignupWidget::OnBackClicked);
	}

	if (UAuthSubsystem* Auth = GetAuth())
	{
		Auth->OnSignupCompleted.AddDynamic(this, &UAuthSignupWidget::OnSignupResult);
	}
}

void UAuthSignupWidget::NativeDestruct()
{
	if (UAuthSubsystem* Auth = GetAuth())
	{
		Auth->OnSignupCompleted.RemoveDynamic(this, &UAuthSignupWidget::OnSignupResult);
	}
	Super::NativeDestruct();
}

void UAuthSignupWidget::OnSignupClicked()
{
	UAuthSubsystem* Auth = GetAuth();
	if (!Auth || !EmailBox || !UsernameBox || !PasswordBox)
	{
		return;
	}

	const FString Email = EmailBox->GetText().ToString().TrimStartAndEnd();
	const FString Username = UsernameBox->GetText().ToString().TrimStartAndEnd();
	const FString Password = PasswordBox->GetText().ToString();
	if (Email.IsEmpty() || Username.IsEmpty() || Password.IsEmpty())
	{
		SetStatus(TEXT("이메일, 사용자 이름, 비밀번호를 모두 입력해 주세요."));
		return;
	}
	if (Password.Len() < 8)
	{
		SetStatus(TEXT("비밀번호는 8자 이상이어야 합니다."));
		return;
	}

	SetStatus(TEXT("회원가입 중..."));
	Auth->Signup(Email, Username, Password);
}

void UAuthSignupWidget::OnBackClicked()
{
	SwapTo(LoginWidgetClass);
}

void UAuthSignupWidget::OnSignupResult(bool bSuccess, const FString& Message)
{
	if (bSuccess)
	{
		// 가입 성공 -> 로그인 화면으로 복귀해 직접 로그인.
		SwapTo(LoginWidgetClass);
	}
	else
	{
		SetStatus(Message);
	}
}

void UAuthSignupWidget::SetStatus(const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
	}
}
