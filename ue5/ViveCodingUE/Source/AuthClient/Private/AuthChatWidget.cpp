#include "AuthChatWidget.h"

#include "AuthSubsystem.h"
#include "ChatSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
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
	if (InputBox)
	{
		InputBox->OnTextCommitted.AddDynamic(this, &UAuthChatWidget::OnInputCommitted);
		InputBox->SetKeyboardFocus();  // 채팅 화면 진입 시 입력창에 자동 포커스.
	}
	if (ProfileButton)
	{
		ProfileButton->OnClicked.AddDynamic(this, &UAuthChatWidget::OnProfileClicked);
	}
	if (UChatSubsystem* Chat = GetChat(this))
	{
		Chat->OnChatResponse.AddDynamic(this, &UAuthChatWidget::OnChatResult);

		// 이 NPC와의 새 대화 시작: 상대를 지정하고 서버에 남은 이전 이력을 비운다.
		// (한 번에 한 NPC와 대화하는 방식이라, 채팅 화면을 열 때마다 새 대화로 시작한다.)
		Chat->SetNpcId(NpcId);
		Chat->ResetConversation();
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
	SubmitInput();
}

void UAuthChatWidget::OnInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// Enter 로 확정했을 때만 전송한다(포커스 이동 등 다른 커밋은 무시).
	if (CommitMethod == ETextCommit::OnEnter)
	{
		SubmitInput();
	}
}

void UAuthChatWidget::SubmitInput()
{
	UChatSubsystem* Chat = GetChat(this);
	if (!Chat || !InputBox)
	{
		return;
	}

	// 이전 질문의 응답을 기다리는 중이면 새 전송을 무시한다(중복 요청/이력 꼬임 방지).
	if (bWaitingForResponse)
	{
		InputBox->SetKeyboardFocus();
		return;
	}

	const FString Text = InputBox->GetText().ToString().TrimStartAndEnd();
	if (!Text.IsEmpty())
	{
		AppendLine(FString::Printf(TEXT("You: %s"), *Text));
		InputBox->SetText(FText::GetEmpty());
		SetStatus(TEXT("NPC가 생각 중..."));
		bWaitingForResponse = true;
		if (SendButton)
		{
			SendButton->SetIsEnabled(false);  // 대기 중 시각 피드백.
		}
		Chat->SendMessage(Text);
	}

	// 전송 후에도 계속 이어서 입력할 수 있도록 입력창에 다시 포커스한다.
	InputBox->SetKeyboardFocus();
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
	// 서버가 로그아웃 시 이 유저의 대화 이력을 파기하므로 클라에서 따로 비울 것이 없다.
	SwapTo(LoginWidgetClass);
}

void UAuthChatWidget::OnChatResult(bool bSuccess, const FString& Reply)
{
	// 성공/실패와 무관하게 대기 상태를 풀어 다시 전송할 수 있게 한다.
	bWaitingForResponse = false;
	if (SendButton)
	{
		SendButton->SetIsEnabled(true);
	}

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
	if (ChatScroll)
	{
		ChatScroll->ScrollToEnd();  // 새 줄이 추가될 때마다 맨 아래로 고정.
	}
}

void UAuthChatWidget::SetStatus(const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
	}
}
