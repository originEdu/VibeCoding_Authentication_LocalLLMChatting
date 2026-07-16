#include "AuthLoginWidget.h"

#include "AuthSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

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

		// 위젯 생성 전에 자동 로그인(Refresh)이 이미 끝났다면 OnLoginCompleted 브로드캐스트를
		// 놓쳤을 수 있으므로, 여기서 로그인 상태를 직접 확인해 채팅으로 전환한다.
		// 단, NativeConstruct 도중 자기 위젯을 제거(SwapTo)하는 것은 위험하므로 다음 틱으로 미룬다.
		if (Auth->IsLoggedIn())
		{
			if (UWorld* World = GetWorld())
			{
				TWeakObjectPtr<UAuthLoginWidget> WeakThis(this);
				World->GetTimerManager().SetTimerForNextTick([WeakThis]()
				{
					if (WeakThis.IsValid())
					{
						WeakThis->SwapTo(WeakThis->ChatWidgetClass);
					}
				});
			}
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
