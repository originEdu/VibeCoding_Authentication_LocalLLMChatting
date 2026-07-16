#include "AuthSubsystem.h"

#include "AuthSaveGame.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/DateTime.h"

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

	/** 서버의 ISO 8601 created_at 을 "YYYY-MM-DD HH:MM" 으로. 파싱 실패 시 날짜(앞 10자)만. */
	FString FormatCreatedAt(const FString& Iso)
	{
		FDateTime Dt;
		if (FDateTime::ParseIso8601(*Iso, Dt))
		{
			return Dt.ToString(TEXT("%Y-%m-%d %H:%M"));
		}
		return Iso.Left(10);
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
		return TEXT("서버에 연결할 수 없습니다. 서버가 실행 중인지 확인해 주세요.");
	}

	TSharedPtr<FJsonObject> Json;
	ParseJson(Response, Json);

	// 문자열 detail(400/401 등) -> 사용자 친화 메시지로 매핑.
	FString Detail;
	if (Json.IsValid() && Json->TryGetStringField(TEXT("detail"), Detail))
	{
		if (Detail == TEXT("Invalid credentials"))
		{
			return TEXT("이메일 또는 비밀번호가 올바르지 않습니다.");
		}
		if (Detail == TEXT("Account is inactive"))
		{
			return TEXT("탈퇴했거나 사용할 수 없는 계정입니다.");
		}
		if (Detail == TEXT("Email already registered"))
		{
			return TEXT("이미 가입된 이메일입니다. 다른 이메일을 사용해 주세요.");
		}
		if (Detail == TEXT("Invalid refresh token"))
		{
			return TEXT("세션이 만료되었습니다. 다시 로그인해 주세요.");
		}
		return Detail;  // 알 수 없는 메시지는 서버 원문 그대로 노출.
	}

	// 검증 오류(422): detail 이 배열 -> 첫 항목의 필드명으로 안내.
	const TArray<TSharedPtr<FJsonValue>>* Errors = nullptr;
	if (Json.IsValid() && Json->TryGetArrayField(TEXT("detail"), Errors) && Errors->Num() > 0)
	{
		FString Field;
		if (const TSharedPtr<FJsonObject> First = (*Errors)[0]->AsObject())
		{
			const TArray<TSharedPtr<FJsonValue>>* Loc = nullptr;
			if (First->TryGetArrayField(TEXT("loc"), Loc) && Loc->Num() > 0)
			{
				Field = (*Loc)[Loc->Num() - 1]->AsString();  // 예: ["body","password"] -> "password"
			}
		}
		if (Field == TEXT("email"))
		{
			return TEXT("이메일 형식이 올바르지 않습니다.");
		}
		if (Field == TEXT("password"))
		{
			return TEXT("비밀번호는 8자 이상 128자 이하여야 합니다.");
		}
		if (Field == TEXT("username"))
		{
			return TEXT("사용자 이름은 1자 이상 50자 이하여야 합니다.");
		}
		return TEXT("입력값을 확인해 주세요.");
	}

	return FString::Printf(TEXT("요청에 실패했습니다. (오류 코드 %d)"), Response->GetResponseCode());
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
		const FString Email = Json->GetStringField(TEXT("email"));
		const FString Username = Json->GetStringField(TEXT("username"));
		const FString Created = FormatCreatedAt(Json->GetStringField(TEXT("created_at")));
		const FString Info = FString::Printf(
			TEXT("Email : %s\nUsername : %s\n생성일 : %s"), *Email, *Username, *Created);
		OnMeCompleted.Broadcast(true, Info);
	}
	else
	{
		OnMeCompleted.Broadcast(false, ExtractError(Res, bOk));
	}
}
