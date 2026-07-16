# UE5 로컬 LLM NPC 대화 기능 설계

> 정해진 문답이 아닌, 사용자가 자유롭게 입력한 질문에 NPC가 답변하는 대화 기능. NPC의 두뇌는 `http://localhost:8080`에 떠 있는 로컬 LLM(llama.cpp + Gemma-4-E2B, OpenAI 호환 API)이며, UE5 C++에서 HTTP로 통신한다.

작성일: 2026-07-16 · 최종 갱신: 2026-07-16 (구현 완료 반영)

---

## 배경 (Context)

언리얼 프로젝트(`ue5/ViveCodingUE`)에 자유 입력형 NPC 대화 기능을 추가한다. 이 프로젝트에는 이미 `AuthClient` 플러그인이 있어 `FHttpModule` 기반 비동기 HTTP + JSON 파싱 + UMG 위젯 + 델리게이트 콜백 패턴이 완성되어 있다. 새 기능은 이 검증된 패턴을 거의 그대로 복제해 위험을 낮춘다.

### 결정된 요구사항

| 항목 | 결정 | 이유 |
| --- | --- | --- |
| 응답 수신 | **블로킹** (완성 응답 한 번에) | 기존 코드가 이미 이 방식 → 스트리밍 대비 구현 단순 |
| 대화 기억 | **멀티턴** (메시지 배열 누적) | 이전 질문/답변을 반영한 자연스러운 대화 |
| NPC 정체성 | **지금은 없음** | 나중에 한 줄로 채울 수 있게 빈 `SystemPrompt` 필드만 준비 |
| 로그인 게이팅 | **로그인 성공 후에만** 채팅 진입 | 채팅이 로그인 후의 메인 화면 |
| 프로필 열기 | **오버레이** (SwapTo 아님) | 채팅 위에 얹었다 닫으면 대화가 그대로 유지됨 |
| 배치 | **기존 `AuthClient` 플러그인에 추가** | 채팅 위젯이 `UAuthScreenWidget` 공유 헬퍼를 상속 → 동일 모듈이 가장 단순 |

---

## 화면 흐름 (구현 결과)

```
[WBP_Login] --로그인 성공--> [WBP_Chat] (메인 허브)
                                  │  ▲
                     프로필보기   │  │  돌아가기 = 오버레이 닫기
                                  ▼  │
                          [WBP_Profile] (채팅 위에 얹힘)
                                  │
                               로그아웃 --> 채팅이 이력 비우고 [WBP_Login] 으로 복귀
```

- 로그인 성공(및 저장된 토큰으로 자동 로그인) 시 곧바로 **채팅** 화면으로 이동한다.
- **프로필보기**는 채팅을 `SwapTo`로 교체하지 않고 그 위에 프로필을 `AddToViewport`로 얹는다. 채팅 인스턴스와 대화 로그가 살아있다.
- **돌아가기**는 프로필이 자기 자신을 `RemoveFromParent` 하여 아래 채팅을 그대로 다시 드러낸다.
- **로그아웃**은 프로필 오버레이를 닫고, 아래 채팅 화면이 대화 이력을 비운 뒤 로그인 화면으로 복귀한다.

---

## 구현 (파일별)

### 1. `UChatSubsystem` — `AuthSubsystem` 미러링
파일: `Plugins/AuthClient/Source/AuthClient/Public/ChatSubsystem.h`, `Private/ChatSubsystem.cpp`

- `UGameInstanceSubsystem` 상속, `AUTHCLIENT_API`
- 상태:
  - `FString BaseUrl` — 기본값 `http://localhost:8080`
  - `FString SystemPrompt` — 기본값 빈 문자열
  - `TArray<FChatMessage> History` — `FChatMessage { FString Role; FString Content; }`
- `UFUNCTION(BlueprintCallable)`:
  - `SendMessage(const FString& UserText)` — History에 `{"user", UserText}` 추가 → `/v1/chat/completions` 바디 조립(SystemPrompt 비어있지 않으면 맨 앞에 system 메시지 포함) → 요청 전송 + `HandleChat` 바인딩
  - `ClearHistory()`, `SetBaseUrl(const FString&)`, `SetSystemPrompt(const FString&)`
- `HandleChat(...)` (private) — `EHttpResponseCodes::IsOk` 확인 → `choices[0].message.content` 파싱 → History에 `{"assistant", reply}` 추가 → `OnChatResponse.Broadcast(true, reply)`; 실패 시 `Broadcast(false, 에러)`
- 델리게이트: `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FChatResult, bool, bSuccess, const FString&, Reply)` → `UPROPERTY(BlueprintAssignable) FChatResult OnChatResponse;`

