# NPC 대화를 FastAPI 경유로 전환 — 설계

작성일: 2026-07-23

## 목표

UE5 클라이언트가 로컬 LLM(llama.cpp)을 **직접 호출하던 구조를 걷어내고**, 인증과 대화 모두 FastAPI 서버 하나만 거치게 한다. 클라가 아는 외부 주소는 `http://192.168.0.42:8081` 하나뿐이 된다.

대화 이력과 NPC 성격의 소유권도 클라에서 서버로 옮긴다.

## 성공 기준

1. 로그인 후 채팅 화면에서 질문을 보내면 NPC 답변이 돌아온다.
2. llama.cpp가 `127.0.0.1:8080`에 바인딩된 상태 그대로 동작한다. 방화벽을 열지 않는다.
3. 인증 토큰 없이 `/chat`을 호출하면 401.
4. 액세스 토큰이 만료된 뒤 메시지를 보내도 유저는 끊김을 느끼지 않는다(자동 refresh 후 재전송).
5. 채팅 화면을 다시 열면 이전 대화가 이어지지 않는다(새 대화).
6. `pytest`가 전부 통과하고, UE5 C++ 모듈이 에러 없이 컴파일된다.

## 배경 — 이 작업을 하게 된 이유

원래 구조는 클라가 LLM을 직접 호출했다. LLM이 같은 PC(`localhost`)에 있다는 전제였다.

LLM이 별도 리눅스 PC로 분리되면서 이 전제가 깨졌고, 다음이 드러났다:

| 문제 | 내용 |
|---|---|
| 주소 불일치 | `AuthSubsystem`은 `192.168.0.42:8081`인데 `ChatSubsystem`은 `127.0.0.1:8080`. 커밋 `18bacf3`이 auth 주소만 고쳤다. `SetBaseUrl`을 호출하는 에셋도 없어(Content 전체 바이트 스캔으로 확인) 하드코딩 값이 그대로 쓰였다. |
| 포트 미개방 | `192.168.0.42`의 8080은 LAN에서 연결 거부. llama.cpp가 `127.0.0.1`에 바인딩돼 있거나 방화벽이 막고 있다. |
| 인증 부재 | LLM을 LAN에 열면 누구나 인증 없이 호출 가능. 토큰 검증·rate limit·대화 로그가 불가능하다. |
| 프롬프트 노출 | NPC 성격(system 프롬프트)을 클라가 통째로 들고 있다. |

주소만 고치면 1·2번은 넘어가지만 3·4번은 남고, **두 개의 BaseUrl이 어긋날 수 있는 구조** 자체가 그대로다. 프록시로 바꾸면 네 가지가 한꺼번에 사라진다.

## 아키텍처

```
UE5 클라 ──/auth/*───────────┐
                             ├──> FastAPI 8081 ──(127.0.0.1:8080)──> llama.cpp
UE5 클라 ──/chat, /chat/reset ┘
                                  ↑ 유일한 외부 노출 지점
```

llama.cpp는 `127.0.0.1` 바인딩을 유지한다. FastAPI와 같은 리눅스 PC에 있으므로 루프백으로 붙는다.

## 결정 사항

| 항목 | 결정 | 이유 |
|---|---|---|
| 서버 역할 | 대화를 소유하는 프록시 | 얇은 패스스루면 NPC 성격이 여전히 클라에 남는다 |
| 이력 저장 | 프로세스 메모리 dict | DB 영속화는 현 단계에 과함 |
| 이력 초기화 | 클라가 명시적으로 `/chat/reset` | 기존 `ClearHistory()` 동작과 정확히 동일 |
| NPC 성격 | `app/npcs.yaml` | 긴 프롬프트를 여러 줄로 쓰기 쉽고, 기획 수정이 코드와 분리됨 |
| 클라 주소 출처 | `AuthSubsystem` 단독 | 주소가 한 곳에만 있으면 이번 같은 불일치가 구조적으로 불가능 |
| 토큰 만료 | 자동 refresh 후 1회 재시도 | 액세스 토큰 30분, 플레이 세션이 쉽게 넘긴다 |
| 스트리밍 | 쓰지 않음 | 현 UX가 "생각 중…" → 전체 답변. 필요해지면 그때 |

## 서버 구성

기존 `routers / schemas / services / models / core` 레이어를 그대로 따른다.

| 파일 | 역할 |
|---|---|
| `app/routers/chat.py` | `/chat`, `/chat/reset`. `get_current_user` 의존 |
| `app/services/chat_service.py` | 이력 보관, 프롬프트 조립, LLM 호출 |
| `app/services/npc.py` | `npcs.yaml` 로드 및 조회 |
| `app/schemas/chat.py` | `ChatRequest` / `ChatResponse` / `ResetRequest` |
| `app/npcs.yaml` | NPC id → 이름 + 성격 |

`app/core/config.py`에 설정 두 개를 추가한다:

- `llm_base_url: str = "http://127.0.0.1:8080"`
- `llm_timeout_seconds: int = 110`

