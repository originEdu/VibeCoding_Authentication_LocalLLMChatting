#include "ChatSubsystem.h"

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

	/** {"role":..., "content":...} JSON 값 하나 생성. */
	TSharedRef<FJsonValue> MakeMessageValue(const FString& Role, const FString& Content)
	{
		const TSharedRef<FJsonObject> Msg = MakeShared<FJsonObject>();
		Msg->SetStringField(TEXT("role"), Role);
		Msg->SetStringField(TEXT("content"), Content);
		return MakeShared<FJsonValueObject>(Msg);
	}
}

void UChatSubsystem::SetBaseUrl(const FString& InBaseUrl)
{
	BaseUrl = InBaseUrl;
	if (BaseUrl.EndsWith(TEXT("/")))
	{
		BaseUrl.LeftChopInline(1);
	}
}

void UChatSubsystem::SetSystemPrompt(const FString& InSystemPrompt)
{
	SystemPrompt = InSystemPrompt;
}

void UChatSubsystem::ClearHistory()
{
	History.Empty();
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UChatSubsystem::MakeRequest(
	const FString& Verb, const FString& Path, const FString& Body)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BaseUrl + Path);
	Request->SetVerb(Verb);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (!Body.IsEmpty())
	{
		Request->SetContentAsString(Body);
	}
	return Request;
}

FString UChatSubsystem::BuildChatBody() const
{
	TArray<TSharedPtr<FJsonValue>> Messages;
	if (!SystemPrompt.IsEmpty())
	{
		Messages.Add(MakeMessageValue(TEXT("system"), SystemPrompt));
	}
	for (const FChatMessage& Msg : History)
	{
		Messages.Add(MakeMessageValue(Msg.Role, Msg.Content));
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetArrayField(TEXT("messages"), Messages);
	Body->SetBoolField(TEXT("stream"), false);
	Body->SetNumberField(TEXT("temperature"), 0.7);
	return JsonToString(Body);
}

void UChatSubsystem::SendMessage(const FString& UserText)
{
	History.Add({ TEXT("user"), UserText });

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
		MakeRequest(TEXT("POST"), TEXT("/v1/chat/completions"), BuildChatBody());
	// 로컬 LLM은 답변 생성에 수십 초가 걸릴 수 있다(스트리밍 미사용). 생성 동안 서버가 아무 바이트도
	// 보내지 않으므로, 총 타임아웃(SetTimeout)뿐 아니라 유휴 구간을 재는 활동 타임아웃(기본 30초)도
	// 함께 늘려야 한다. 그렇지 않으면 30초 유휴에서 요청이 끊겨 연결 실패로 처리된다.
	Req->SetTimeout(120.f);
	Req->SetActivityTimeout(120.f);
	Req->OnProcessRequestComplete().BindUObject(this, &UChatSubsystem::HandleChat);
	Req->ProcessRequest();
}

void UChatSubsystem::HandleChat(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
{
	if (!bOk || !Res.IsValid())
	{
		OnChatResponse.Broadcast(false, TEXT("LLM 서버에 연결할 수 없습니다. 서버가 실행 중인지 확인해 주세요."));
		return;
	}
	if (!EHttpResponseCodes::IsOk(Res->GetResponseCode()))
	{
		OnChatResponse.Broadcast(false,
			FString::Printf(TEXT("LLM 요청에 실패했습니다. (오류 코드 %d)"), Res->GetResponseCode()));
		return;
	}

	// choices[0].message.content 추출.
	TSharedPtr<FJsonObject> Json;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
	const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid() ||
		!Json->TryGetArrayField(TEXT("choices"), Choices) || Choices->Num() == 0)
	{
		OnChatResponse.Broadcast(false, TEXT("LLM 응답을 해석할 수 없습니다."));
		return;
	}

	const TSharedPtr<FJsonObject> First = (*Choices)[0]->AsObject();
	const TSharedPtr<FJsonObject> Message = First.IsValid() ? First->GetObjectField(TEXT("message")) : nullptr;
	FString Reply;
	if (!Message.IsValid() || !Message->TryGetStringField(TEXT("content"), Reply))
	{
		OnChatResponse.Broadcast(false, TEXT("LLM 응답을 해석할 수 없습니다."));
		return;
	}

	Reply = Reply.TrimStartAndEnd();
	History.Add({ TEXT("assistant"), Reply });
	OnChatResponse.Broadcast(true, Reply);
}
