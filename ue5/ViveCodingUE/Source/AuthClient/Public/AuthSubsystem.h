#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "AuthSubsystem.generated.h"

/**
 * 인증 작업 결과 통지용 델리게이트.
 * bSuccess: 성공 여부, Message: 사람이 읽을 수 있는 상태/오류 메시지.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAuthResult, bool, bSuccess, const FString&, Message);

/**
 * FastAPI 인증 서버와 통신하는 게임 인스턴스 서브시스템.
 *
 * Blueprint 및 C++에서 회원가입/로그인/토큰 갱신/로그아웃/탈퇴를 호출하고,
 * 결과는 대응하는 델리게이트로 통지받는다. access/refresh 토큰은 메모리에 보관하며
 * refresh 토큰은 SaveGame으로 영속화하여 재시작 시 자동 로그인에 사용한다.
 */
UCLASS()
class AUTHCLIENT_API UAuthSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 서버 base URL (예: http://127.0.0.1:8000). 끝에 슬래시 없이. */
	UFUNCTION(BlueprintCallable, Category = "Auth")
	void SetBaseUrl(const FString& InBaseUrl);

	UFUNCTION(BlueprintCallable, Category = "Auth")
	void Signup(const FString& Email, const FString& Username, const FString& Password);

	UFUNCTION(BlueprintCallable, Category = "Auth")
	void Login(const FString& Email, const FString& Password);

	/** 저장된 refresh 토큰으로 새 토큰을 발급받는다(회전). */
	UFUNCTION(BlueprintCallable, Category = "Auth")
	void Refresh();

	UFUNCTION(BlueprintCallable, Category = "Auth")
	void Logout();

	/** 회원 탈퇴(소프트 삭제). 성공 시 로컬 토큰도 제거. */
	UFUNCTION(BlueprintCallable, Category = "Auth")
	void Withdraw();

	/** 현재 access 토큰으로 내 정보를 조회한다. */
	UFUNCTION(BlueprintCallable, Category = "Auth")
	void GetMe();

	UFUNCTION(BlueprintPure, Category = "Auth")
	bool IsLoggedIn() const { return !AccessToken.IsEmpty(); }

	// --- 결과 델리게이트 ---
	UPROPERTY(BlueprintAssignable, Category = "Auth")
	FAuthResult OnSignupCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Auth")
	FAuthResult OnLoginCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Auth")
	FAuthResult OnLogoutCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Auth")
	FAuthResult OnWithdrawCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Auth")
	FAuthResult OnMeCompleted;

private:
	FString BaseUrl = TEXT("http://192.168.0.42:8081");
	FString AccessToken;
	FString RefreshToken;

	static const TCHAR* SaveSlotName;

	/** HTTP 요청 생성 헬퍼. bAuth=true 이면 Authorization: Bearer 헤더 추가. */
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MakeRequest(
		const FString& Verb, const FString& Path, const FString& Body, bool bAuth);

	/** 응답에서 JSON 오브젝트를 파싱. 실패 시 false 와 오류 메시지 반환. */
	static bool ParseJson(FHttpResponsePtr Response, TSharedPtr<FJsonObject>& OutJson);

	/** 응답에서 서버가 준 detail 메시지 또는 상태 코드 기반 기본 메시지 추출. */
	static FString ExtractError(FHttpResponsePtr Response, bool bConnected);

	// 토큰 영속화
	void SaveTokens();
	void LoadTokens();
	void ClearTokens();

	// 응답 핸들러
	void HandleSignup(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk);
	void HandleLogin(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk);
	void HandleRefresh(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk);
	void HandleLogout(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk);
	void HandleWithdraw(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk);
	void HandleMe(FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk);

	/** 로그인/갱신 응답 공통: access/refresh 토큰 저장. 성공 여부 반환. */
	bool StoreTokensFromResponse(FHttpResponsePtr Response, bool bConnected, FString& OutError);
};
