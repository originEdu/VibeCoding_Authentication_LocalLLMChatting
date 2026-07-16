# UE5 로컬 LLM NPC 대화 기능 설계

> 정해진 문답이 아닌, 사용자가 자유롭게 입력한 질문에 NPC가 답변하는 대화 기능. NPC의 두뇌는 `http://localhost:8080`에 떠 있는 로컬 LLM(llama.cpp + Gemma-4-E2B, OpenAI 호환 API)이며, UE5 C++에서 HTTP로 통신한다.

작성일: 2026-07-16

---

## 배경 (Context)

언리얼 프로젝트(`ue5/ViveCodingUE`)에 자유 입력형 NPC 대화 기능을 추가한다. 이 프로젝트에는 이미 `AuthClient` 플러그인이 있어 `FHttpModule` 기반 비동기 HTTP + JSON 파싱 + UMG 위젯 + 델리게이트 콜백 패턴이 완성되어 있다. 새 기능은 이 검증된 패턴을 거의 그대로 복제해 위험을 낮춘다.

### 결정된 요구사항 (브레인스토밍 결과)

| 항목 | 결정 | 이유 |
| --- | --- | --- |
| 응답 수신 | **블로킹** (완성 응답 한 번에 표시) | 기존 코드가 이미 이 방식 → 스트리밍 대비 구현 단순 |
| 대화 기억 | **멀티턴** (메시지 배열 누적) | 이전 질문/답변을 반영한 자연스러운 대화 |
| NPC 정체성 | **지금은 없음** | 나중에 한 줄로 채울 수 있게 빈 `SystemPrompt` 필드만 준비 |
| UI 범위 | **채팅 위젯 포함** | 단, **로그인 후에만** 채팅 화면 진입 가능 |
| 배치 | **기존 `AuthClient` 플러그인에 추가** | 채팅 위젯이 `UAuthScreenWidget` 공유 헬퍼를 상속 → 동일 모듈이 가장 단순 |

---

## 재사용할 기존 패턴 (AuthClient)

- **`AuthSubsystem.cpp`** — `FHttpModule::Get().CreateRequest()` → URL/Verb/`Content-Type` 헤더 설정 → `SetContentAsString(body)` → `OnProcessRequestComplete().BindUObject(...)` → `ProcessRequest()`. 응답은 `TJsonReaderFactory` + `FJsonSerializer::Deserialize`로 파싱. 바디 직렬화 헬퍼 `JsonToString`.
- **`AuthSubsystem.h`** — `UGameInstanceSubsystem` 기반, `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams` 결과 델리게이트, `UPROPERTY(BlueprintAssignable)`로 노출.
- **`AuthScreenWidget`** — `UUserWidget` 추상 베이스. `GetAuth()`(서브시스템 조회), `SwapTo(TSubclassOf<UUserWidget>)`(위젯 교체) 헬퍼 제공.
- **`AuthLoginWidget`** — `meta=(BindWidget)`로 UMG 요소 바인딩, `NativeConstruct`에서 버튼 `OnClicked.AddDynamic` + 서브시스템 델리게이트 구독, `NativeDestruct`에서 `RemoveDynamic`.
- **`AuthClient.Build.cs`** — `HTTP`, `Json`, `JsonUtilities`, `UMG`, `Slate`, `SlateCore` 의존성이 이미 선언됨. **추가 의존성 불필요.**

---

## 구현 계획

### 1. `UChatSubsystem` (신규) — `AuthSubsystem` 미러링

파일: `Plugins/AuthClient/Source/AuthClient/Public/ChatSubsystem.h`, `Private/ChatSubsystem.cpp`

- `UGameInstanceSubsystem` 상속, `AUTHCLIENT_API`
- 상태:
  - `FString BaseUrl` — 기본값 `http://localhost:8080`
  - `FString SystemPrompt` — 기본값 빈 문자열
  - `TArray<FChatMessage> History` — `FChatMessage { FString Role; FString Content; }` (USTRUCT)
- `UFUNCTION(BlueprintCallable)`:
  - `SendMessage(const FString& UserText)` — History에 `{"user", UserText}` 추가 → `/v1/chat/completions` 바디 조립(SystemPrompt 비어있지 않으면 맨 앞에 system 메시지 포함) → 요청 전송 + `HandleChat` 바인딩
  - `ClearHistory()` — History 비움
  - `SetBaseUrl(const FString&)`, `SetSystemPrompt(const FString&)`
