#include "AuthProfileWidget.h"

#include "AuthSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UAuthProfileWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (LogoutButton)
	{
		LogoutButton->OnClicked.AddDynamic(this, &UAuthProfileWidget::OnLogoutClicked);
	}
	if (BackButton)
	{
		BackButton->OnClicked.AddDynamic(this, &UAuthProfileWidget::OnBackClicked);
	}

	if (UAuthSubsystem* Auth = GetAuth())
	{
		Auth->OnMeCompleted.AddDynamic(this, &UAuthProfileWidget::OnMeResult);
		Auth->OnLogoutCompleted.AddDynamic(this, &UAuthProfileWidget::OnLogoutResult);

		if (InfoText)
		{
			InfoText->SetText(FText::FromString(TEXT("내 정보 불러오는 중...")));
		}
		Auth->GetMe();
	}
}

void UAuthProfileWidget::NativeDestruct()
{
	if (UAuthSubsystem* Auth = GetAuth())
	{
		Auth->OnMeCompleted.RemoveDynamic(this, &UAuthProfileWidget::OnMeResult);
		Auth->OnLogoutCompleted.RemoveDynamic(this, &UAuthProfileWidget::OnLogoutResult);
	}
	Super::NativeDestruct();
}

void UAuthProfileWidget::OnLogoutClicked()
{
	if (UAuthSubsystem* Auth = GetAuth())
	{
		if (LogoutButton)
		{
			// 중복 클릭 방지.
			LogoutButton->SetIsEnabled(false);
		}
		Auth->Logout();
	}
}

void UAuthProfileWidget::OnBackClicked()
{
	// 오버레이를 닫으면 아래의 채팅 화면이 그대로 다시 보인다.
	RemoveFromParent();
}

void UAuthProfileWidget::OnMeResult(bool bSuccess, const FString& Message)
{
	if (InfoText)
	{
		InfoText->SetText(FText::FromString(
			bSuccess ? Message : FString::Printf(TEXT("정보 조회 실패: %s"), *Message)));
	}
}

void UAuthProfileWidget::OnLogoutResult(bool bSuccess, const FString& Message)
{
	// 이 오버레이만 닫는다. 로그인 화면으로의 복귀는 아래 채팅 화면이 담당한다.
	RemoveFromParent();
}
