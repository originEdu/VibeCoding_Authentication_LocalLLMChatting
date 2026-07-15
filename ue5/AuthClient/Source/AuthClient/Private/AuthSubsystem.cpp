#include "AuthSubsystem.h"

#include "AuthSaveGame.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const TCHAR* UAuthSubsystem::SaveSlotName = TEXT("AuthClientTokens");

namespace
{
	FString JsonToString(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}
}

void UAuthSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadTokens();
	if (!RefreshToken.IsEmpty())
	{
		// 저장된 refresh 토큰으로 자동 로그인 시도.
		Refresh();
	}
}

void UAuthSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UAuthSubsystem::SetBaseUrl(const FString& InBaseUrl)
{
	BaseUrl = InBaseUrl;
	if (BaseUrl.EndsWith(TEXT("/")))
	{
		BaseUrl.LeftChopInline(1);
	}
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UAuthSubsystem::MakeRequest(
	const FString& Verb, const FString& Path, const FString& Body, bool bAuth)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BaseUrl + Path);
	Request->SetVerb(Verb);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (bAuth && !AccessToken.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));
	}
	if (!Body.IsEmpty())
	{
		Request->SetContentAsString(Body);
	}
	return Request;
}

bool UAuthSubsystem::ParseJson(FHttpResponsePtr Response, TSharedPtr<FJsonObject>& OutJson)
{
	if (!Response.IsValid())
	{
		return false;
	}
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	return FJsonSerializer::Deserialize(Reader, OutJson) && OutJson.IsValid();
}

FString UAuthSubsystem::ExtractError(FHttpResponsePtr Response, bool bConnected)
{
	if (!bConnected || !Response.IsValid())
	{
		return TEXT("서버에 연결할 수 없습니다.");
	}
	TSharedPtr<FJsonObject> Json;
	if (ParseJson(Response, Json))
	{
		FString Detail;
		if (Json->TryGetStringField(TEXT("detail"), Detail))
		{
			return Detail;
		}
	}
	return FString::Printf(TEXT("요청 실패 (HTTP %d)"), Response->GetResponseCode());
}

// --- 토큰 영속화 ---
void UAuthSubsystem::SaveTokens()
{
	UAuthSaveGame* Save = Cast<UAuthSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UAuthSaveGame::StaticClass()));
	Save->RefreshToken = RefreshToken;
	UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0);
}

void UAuthSubsystem::LoadTokens()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		return;
	}
	if (UAuthSaveGame* Save = Cast<UAuthSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0)))
	{
		RefreshToken = Save->RefreshToken;
	}
}

void UAuthSubsystem::ClearTokens()
{
	AccessToken.Empty();
	RefreshToken.Empty();
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0);
	}
}

bool UAuthSubsystem::StoreTokensFromResponse(FHttpResponsePtr Response, bool bConnected, FString& OutError)
{
	if (!bConnected || !Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		OutError = ExtractError(Response, bConnected);
		return false;
	}
	TSharedPtr<FJsonObject> Json;
	if (!ParseJson(Response, Json) ||
		!Json->TryGetStringField(TEXT("access_token"), AccessToken) ||
		!Json->TryGetStringField(TEXT("refresh_token"), RefreshToken))
	{
		OutError = TEXT("서버 응답을 해석할 수 없습니다.");
		return false;
	}
	SaveTokens();
	return true;
}

// --- Signup ---
void UAuthSubsystem::Signup(const FString& Email, const FString& Username, const FString& Password)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("email"), Email);
	Body->SetStringField(TEXT("username"), Username);
	Body->SetStringField(TEXT("password"), Password);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
		MakeRequest(TEXT("POST"), TEXT("/auth/signup"), JsonToString(Body), false);
	Req->OnProcessRequestComplete().BindUObject(this, &UAuthSubsystem::HandleSignup);
	Req->ProcessRequest();
}