`requirements.txt`에 `pyyaml`을 추가하고, dev 섹션에 있던 `httpx`를 정식 의존성으로 올린다(서버 런타임이 쓰게 되므로).

### `app/npcs.yaml`

```yaml
merchant:
  name: 마을 상인
  personality: |
    너는 중세 마을의 상인이다.
    농담을 즐기며 항상 돈 이야기를 꺼낸다.
    답변은 3문장을 넘지 않는다.
```

기존 `WBP_Chat`에 적혀 있던 성격 텍스트를 이 파일로 옮긴다. 서버 시작 시 한 번 읽고 메모리에 둔다. 수정하려면 서버를 재시작한다.

### `app/services/chat_service.py`

모듈 레벨 딕셔너리 하나로 이력을 들고 있는다.

```python
_histories: dict[tuple[int, str], list[dict[str, str]]] = {}
```

키는 `(user_id, npc_id)`. 값은 `{"role": ..., "content": ...}` 목록이며 **system 프롬프트는 넣지 않는다** — 요청 시점에 yaml에서 읽어 앞에 붙인다. 성격을 바꾸면 진행 중인 대화에도 바로 반영된다.

함수:

- `send(user_id, npc_id, message) -> str` — 이력에 user 메시지 추가 → system + 이력으로 LLM 호출 → 답변을 assistant로 추가 후 반환
- `reset(user_id, npc_id)` — 해당 대화 삭제
- `clear_user(user_id)` — 그 유저의 **모든** NPC 대화 삭제

LLM 호출은 `httpx.AsyncClient`로 `{llm_base_url}/v1/chat/completions`에 POST. 바디는 기존 클라가 보내던 것과 동일하다(`messages`, `stream: false`, `temperature: 0.7`).

`send`는 `async def`이고, `/chat` 라우트도 `async def`로 둔다. 기존 auth 라우트는 전부 동기 `def`라 이 부분만 다르다 — LLM 응답을 최대 110초 기다리는데, 동기 라우트로 두면 그동안 FastAPI 스레드풀 워커 하나가 통째로 묶인다. 동기 의존성(`get_db`, `get_current_user`)은 FastAPI가 알아서 스레드풀에서 실행하므로 async 라우트와 섞어 써도 문제없다.

#### 이력 상한

LLM 호출 직전, 이력이 20개를 넘으면 **뒤에서 20개만** 남긴다. system 프롬프트는 이력에 들어 있지 않으므로 잘릴 위험이 없다. 상한이 없으면 대화가 길어질수록 모델 컨텍스트를 넘겨 요청이 실패한다.

#### 운영 전제: uvicorn 워커 1개

이력이 프로세스 메모리에 있으므로 워커를 여러 개 띄우면 요청마다 다른 워커에 붙어 대화가 갈린다. **`--workers 1`로 운영한다.** 서버 재시작 시 진행 중이던 대화가 사라지는 것은 수용한다.

### API 계약

```
POST /chat            Authorization: Bearer <access>
  → {"npc_id": "merchant", "message": "안녕"}
  ← 200 {"reply": "어서오게, 나그네!"}

POST /chat/reset      Authorization: Bearer <access>
  → {"npc_id": "merchant"}
  ← 204
```

오류는 기존 `detail` 문자열 컨벤션을 따른다.

| 코드 | detail | 상황 |
|---|---|---|
| 401 | `Invalid or expired token` | 기존 `get_current_user`가 그대로 반환 |
| 404 | `Unknown NPC` | `npc_id`가 yaml에 없음 |
| 502 | `LLM unavailable` | llama.cpp 연결 실패 또는 비정상 응답 |
| 504 | `LLM timeout` | `llm_timeout_seconds` 초과 |

`/auth/logout`은 기존 동작에 더해 `chat_service.clear_user(current_user.id)`를 호출한다. 로그아웃한 유저의 대화는 NPC를 가리지 않고 전부 파기된다.

## 클라 변경

### `UAuthSubsystem`

게터 두 개만 추가한다. 주소와 토큰의 단일 출처가 된다.

- `const FString& GetBaseUrl() const`
- `const FString& GetAccessToken() const`

### `UChatSubsystem`

| 제거 | 이유 |
|---|---|
| `FString BaseUrl` | `AuthSubsystem`에서 가져온다 |
| `FString SystemPrompt` | 서버가 소유 |
| `TArray<FChatMessage> History` | 서버가 소유 |
| `SetBaseUrl()` / `SetSystemPrompt()` / `ClearHistory()` | 아래로 대체 |
| `BuildChatBody()` | 이력 조립이 사라짐 |

| 추가 | 내용 |
|---|---|
| `SetNpcId(const FString&)` | 대화 상대 지정 |
| `ResetConversation()` | `POST /chat/reset` |
| `FString NpcId` | 현재 대화 상대 |
| `FString PendingMessage` | 401 재시도용 보관 |
| `bool bRetriedAfterRefresh` | 재시도 1회 제한 |

`SendMessage`는 `{"npc_id": ..., "message": ...}`만 보낸다. 요청 URL은 `Auth->GetBaseUrl() + "/chat"`, 헤더에 `Authorization: Bearer <access>`.

