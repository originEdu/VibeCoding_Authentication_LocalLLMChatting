#include "AuthScreenWidget.h"

#include "AuthSubsystem.h"
#include "Engine/GameInstance.h"

UAuthSubsystem* UAuthScreenWidget::GetAuth() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UAuthSubsystem>();
	}
	return nullptr;
}

void UAuthScreenWidget::SwapTo(TSubclassOf<UUserWidget> NextClass)
{
	if (!NextClass)
	{
		return;
	}
	// 소유 플레이어 컨트롤러가 있으면 그대로 넘겨 입력/포커스 연속성을 유지한다.
	UUserWidget* Next = GetOwningPlayer()
		? CreateWidget<UUserWidget>(GetOwningPlayer(), NextClass)
		: CreateWidget<UUserWidget>(GetGameInstance(), NextClass);

	// 다음 화면을 먼저 띄운 뒤 현재 화면을 제거해 UI 공백 프레임을 피한다.
	if (Next)
	{
		Next->AddToViewport();
	}
	RemoveFromParent();
}