- `HandleChat(FHttpRequestPtr, FHttpResponsePtr, bool)` (private) — `EHttpResponseCodes::IsOk` 확인 → `choices[0].message.content` 파싱 → History에 `{"assistant", reply}` 추가 → `OnChatResponse.Broadcast(true, reply)`; 실패 시 `Broadcast(false, 에러메시지)`
- 델리게이트: `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FChatResult, bool, bSuccess, const FString&, Reply)` → `UPROPERTY(BlueprintAssignable) FChatResult OnChatResponse;`
- 바디 직렬화는 `AuthSubsystem`의 `JsonToString`와 동일한 소형 헬퍼를 이 파일에 복제(모듈 내 private 메서드라 직접 재사용 불가, 몇 줄짜리라 복제가 적절)

**전송 바디 예시:**

```json
{ "messages": [{"role":"user","content":"안녕?"}], "stream": false, "temperature": 0.7 }
```

### 2. `UAuthChatWidget` (신규) — `AuthLoginWidget` 미러링

파일: `Plugins/AuthClient/Source/AuthClient/Public/AuthChatWidget.h`, `Private/AuthChatWidget.cpp`

- `UAuthScreenWidget` 상속 (→ `GetGameInstance()->GetSubsystem<UChatSubsystem>()` 조회, `SwapTo` 사용 가능)
- `UPROPERTY(meta=(BindWidget))`: `UEditableTextBox* InputBox`, `UButton* SendButton`, `UTextBlock* ChatLog`(또는 ScrollBox+TextBlock), `UTextBlock* StatusText`
- `NativeConstruct`: `SendButton->OnClicked.AddDynamic(this, &OnSendClicked)` + `Chat->OnChatResponse.AddDynamic(this, &OnChatResult)`
- `NativeDestruct`: 대응 `RemoveDynamic`
- `OnSendClicked()` (UFUNCTION): 입력 읽기 → 빈 값이면 무시 → ChatLog에 "You: {text}" 추가 → InputBox 비움 → `StatusText`에 "..." 표시 → `Chat->SendMessage(text)`
- `OnChatResult(bool bSuccess, const FString& Reply)` (UFUNCTION): 성공 시 ChatLog에 "NPC: {Reply}" 추가, 실패 시 StatusText에 에러 표시

### 3. 로그인 게이팅

- `WBP_Profile`(에디터)에 "채팅" 버튼을 추가하고, `AuthProfileWidget`에 `TSubclassOf<UUserWidget> ChatWidgetClass`(`EditAnywhere`) + 버튼 핸들러 `SwapTo(ChatWidgetClass)` 추가 — `AuthLoginWidget`이 SignupWidget으로 이동하는 방식과 동일
- 로그인 성공 → 프로필 → 채팅 순서라 로그인 없이는 채팅 진입 불가

### 4. 에디터 수작업 (코드로 생성 불가)

- 에디터에서 `WBP_Chat.uasset` 생성 → 부모 클래스를 `UAuthChatWidget`으로 지정 → 위젯 이름을 `BindWidget` 이름(`InputBox`, `SendButton`, `ChatLog`, `StatusText`)과 정확히 일치
- `WBP_Profile`에 채팅 버튼 배치 후 `ChatWidgetClass`에 `WBP_Chat` 지정
- (기존 `WBP_Login` / `WBP_Profile` / `WBP_Signup`이 만들어진 방식과 동일)

---

## 화면 흐름

```
[로그인] --성공--> [프로필] --채팅 버튼--> [채팅]
                                            |
                                   사용자 입력 → /v1/chat/completions → NPC 답변
```

로그인 없이는 채팅 화면에 도달할 경로가 없다.

---

## 검증 (구현 후)

1. **빌드**: UE5에서 플러그인 C++ 컴파일 성공(`ChatSubsystem`, `AuthChatWidget` 링크 오류 없음)
2. **서버 연결**: `http://localhost:8080`에 llama.cpp 서버가 떠 있는 상태에서, 로그인 → 프로필 → 채팅 진입
3. **단발 질문**: "안녕?" 입력 → NPC 답변이 ChatLog에 표시됨
4. **멀티턴 확인**: "방금 뭐라고 했어?" 입력 → 직전 맥락을 반영한 답변이 오는지 확인(History 누적 동작 검증)
5. **게이팅 확인**: 로그아웃 상태에서 채팅 화면에 도달할 경로가 없는지 확인
6. **로그**: 서버 콘솔에 `/v1/chat/completions` 요청이 매 전송마다 들어오는지 확인