타임아웃은 기존 값을 유지한다 — `SetTimeout(120)`과 **`SetActivityTimeout(120)` 둘 다** 필요하다. 서버를 경유해도 LLM이 생성하는 동안 아무 바이트도 오지 않으므로, 활동 타임아웃 기본값 30초에 걸려 끊긴다.

응답 파싱은 `choices[0].message.content`에서 `reply` 한 필드로 단순해진다.

### 401 재시도 흐름

```
SendMessage → 401 응답
  ├ bRetriedAfterRefresh == true  → 실패 통지("세션이 만료되었습니다") 후 종료
  └ false → PendingMessage 보관, bRetriedAfterRefresh = true
            Auth->OnLoginCompleted 일시 구독 → Auth->Refresh()
              ├ 성공 → 구독 해제, PendingMessage 재전송
              └ 실패 → 구독 해제, 실패 통지
```

성공적으로 응답을 받으면 `bRetriedAfterRefresh`를 false로 되돌린다. `AuthSubsystem::Refresh()`는 성공/실패 모두 `OnLoginCompleted`로 통지하므로 이 델리게이트 하나만 보면 된다.

### `UAuthChatWidget`

`NpcPersonality`(성격 원문) → `NpcId`(예: `"merchant"`)로 교체한다. `NativeConstruct`의 `SetSystemPrompt` + `ClearHistory` 호출은 `SetNpcId` + `ResetConversation`이 된다.

`ExtractError`에 새 detail 매핑을 추가한다:

| detail | 표시 메시지 |
|---|---|
| `Unknown NPC` | 알 수 없는 NPC입니다. |
| `LLM unavailable` | NPC가 응답할 수 없는 상태입니다. 잠시 후 다시 시도해 주세요. |
| `LLM timeout` | 응답이 너무 오래 걸립니다. 다시 시도해 주세요. |

### 에디터 작업 (사람이 해야 함)

`NpcPersonality` 프로퍼티를 제거하면 **`WBP_Chat`에 저장된 성격 텍스트가 소실된다.** 순서를 지켜야 한다:

1. 에디터에서 `WBP_Chat`의 현재 `NpcPersonality` 값을 복사해 둔다.
2. 그 내용을 `app/npcs.yaml`에 옮긴다.
3. C++ 변경 후 에디터에서 `WBP_Chat`의 `NpcId`를 지정한다.

1번은 사람만 할 수 있다. 구현을 시작하기 전에 값을 받아야 한다.

## 예외 처리

| 상황 | 동작 |
|---|---|
| 토큰 없이 `/chat` | 401, 기존 `get_current_user`가 처리 |
| 모르는 `npc_id` | 404 `Unknown NPC` |
| llama.cpp 미기동 | 502 `LLM unavailable` |
| LLM 응답 지연 | 서버가 110초에 끊고 504. 클라 타임아웃(120초)보다 짧게 잡아, 클라가 먼저 끊겨 원인 불명의 연결 실패로 보이는 것을 막는다 |
| 액세스 토큰 만료 | refresh 후 1회 재시도 |
| 이력 20개 초과 | 오래된 것부터 버림 |
| 이력 없는 상태에서 `/chat/reset` | 204. 없는 키 삭제는 오류가 아니다 |

## 검증

### 서버 — `tests/test_chat.py`

기존 `tests/conftest.py` 픽스처를 재사용하고, LLM은 `httpx` 호출을 monkeypatch로 대체한다.

1. 토큰 없이 `POST /chat` → 401
2. 모르는 `npc_id` → 404
3. 정상 요청 → 200, `reply` 반환
4. 두 번 연속 요청 → 두 번째 호출의 `messages`에 첫 대화가 포함됨(이력 누적 확인)
5. `/chat/reset` 후 요청 → `messages`에 system + 새 user만 있음
6. LLM 연결 실패 → 502
7. 유저 A의 이력이 유저 B 요청에 섞이지 않음
8. `/auth/logout` 후 → 이력이 비어 있음
9. 21개를 넘긴 이력 → LLM에 보내는 `messages`가 상한을 지킴

### 클라

자동 테스트가 없으므로 커맨드라인 빌드로 컴파일을 확인하고, 실제 플레이로 검증한다. 뷰포트 캡처는 쓰지 않는다.

1. 로그인 → 채팅 → 답변 수신
2. 채팅 화면을 닫았다 다시 열면 이전 대화가 이어지지 않음
3. llama.cpp를 내린 상태에서 전송 → "NPC가 응답할 수 없는 상태입니다"

## 범위 밖

- 대화 이력 DB 영속화
- 스트리밍 응답
- rate limit
- NPC 여러 명 동시 대화
- 관리 페이지에서 NPC 성격 수정

## 관련 정리

이 변경 후 llama.cpp를 LAN에 노출할 이유가 없어진다. `--host 0.0.0.0`으로 띄워 뒀다면 `127.0.0.1`로 되돌리는 편이 안전하다.
