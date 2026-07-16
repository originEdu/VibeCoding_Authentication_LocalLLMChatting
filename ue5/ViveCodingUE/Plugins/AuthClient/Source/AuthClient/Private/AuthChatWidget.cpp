#include "AuthChatWidget.h"

#include "AuthSubsystem.h"
#include "ChatSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

namespace
{
	UChatSubsystem* GetChat(const UUserWidget* Widget)
	{
		if (const UGameInstance* GI = Widget ? Widget->GetGameInstance() : nullptr)
		{
			return GI->GetSubsystem<UChatSubsystem>();
		}
		return nullptr;
	}
}

void UAuthChatWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SendButton)
	{
		SendButton->OnClicked.AddDynamic(this, &UAuthChatWidget::OnSendClicked);
	}
	if (ProfileButton)
	{
		ProfileButton->OnClicked.AddDynamic(this, &UAuthChatWidget::OnProfileClicked);
	}
	if (UChatSubsystem* Chat = GetChat(this))
	{
		Chat->OnChatResponse.AddDynamic(this, &UAuthChatWidget::OnChatResult);
	}
	if (UAuthSubsystem* Auth = GetAuth())
	{
		// 프로필 오버레이에서 로그아웃하면 이 채팅 화면도 로그인으로 되돌린다.
		Auth->OnLogoutCompleted.AddDynamic(this, &UAuthChatWidget::OnLogoutResult);
	}
}

void UAuthChatWidget::NativeDestruct()
{
	if (UChatSubsystem* Chat = GetChat(this))
	{
		Chat->OnChatResponse.RemoveDynamic(this, &UAuthChatWidget::OnChatResult);
	}
	if (UAuthSubsystem* Auth = GetAuth())
	{
		Auth->OnLogoutCompleted.RemoveDynamic(this, &UAuthChatWidget::OnLogoutResult);
	}
	Super::NativeDestruct();
}

void UAuthChatWidget::OnSendClicked()
{
	UChatSubsystem* Chat = GetChat(this);
	if (!Chat || !InputBox)
	{
		return;
	}

	const FString Text = InputBox->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return;
	}

	AppendLine(FString::Printf(TEXT("You: %s"), *Text));
	InputBox->SetText(FText::GetEmpty());
	SetStatus(TEXT("NPC가 생각 중..."));
	Chat->SendMessage(Text);
}

void UAuthChatWidget::OnProfileClicked()
{
	if (!ProfileWidgetClass)
	{
		return;
	}
	// SwapTo 로 교체하지 않고, 이 채팅 화면 위에 프로필을 얹는다(높은 ZOrder).
	// 프로필의 '돌아가기'가 자기 자신을 제거하면 아래의 이 화면이 그대로 다시 보인다.
	UUserWidget* Profile = GetOwningPlayer()
		? CreateWidget<UUserWidget>(GetOwningPlayer(), ProfileWidgetClass)
		: CreateWidget<UUserWidget>(GetGameInstance(), ProfileWidgetClass);
	if (Profile)
	{
		Profile->AddToViewport(10);
	}
}

void UAuthChatWidget::OnLogoutResult(bool bSuccess, const FString& Message)
{
	// 로그아웃 시 다음 로그인에 이전 대화가 섞이지 않도록 이력을 비우고 로그인 화면으로 복귀.
	if (UChatSubsystem* Chat = GetChat(this))
	{
		Chat->ClearHistory();
	}
	SwapTo(LoginWidgetClass);
}

void UAuthChatWidget::OnChatResult(bool bSuccess, const FString& Reply)
{
	if (bSuccess)
	{
		AppendLine(FString::Printf(TEXT("NPC: %s"), *Reply));
		SetStatus(FString());
	}
	else
	{
		SetStatus(Reply);
	}
}

void UAuthChatWidget::AppendLine(const FString& Line)
{
	if (!LogBuffer.IsEmpty())
	{
		LogBuffer.Append(TEXT("\n"));
	}
	LogBuffer.Append(Line);
	if (ChatLog)
	{
		ChatLog->SetText(FText::FromString(LogBuffer));
	}
}

void UAuthChatWidget::SetStatus(const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
	}
}
