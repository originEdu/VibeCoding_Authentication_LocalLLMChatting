#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "ChatSubsystem.generated.h"

/**
 * NPC 대화 응답 통지용 델리게이트.
 * bSuccess: 성공 여부, Reply: NPC 답변 또는 사람이 읽을 수 있는 오류 메시지.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FChatResult, bool, bSuccess, const FString&, Reply);

class UAuthSubsystem;

/**
 * NPC 대화를 담당하는 게임 인스턴스 서브시스템.
 *
 * LLM을 직접 호출하지 않고 FastAPI 서버의 /chat 을 거친다. 대화 이력과 NPC 성격은
 * 서버가 소유하므로 여기서는 어떤 NPC와 이야기 중인지(NpcId)만 들고 있다.
 * 서버 주소와 access 토큰은 UAuthSubsystem 이 단일 출처다.
 */
UCLASS()
class AUTHCLIENT_API UChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 대화 상대 NPC id (서버 npcs.yaml 의 키, 예: "merchant"). */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void SetNpcId(const FString& InNpcId);

	/** 서버에 보관된 이 NPC와의 대화 이력을 비운다(새 대화 시작). */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void ResetConversation();

	/** 사용자 질문을 서버로 보낸다. 응답은 OnChatResponse 로 통지. */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void SendMessage(const FString& UserText);

	// --- 결과 델리게이트 ---
	UPROPERTY(BlueprintAssignable, Category = "Chat")
	FChatResult OnChatResponse;

private:
	FString NpcId;

	/** 401 재시도용으로 잠시 보관하는 질문. */
	FString PendingMessage;

	/** 이번 질문에 대해 이미 토큰 갱신 재시도를 했는지. 재시도는 1회로 제한한다. */
	bool bRetriedAfterRefresh = false;

	UAuthSubsystem* GetAuth() const;

	/** POST 요청 생성. Authorization 헤더와 타임아웃을 붙인다. */
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MakePost(const FString& Path, const FString& Body);

	void SendChatRequest(const FString& UserText);

	// 응답 핸들러
	void HandleChat(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk);

	/** 401 이후 Auth->Refresh() 결과를 한 번 받기 위한 콜백(동적 델리게이트라 UFUNCTION 필요). */
	UFUNCTION()
	void HandleRefreshForRetry(bool bSuccess, const FString& Message);
};