**전송 바디 예시:**

```json
{ "messages": [{"role":"user","content":"안녕?"}], "stream": false, "temperature": 0.7 }
```

### 2. `UAuthChatWidget` — 로그인 후 메인 허브
파일: `Plugins/AuthClient/Source/AuthClient/Public/AuthChatWidget.h`, `Private/AuthChatWidget.cpp`

- `UAuthScreenWidget` 상속
- `UPROPERTY(meta=(BindWidget))`: `InputBox`(EditableTextBox), `SendButton`(Button), `ChatLog`(TextBlock), `StatusText`(TextBlock)
- `UPROPERTY(meta=(BindWidgetOptional))`: `ProfileButton`(Button) — 프로필 오버레이 열기
- 네비게이션 대상: `ProfileWidgetClass`(WBP_Profile), `LoginWidgetClass`(WBP_Login, 로그아웃 복귀용)
- `NativeConstruct`: 버튼 바인딩 + `ChatSubsystem::OnChatResponse` 구독 + `AuthSubsystem::OnLogoutCompleted` 구독 / `NativeDestruct`에서 해제
- `OnSendClicked`: 입력 읽기 → 빈 값 무시 → ChatLog에 "You: ..." 추가 → `SendMessage`
- `OnChatResult`: 성공 시 "NPC: ..." 추가
- `OnProfileClicked`: `CreateWidget(ProfileWidgetClass)` + `AddToViewport(ZOrder 10)` — **채팅은 그대로 두고 위에 얹음**
- `OnLogoutResult`: `ClearHistory()` 후 `SwapTo(LoginWidgetClass)`

### 3. `UAuthProfileWidget` — 채팅 위 오버레이
파일: `Plugins/AuthClient/Source/AuthClient/Public/AuthProfileWidget.h`, `Private/AuthProfileWidget.cpp`

- `InfoText`, `LogoutButton`(기존) + `BackButton`(BindWidgetOptional) 추가
- 화면 전환 대상 클래스 **불필요** — 오버레이라 닫기만 하면 됨
- `OnBackClicked`: `RemoveFromParent()` (오버레이 닫기 → 아래 채팅 노출)
- `OnLogoutResult`: `RemoveFromParent()` (오버레이만 닫음. 로그인 복귀는 채팅이 담당)
- 생성 시 `GetMe()`로 내 정보 갱신 표시(기존 동작 유지)

### 4. `UAuthLoginWidget` — 성공 시 채팅으로
파일: `.../AuthLoginWidget.h/.cpp`

- 로그인 성공(및 자동 로그인) 시 이동 대상을 `ProfileWidgetClass` → **`ChatWidgetClass`**(WBP_Chat)로 변경

---

## 에디터 배선 (코드로 불가)

| 위젯 블루프린트 | 부모 클래스 | 배치할 위젯(이름 일치) | 디테일 패널 클래스 지정 |
| --- | --- | --- | --- |
| `WBP_Login` | `AuthLoginWidget` | (기존) | `Chat Widget Class` = **WBP_Chat** |
| `WBP_Chat` | `AuthChatWidget` | `InputBox`, `SendButton`, `ChatLog`, `StatusText`, `ProfileButton` | `Profile Widget Class` = **WBP_Profile**, `Login Widget Class` = **WBP_Login** |
| `WBP_Profile` | `AuthProfileWidget` | `InfoText`, `LogoutButton`, `BackButton` | (없음) |

⚠️ 프로필 위젯은 **화면을 꽉 채우는 불투명 배경**을 둔다. 안 그러면 아래 채팅이 비쳐 보이고, 가려지지 않은 영역의 채팅 버튼이 클릭될 수 있다.

---

## 검증

- **컴파일**: `ViveCodingUEEditor` 빌드 성공 — `ChatSubsystem`, `AuthChatWidget`, `AuthProfileWidget`, `AuthLoginWidget` 컴파일·링크 완료 (에러/경고 0)
- **플레이 테스트** (`localhost:8080` 서버 실행 상태, 에디터 배선 후):
  1. 로그인 성공 → 채팅 화면 진입
  2. "안녕?" 입력 → NPC 답변이 ChatLog에 표시
  3. "방금 뭐라고 했어?" → 직전 맥락 반영(멀티턴 검증)
  4. 프로필보기 → 프로필이 채팅 위에 뜸 → 돌아가기 → **대화 로그가 그대로 유지된 채** 채팅 복귀
  5. 로그아웃 → 로그인 화면 복귀, 재로그인 시 이전 대화가 섞이지 않음
  6. 서버 콘솔에 매 전송마다 `/v1/chat/completions` 요청 확인
