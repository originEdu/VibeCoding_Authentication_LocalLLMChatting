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

/** 대화 이력 한 줄. Role 은 "system"/"user"/"assistant". */
struct FChatMessage
{
	FString Role;
	FString Content;
};

/**
 * 로컬 LLM(llama.cpp, OpenAI 호환 API)과 통신하는 게임 인스턴스 서브시스템.
 *
 * 사용자가 입력한 질문을 /v1/chat/completions 로 보내고, NPC 답변을 델리게이트로 통지한다.
 * 대화 이력(History)을 누적해 멀티턴 대화를 지원하며, 매 요청마다 전체 이력을 함께 보낸다.
 */
UCLASS()
class AUTHCLIENT_API UChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** LLM 서버 base URL (기본 http://localhost:8080). 끝에 슬래시 없이. */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void SetBaseUrl(const FString& InBaseUrl);

	/** NPC 정체성을 정의하는 system 프롬프트. 비어 있으면 보내지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void SetSystemPrompt(const FString& InSystemPrompt);

	/** 사용자 질문을 이력에 추가하고 LLM에 전송한다. 응답은 OnChatResponse 로 통지. */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void SendMessage(const FString& UserText);

	/** 대화 이력을 비운다(새 대화 시작). */
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void ClearHistory();

	// --- 결과 델리게이트 ---
	UPROPERTY(BlueprintAssignable, Category = "Chat")
	FChatResult OnChatResponse;

private:
	FString BaseUrl = TEXT("http://localhost:8080");
	FString SystemPrompt;
	TArray<FChatMessage> History;

	/** HTTP 요청 생성 헬퍼. */
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MakeRequest(
		const FString& Verb, const FString& Path, const FString& Body);

	/** 현재 이력(+system 프롬프트)으로 /v1/chat/completions 요청 바디를 만든다. */
	FString BuildChatBody() const;

	// 응답 핸들러
	void HandleChat(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk);
};
