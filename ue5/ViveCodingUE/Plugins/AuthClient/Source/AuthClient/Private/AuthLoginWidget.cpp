#include "AuthLoginWidget.h"

#include "AuthSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

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
		Auth->OnLoginCompleted.AddDynamic(this, &UAuthLoginWidget::OnAuthResult);
		Auth->OnSignupCompleted.AddDynamic(this, &UAuthLoginWidget::OnAuthResult);
	}
}

void UAuthLoginWidget::NativeDestruct()
{
	if (UAuthSubsystem* Auth = GetAuth())
	{
		Auth->OnLoginCompleted.RemoveDynamic(this, &UAuthLoginWidget::OnAuthResult);
		Auth->OnSignupCompleted.RemoveDynamic(this, &UAuthLoginWidget::OnAuthResult);
	}
	Super::NativeDestruct();
}

UAuthSubsystem* UAuthLoginWidget::GetAuth() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UAuthSubsystem>();
	}
	return nullptr;
}

void UAuthLoginWidget::OnLoginClicked()
{
	UAuthSubsystem* Auth = GetAuth();
	if (!Auth || !EmailBox || !PasswordBox)
	{
		return;
	}
	SetStatus(TEXT("로그인 중..."));
	Auth->Login(EmailBox->GetText().ToString(), PasswordBox->GetText().ToString());
}

void UAuthLoginWidget::OnSignupClicked()
{
	UAuthSubsystem* Auth = GetAuth();
	if (!Auth || !EmailBox || !PasswordBox || !UsernameBox)
	{
		return;
	}
	SetStatus(TEXT("회원가입 중..."));
	Auth->Signup(
		EmailBox->GetText().ToString(),
		UsernameBox->GetText().ToString(),
		PasswordBox->GetText().ToString());
}

void UAuthLoginWidget::OnAuthResult(bool bSuccess, const FString& Message)
{
	SetStatus(Message);
}

void UAuthLoginWidget::SetStatus(const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
	}
}
