#include "ChatSubsystem.h"

#include "AuthSubsystem.h"
#include "Engine/GameInstance.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString JsonToString(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	/** 서버가 준 detail 문자열을 사용자에게 보일 한국어로. */
	FString ExtractChatError(FHttpResponsePtr Response, bool bConnected)
	{
		if (!bConnected || !Response.IsValid())
		{
			return TEXT("서버에 연결할 수 없습니다. 서버가 실행 중인지 확인해 주세요.");
		}

		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		FString Detail;
		if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid() &&
			Json->TryGetStringField(TEXT("detail"), Detail))
		{
			if (Detail == TEXT("Unknown NPC"))
			{
				return TEXT("알 수 없는 NPC입니다.");
			}
			if (Detail == TEXT("LLM unavailable"))
			{
				return TEXT("NPC가 응답할 수 없는 상태입니다. 잠시 후 다시 시도해 주세요.");
			}
			if (Detail == TEXT("LLM timeout"))
			{
				return TEXT("응답이 너무 오래 걸립니다. 다시 시도해 주세요.");
			}
			return Detail;
		}
		return FString::Printf(TEXT("요청에 실패했습니다. (오류 코드 %d)"), Response->GetResponseCode());
	}
}

UAuthSubsystem* UChatSubsystem::GetAuth() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UAuthSubsystem>() : nullptr;
}

void UChatSubsystem::SetNpcId(const FString& InNpcId)
{
	NpcId = InNpcId;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UChatSubsystem::MakePost(
	const FString& Path, const FString& Body)
{
	const UAuthSubsystem* Auth = GetAuth();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL((Auth ? Auth->GetBaseUrl() : FString()) + Path);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (Auth)
	{
		Request->SetHeader(TEXT("Authorization"),
			FString::Printf(TEXT("Bearer %s"), *Auth->GetAccessToken()));
	}
	Request->SetContentAsString(Body);
	// 로컬 LLM은 답변 생성에 수십 초가 걸릴 수 있다(스트리밍 미사용). 생성 동안 서버가 아무 바이트도
	// 보내지 않으므로, 총 타임아웃(SetTimeout)뿐 아니라 유휴 구간을 재는 활동 타임아웃(기본 30초)도
	// 함께 늘려야 한다. 그렇지 않으면 30초 유휴에서 요청이 끊겨 연결 실패로 처리된다.
	// 서버는 110초에 끊고 504를 주므로, 클라는 그보다 길게 잡는다.
	Request->SetTimeout(120.f);
	Request->SetActivityTimeout(120.f);
	return Request;
}

void UChatSubsystem::ResetConversation()
{
	if (NpcId.IsEmpty())
	{
		return;
	}
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("npc_id"), NpcId);

	// 결과를 화면에 알릴 것이 없으므로 응답은 무시한다.
	MakePost(TEXT("/chat/reset"), JsonToString(Body))->ProcessRequest();
}

void UChatSubsystem::SendMessage(const FString& UserText)
{
	const UAuthSubsystem* Auth = GetAuth();
	if (!Auth || !Auth->IsLoggedIn())
	{
		OnChatResponse.Broadcast(false, TEXT("로그인이 필요합니다."));
		return;
	}
	bRetriedAfterRefresh = false;
	SendChatRequest(UserText);
}

void UChatSubsystem::SendChatRequest(const FString& UserText)
{
	// 401 재시도 때 다시 보내야 하므로 원문을 보관한다.
	// (IHttpRequest 에는 GetContentAsString() 이 없어 응답 시점에 되꺼낼 수 없다.)
	PendingMessage = UserText;

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("npc_id"), NpcId);
	Body->SetStringField(TEXT("message"), UserText);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = MakePost(TEXT("/chat"), JsonToString(Body));
	Req->OnProcessRequestComplete().BindUObject(this, &UChatSubsystem::HandleChat);
	Req->ProcessRequest();
}

void UChatSubsystem::HandleChat(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
{
	if (!bOk || !Res.IsValid())
	{
		OnChatResponse.Broadcast(false, ExtractChatError(Res, bOk));
		return;
	}

	// 401: access 토큰 만료. 한 번만 갱신 후 같은 질문(PendingMessage)을 다시 보낸다.
	if (Res->GetResponseCode() == 401 && !bRetriedAfterRefresh && !PendingMessage.IsEmpty())
	{
		if (UAuthSubsystem* Auth = GetAuth())
		{
			bRetriedAfterRefresh = true;
			Auth->OnLoginCompleted.AddDynamic(this, &UChatSubsystem::HandleRefreshForRetry);
			Auth->Refresh();
			return;
		}
	}

	if (!EHttpResponseCodes::IsOk(Res->GetResponseCode()))
	{
		OnChatResponse.Broadcast(false, ExtractChatError(Res, bOk));
		return;
	}

	TSharedPtr<FJsonObject> Json;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
	FString Reply;
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid() ||
		!Json->TryGetStringField(TEXT("reply"), Reply))
	{
		OnChatResponse.Broadcast(false, TEXT("서버 응답을 해석할 수 없습니다."));
		return;
	}

	bRetriedAfterRefresh = false;
	OnChatResponse.Broadcast(true, Reply.TrimStartAndEnd());
}

void UChatSubsystem::HandleRefreshForRetry(bool bSuccess, const FString& Message)
{
	if (UAuthSubsystem* Auth = GetAuth())
	{
		Auth->OnLoginCompleted.RemoveDynamic(this, &UChatSubsystem::HandleRefreshForRetry);
	}

	if (!bSuccess)
	{
		OnChatResponse.Broadcast(false, TEXT("세션이 만료되었습니다. 다시 로그인해 주세요."));
		return;
	}
	// SendChatRequest 가 PendingMessage 에 다시 대입하므로 복사본으로 넘긴다.
	const FString Retry = PendingMessage;
	SendChatRequest(Retry);
}