void UAuthSubsystem::HandleSignup(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
{
	if (bOk && Res.IsValid() && Res->GetResponseCode() == 201)
	{
		OnSignupCompleted.Broadcast(true, TEXT("회원가입 성공"));
	}
	else
	{
		OnSignupCompleted.Broadcast(false, ExtractError(Res, bOk));
	}
}

// --- Login ---
void UAuthSubsystem::Login(const FString& Email, const FString& Password)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("email"), Email);
	Body->SetStringField(TEXT("password"), Password);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
		MakeRequest(TEXT("POST"), TEXT("/auth/login"), JsonToString(Body), false);
	Req->OnProcessRequestComplete().BindUObject(this, &UAuthSubsystem::HandleLogin);
	Req->ProcessRequest();
}

void UAuthSubsystem::HandleLogin(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
{
	FString Error;
	if (StoreTokensFromResponse(Res, bOk, Error))
	{
		OnLoginCompleted.Broadcast(true, TEXT("로그인 성공"));
	}
	else
	{
		OnLoginCompleted.Broadcast(false, Error);
	}
}

// --- Refresh ---
void UAuthSubsystem::Refresh()
{
	if (RefreshToken.IsEmpty())
	{
		OnLoginCompleted.Broadcast(false, TEXT("저장된 refresh 토큰이 없습니다."));
		return;
	}
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("refresh_token"), RefreshToken);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
		MakeRequest(TEXT("POST"), TEXT("/auth/refresh"), JsonToString(Body), false);
	Req->OnProcessRequestComplete().BindUObject(this, &UAuthSubsystem::HandleRefresh);
	Req->ProcessRequest();
}

void UAuthSubsystem::HandleRefresh(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
{
	FString Error;
	if (StoreTokensFromResponse(Res, bOk, Error))
	{
		OnLoginCompleted.Broadcast(true, TEXT("세션 갱신됨"));
	}
	else
	{
		ClearTokens();  // refresh 실패 -> 자동 로그인 불가, 저장 토큰 폐기
		OnLoginCompleted.Broadcast(false, Error);
	}
}

// --- Logout ---
void UAuthSubsystem::Logout()
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("refresh_token"), RefreshToken);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
		MakeRequest(TEXT("POST"), TEXT("/auth/logout"), JsonToString(Body), true);
	Req->OnProcessRequestComplete().BindUObject(this, &UAuthSubsystem::HandleLogout);
	Req->ProcessRequest();
}

void UAuthSubsystem::HandleLogout(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
{
	const bool bSuccess = bOk && Res.IsValid() && Res->GetResponseCode() == 204;
	ClearTokens();  // 서버 결과와 무관하게 로컬 세션 종료
	OnLogoutCompleted.Broadcast(bSuccess, bSuccess ? TEXT("로그아웃 완료") : ExtractError(Res, bOk));
}

// --- Withdraw ---
void UAuthSubsystem::Withdraw()
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
		MakeRequest(TEXT("DELETE"), TEXT("/auth/me"), FString(), true);
	Req->OnProcessRequestComplete().BindUObject(this, &UAuthSubsystem::HandleWithdraw);
	Req->ProcessRequest();
}

void UAuthSubsystem::HandleWithdraw(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
{
	if (bOk && Res.IsValid() && Res->GetResponseCode() == 204)
	{
		ClearTokens();
		OnWithdrawCompleted.Broadcast(true, TEXT("탈퇴 완료"));
	}
	else
	{
		OnWithdrawCompleted.Broadcast(false, ExtractError(Res, bOk));
	}
}

// --- GetMe ---
void UAuthSubsystem::GetMe()
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
		MakeRequest(TEXT("GET"), TEXT("/auth/me"), FString(), true);
	Req->OnProcessRequestComplete().BindUObject(this, &UAuthSubsystem::HandleMe);
	Req->ProcessRequest();
}

void UAuthSubsystem::HandleMe(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
{
	TSharedPtr<FJsonObject> Json;
	if (bOk && Res.IsValid() && EHttpResponseCodes::IsOk(Res->GetResponseCode()) && ParseJson(Res, Json))
	{
		const FString Username = Json->GetStringField(TEXT("username"));
		const FString Email = Json->GetStringField(TEXT("email"));
		OnMeCompleted.Broadcast(true, FString::Printf(TEXT("%s <%s>"), *Username, *Email));
	}
	else
	{
		OnMeCompleted.Broadcast(false, ExtractError(Res, bOk));
	}
}
