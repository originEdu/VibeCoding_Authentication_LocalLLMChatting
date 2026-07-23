# NPC 대화 FastAPI 경유 전환 — 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** UE5 클라이언트가 로컬 LLM을 직접 호출하던 구조를 없애고, 인증과 NPC 대화 모두 FastAPI 서버 하나만 거치게 한다.

**Architecture:** FastAPI에 `/chat`, `/chat/reset`을 추가해 JWT 검증 후 루프백(`127.0.0.1:8080`)의 llama.cpp로 프록시한다. NPC 성격은 서버의 `npcs.yaml`이, 대화 이력은 서버 프로세스 메모리가 소유한다. 클라의 `UChatSubsystem`은 이력과 주소를 모두 버리고 `AuthSubsystem`에서 주소·토큰을 받아 쓴다.

**Tech Stack:** FastAPI, httpx(AsyncClient), PyYAML, SQLAlchemy, pytest / UE 5.8 C++ (HTTP 모듈, GameInstanceSubsystem)

**설계 문서:** `docs/superpowers/specs/2026-07-23-fastapi-chat-proxy-design.md`

## Global Constraints

- 서버 작업 디렉터리는 `C:\Work\VibeCoding\server`. 클라는 `C:\Work\VibeCoding\ue5\ViveCodingUE`.
- 기존 서버 레이어 구조를 따른다: `routers / schemas / services / models / core`.
- 오류 응답은 기존 컨벤션대로 `{"detail": "<영문 문자열>"}`. 사용자에게 보일 한국어 변환은 클라가 한다.
- 대화 이력은 프로세스 메모리에만 둔다. DB 영속화·마이그레이션 금지.
- **uvicorn은 `--workers 1`로 운영한다.** 이력이 프로세스 메모리에 있어 워커가 여러 개면 대화가 갈린다.
- llama.cpp는 `127.0.0.1:8080` 바인딩을 유지한다. 방화벽을 열거나 `--host 0.0.0.0`으로 바꾸지 않는다.
- 이력 상한: LLM에 보내는 대화 메시지(system 프롬프트 제외)는 최대 20개.
- 서버 LLM 타임아웃 110초 < 클라 HTTP 타임아웃 120초. 이 대소 관계를 뒤집지 않는다.
- UE 빌드 검증 명령:
  ```
  & "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" ViveCodingUEEditor Win64 Development -project="C:\Work\VibeCoding\ue5\ViveCodingUE\ViveCodingUE.uproject" -waitmutex
  ```
  성공 시 로그 끝에 `Result: Succeeded`. 로그를 `grep error`로 훑지 말 것 — UHT 커맨드라인의 `-WarningsAsErrors` 문자열이 오탐으로 잡힌다. `[N/M] Compile` 뒤의 error 라인을 본다.

## File Structure

**서버 (신규)**

| 파일 | 책임 |
|---|---|
| `app/npcs.yaml` | NPC id → 이름 + 성격 텍스트 |
| `app/services/npc.py` | yaml 로드, `get_personality()`, `UnknownNpc` |
| `app/services/chat_service.py` | 이력 보관·상한, 프롬프트 조립, LLM 호출 |
| `app/schemas/chat.py` | `ChatRequest` / `ChatResponse` / `ResetRequest` |
| `app/routers/chat.py` | `/chat`, `/chat/reset` |
| `tests/test_chat.py` | 위 전부의 테스트 |

**서버 (수정)**

| 파일 | 변경 |
|---|---|
| `app/core/config.py` | `llm_base_url`, `llm_timeout_seconds` 추가 |
| `app/main.py` | `chat.router` 등록 |
| `app/routers/auth.py` | 로그아웃 시 `chat_service.clear_user()` 호출 |
| `requirements.txt` | `pyyaml` 추가, `httpx`를 정식 의존성으로 승격 |

**클라 (수정)**

| 파일 | 변경 |
|---|---|
| `Source/AuthClient/Public/AuthSubsystem.h` | `GetBaseUrl()`, `GetAccessToken()` 게터 |
| `Source/AuthClient/Public/ChatSubsystem.h` | 이력·주소·프롬프트 제거, NpcId + 401 재시도 상태 |
| `Source/AuthClient/Private/ChatSubsystem.cpp` | 위에 맞춰 전면 수정 |
| `Source/AuthClient/Public/AuthChatWidget.h` | `NpcPersonality` → `NpcId` |
| `Source/AuthClient/Private/AuthChatWidget.cpp` | `SetNpcId` + `ResetConversation` 호출 |

---

### Task 1: 서버 설정과 NPC 정의

**Files:**
- Create: `server/app/npcs.yaml`
- Create: `server/app/services/npc.py`
- Create: `server/tests/test_npc.py`
- Modify: `server/app/core/config.py`
- Modify: `server/requirements.txt`

**Interfaces:**
- Consumes: 없음 (첫 태스크)
- Produces:
  - `app.services.npc.get_personality(npc_id: str) -> str`
  - `app.services.npc.UnknownNpc` (Exception)
  - `settings.llm_base_url: str`, `settings.llm_timeout_seconds: int`

> **참고:** `WBP_Chat.uasset`을 바이트 스캔한 결과 `NpcPersonality`에 저장된 성격 텍스트가 없다(한글/영문 프롬프트 문자열이 발견되지 않음). 즉 옮겨올 기존 내용이 없으므로 아래 yaml 내용이 첫 정의가 된다. 문구는 나중에 자유롭게 고치면 되며, `merchant`라는 id만 Task 6의 에디터 설정과 맞으면 된다.

- [ ] **Step 1: 의존성 추가**

`server/requirements.txt`에서 `httpx`를 dev 섹션에서 위로 올리고 `pyyaml`을 추가한다. 최종 내용:

```
fastapi
pydantic[email]
uvicorn[standard]
sqlalchemy>=2.0
alembic
pymysql
pydantic-settings
passlib[bcrypt]
bcrypt<4.1  # passlib 1.7.4 는 bcrypt 4.1+ 와 호환되지 않음
pyjwt
python-dotenv
httpx
pyyaml

# dev
pytest
```

설치:

```bash
cd /c/Work/VibeCoding/server && pip install -r requirements.txt
```

- [ ] **Step 2: 실패하는 테스트 작성**

`server/tests/test_npc.py`:

```python
import pytest

from app.services import npc


def test_get_personality_returns_text():
    text = npc.get_personality("merchant")
    assert "상인" in text


def test_get_personality_unknown_id_raises():
    with pytest.raises(npc.UnknownNpc):
        npc.get_personality("no-such-npc")
```

- [ ] **Step 3: 테스트가 실패하는지 확인**

```bash
cd /c/Work/VibeCoding/server && python -m pytest tests/test_npc.py -v
```

Expected: FAIL — `ModuleNotFoundError: No module named 'app.services.npc'`

- [ ] **Step 4: NPC 정의 파일 작성**

`server/app/npcs.yaml`:

```yaml
merchant:
  name: 마을 상인
  personality: |
    너는 중세 판타지 마을에서 잡화점을 운영하는 상인이다.
    농담을 즐기고 항상 돈 이야기로 화제를 돌린다.
    손님을 "나그네"라고 부른다.
    답변은 세 문장을 넘지 않는다.
```

- [ ] **Step 5: 로더 구현**

`server/app/services/npc.py`:

```python
from pathlib import Path

import yaml

_NPCS_PATH = Path(__file__).resolve().parents[1] / "npcs.yaml"


class UnknownNpc(Exception):
    """존재하지 않는 npc_id."""


# 서버 시작 시 한 번 읽는다. 수정하려면 서버를 재시작한다.
_npcs: dict[str, dict[str, str]] = (
    yaml.safe_load(_NPCS_PATH.read_text(encoding="utf-8")) or {}
)


def get_personality(npc_id: str) -> str:
    """NPC의 system 프롬프트를 반환. 없는 id면 UnknownNpc."""
    try:
        return _npcs[npc_id]["personality"]
    except KeyError:
        raise UnknownNpc(npc_id)
```

- [ ] **Step 6: 테스트 통과 확인**

```bash
cd /c/Work/VibeCoding/server && python -m pytest tests/test_npc.py -v
```

Expected: PASS (2 passed)

- [ ] **Step 7: 설정 추가**

`server/app/core/config.py`의 `Settings` 클래스에 두 줄을 추가한다. `refresh_token_expire_days` 아래, `model_config` 위:

```python
    llm_base_url: str = "http://127.0.0.1:8080"
    llm_timeout_seconds: int = 110
```

- [ ] **Step 8: 전체 테스트 통과 확인**

```bash
cd /c/Work/VibeCoding/server && python -m pytest -v
```

Expected: 기존 auth 테스트 + 새 npc 테스트 전부 PASS

- [ ] **Step 9: 커밋**

```bash
cd /c/Work/VibeCoding && git add server/app/npcs.yaml server/app/services/npc.py server/tests/test_npc.py server/app/core/config.py server/requirements.txt && git commit -m "Add NPC personality definitions loaded from npcs.yaml"
```

---

### Task 2: 대화 이력과 LLM 호출 서비스

**Files:**
- Create: `server/app/services/chat_service.py`
- Create: `server/tests/test_chat_service.py`

**Interfaces:**
- Consumes: `npc.get_personality(npc_id) -> str`, `npc.UnknownNpc`, `settings.llm_base_url`, `settings.llm_timeout_seconds`
- Produces:
  - `async chat_service.send(user_id: int, npc_id: str, message: str) -> str`
  - `chat_service.reset(user_id: int, npc_id: str) -> None`
  - `chat_service.clear_user(user_id: int) -> None`
  - `chat_service.MAX_HISTORY_MESSAGES: int` (= 20)
  - `async chat_service._call_llm(messages: list[dict[str, str]]) -> str` — 테스트가 monkeypatch하는 지점

- [ ] **Step 1: 실패하는 테스트 작성**

`server/tests/test_chat_service.py`:

```python
import httpx
import pytest
from fastapi import HTTPException

from app.services import chat_service, npc


@pytest.fixture(autouse=True)
def clear_histories():
    """이력이 모듈 전역이라 테스트마다 비운다."""
    chat_service._histories.clear()
    yield
    chat_service._histories.clear()


@pytest.fixture()
def captured(monkeypatch):
    """LLM 호출을 가로채 보낸 messages를 기록하고 고정 답변을 돌려준다."""
    calls: list[list[dict[str, str]]] = []

    async def fake_call_llm(messages):
        calls.append(messages)
        return "네, 나그네."

    monkeypatch.setattr(chat_service, "_call_llm", fake_call_llm)
    return calls


@pytest.mark.asyncio
async def test_send_returns_reply(captured):
    reply = await chat_service.send(1, "merchant", "안녕")
    assert reply == "네, 나그네."


@pytest.mark.asyncio
async def test_send_prepends_system_prompt(captured):
    await chat_service.send(1, "merchant", "안녕")
    assert captured[0][0]["role"] == "system"
    assert "상인" in captured[0][0]["content"]
    assert captured[0][-1] == {"role": "user", "content": "안녕"}


@pytest.mark.asyncio
async def test_send_accumulates_history(captured):
    await chat_service.send(1, "merchant", "첫번째")
    await chat_service.send(1, "merchant", "두번째")
    second = captured[1]
    assert [m["content"] for m in second[1:]] == ["첫번째", "네, 나그네.", "두번째"]


@pytest.mark.asyncio
async def test_reset_clears_history(captured):
    await chat_service.send(1, "merchant", "첫번째")
    chat_service.reset(1, "merchant")
    await chat_service.send(1, "merchant", "두번째")
    assert len(captured[1]) == 2  # system + user 만


@pytest.mark.asyncio
async def test_history_is_per_user(captured):
    await chat_service.send(1, "merchant", "유저1의 말")
    await chat_service.send(2, "merchant", "유저2의 말")
    assert len(captured[1]) == 2  # 유저2에게는 유저1 이력이 없다


@pytest.mark.asyncio
async def test_clear_user_removes_all_npcs(captured):
    await chat_service.send(1, "merchant", "안녕")
    chat_service.clear_user(1)
    assert chat_service._histories == {}


@pytest.mark.asyncio
async def test_history_capped(captured):
    for i in range(15):  # 15턴 = user/assistant 30개
        await chat_service.send(1, "merchant", f"질문{i}")
    last = captured[-1]
    assert len(last) - 1 <= chat_service.MAX_HISTORY_MESSAGES  # system 제외


@pytest.mark.asyncio
async def test_unknown_npc_raises(captured):
    with pytest.raises(npc.UnknownNpc):
        await chat_service.send(1, "no-such-npc", "안녕")


@pytest.mark.asyncio
async def test_llm_connection_failure_is_502(monkeypatch):
    async def boom(self, *args, **kwargs):
        raise httpx.ConnectError("refused")

    monkeypatch.setattr(httpx.AsyncClient, "post", boom)
    with pytest.raises(HTTPException) as exc:
        await chat_service.send(1, "merchant", "안녕")
    assert exc.value.status_code == 502


@pytest.mark.asyncio
async def test_llm_timeout_is_504(monkeypatch):
    async def slow(self, *args, **kwargs):
        raise httpx.ReadTimeout("too slow")

    monkeypatch.setattr(httpx.AsyncClient, "post", slow)
    with pytest.raises(HTTPException) as exc:
        await chat_service.send(1, "merchant", "안녕")
    assert exc.value.status_code == 504


@pytest.mark.asyncio
async def test_failed_llm_call_does_not_pollute_history(monkeypatch):
    async def boom(self, *args, **kwargs):
        raise httpx.ConnectError("refused")

    monkeypatch.setattr(httpx.AsyncClient, "post", boom)
    with pytest.raises(HTTPException):
        await chat_service.send(1, "merchant", "안녕")
    assert chat_service._histories.get((1, "merchant"), []) == []
```

- [ ] **Step 2: async 테스트 러너 추가**

`pytest.mark.asyncio`를 쓰려면 플러그인이 필요하다. `server/requirements.txt`의 dev 섹션에 추가:

```
# dev
pytest
pytest-asyncio
```

`server/pytest.ini`를 새로 만든다 (매 테스트에 `asyncio_mode` 데코레이터를 붙이지 않기 위함):

```ini
[pytest]
asyncio_mode = auto
```

설치:

```bash
cd /c/Work/VibeCoding/server && pip install -r requirements.txt
```

- [ ] **Step 3: 테스트가 실패하는지 확인**

```bash
cd /c/Work/VibeCoding/server && python -m pytest tests/test_chat_service.py -v
```

Expected: FAIL — `ModuleNotFoundError: No module named 'app.services.chat_service'`

- [ ] **Step 4: 서비스 구현**

`server/app/services/chat_service.py`:

```python
import httpx
from fastapi import HTTPException, status

from app.core.config import settings
from app.services import npc

# LLM에 보내는 대화 메시지 상한(system 프롬프트 제외).
# 없으면 대화가 길어질수록 모델 컨텍스트를 넘겨 요청이 실패한다.
MAX_HISTORY_MESSAGES = 20

# (user_id, npc_id) -> [{"role": ..., "content": ...}, ...]
# 프로세스 메모리. 서버 재시작 시 소실되며, uvicorn 워커 1개를 전제로 한다.
_histories: dict[tuple[int, str], list[dict[str, str]]] = {}


async def _call_llm(messages: list[dict[str, str]]) -> str:
    """llama.cpp의 OpenAI 호환 API를 호출하고 답변 텍스트를 반환."""
    payload = {"messages": messages, "stream": False, "temperature": 0.7}
    url = f"{settings.llm_base_url}/v1/chat/completions"
    try:
        async with httpx.AsyncClient(timeout=settings.llm_timeout_seconds) as client:
            res = await client.post(url, json=payload)
    except httpx.TimeoutException:
        raise HTTPException(
            status_code=status.HTTP_504_GATEWAY_TIMEOUT, detail="LLM timeout"
        )
    except httpx.HTTPError:
        raise HTTPException(
            status_code=status.HTTP_502_BAD_GATEWAY, detail="LLM unavailable"
        )

    unavailable = HTTPException(
        status_code=status.HTTP_502_BAD_GATEWAY, detail="LLM unavailable"
    )
    if res.status_code != 200:
        raise unavailable
    try:
        return res.json()["choices"][0]["message"]["content"].strip()
    except (KeyError, IndexError, TypeError, ValueError):
        raise unavailable


async def send(user_id: int, npc_id: str, message: str) -> str:
    """질문을 이력에 얹어 LLM에 보내고 답변을 반환. 없는 npc_id면 UnknownNpc."""
    personality = npc.get_personality(npc_id)
    history = _histories.setdefault((user_id, npc_id), [])

    # 이번 질문 자리를 남겨 상한을 넘지 않게 자른다.
    recent = history[-(MAX_HISTORY_MESSAGES - 1) :] if MAX_HISTORY_MESSAGES > 1 else []
    messages = (
        [{"role": "system", "content": personality}]
        + recent
        + [{"role": "user", "content": message}]
    )

    # 호출이 실패하면 이력을 건드리지 않는다. 다음 시도가 깨끗한 상태에서 시작한다.
    reply = await _call_llm(messages)

    history.append({"role": "user", "content": message})
    history.append({"role": "assistant", "content": reply})
    del history[:-MAX_HISTORY_MESSAGES]
    return reply


def reset(user_id: int, npc_id: str) -> None:
    """이 NPC와의 대화를 비운다. 없으면 조용히 통과."""
    _histories.pop((user_id, npc_id), None)


def clear_user(user_id: int) -> None:
    """이 유저의 모든 NPC 대화를 파기한다(로그아웃)."""
    for key in [k for k in _histories if k[0] == user_id]:
        del _histories[key]
```

- [ ] **Step 5: 테스트 통과 확인**

```bash
cd /c/Work/VibeCoding/server && python -m pytest tests/test_chat_service.py -v
```

Expected: PASS (11 passed)

- [ ] **Step 6: 커밋**

```bash
cd /c/Work/VibeCoding && git add server/app/services/chat_service.py server/tests/test_chat_service.py server/requirements.txt server/pytest.ini && git commit -m "Add chat service holding per-user NPC conversation history"
```

---

### Task 3: /chat, /chat/reset 엔드포인트

**Files:**
- Create: `server/app/schemas/chat.py`
- Create: `server/app/routers/chat.py`
- Create: `server/tests/test_chat.py`
- Modify: `server/app/main.py`

**Interfaces:**
- Consumes: `chat_service.send/reset`, `npc.UnknownNpc`, `app.deps.get_current_user`
- Produces: HTTP `POST /chat` → `{"reply": str}`, `POST /chat/reset` → 204

- [ ] **Step 1: 실패하는 테스트 작성**

`server/tests/test_chat.py`:

```python
import pytest

from app.services import chat_service
from tests.conftest import login, register


def auth_header(token: str) -> dict[str, str]:
    return {"Authorization": f"Bearer {token}"}


@pytest.fixture(autouse=True)
def clear_histories():
    chat_service._histories.clear()
    yield
    chat_service._histories.clear()


@pytest.fixture()
def captured(monkeypatch):
    calls: list[list[dict[str, str]]] = []

    async def fake_call_llm(messages):
        calls.append(messages)
        return "네, 나그네."

    monkeypatch.setattr(chat_service, "_call_llm", fake_call_llm)
    return calls


def token(client) -> str:
    register(client)
    return login(client).json()["access_token"]


def test_chat_without_token(client, captured):
    res = client.post("/chat", json={"npc_id": "merchant", "message": "안녕"})
    assert res.status_code in (401, 403)


def test_chat_success(client, captured):
    res = client.post(
        "/chat",
        json={"npc_id": "merchant", "message": "안녕"},
        headers=auth_header(token(client)),
    )
    assert res.status_code == 200
    assert res.json()["reply"] == "네, 나그네."


def test_chat_unknown_npc(client, captured):
    res = client.post(
        "/chat",
        json={"npc_id": "no-such-npc", "message": "안녕"},
        headers=auth_header(token(client)),
    )
    assert res.status_code == 404
    assert res.json()["detail"] == "Unknown NPC"


def test_chat_empty_message_rejected(client, captured):
    res = client.post(
        "/chat",
        json={"npc_id": "merchant", "message": ""},
        headers=auth_header(token(client)),
    )
    assert res.status_code == 422


def test_reset_clears_history(client, captured):
    header = auth_header(token(client))
    payload = {"npc_id": "merchant", "message": "첫번째"}
    client.post("/chat", json=payload, headers=header)

    res = client.post("/chat/reset", json={"npc_id": "merchant"}, headers=header)
    assert res.status_code == 204

    client.post("/chat", json={"npc_id": "merchant", "message": "두번째"}, headers=header)
    assert len(captured[-1]) == 2  # system + user 만


def test_reset_without_history_is_ok(client, captured):
    res = client.post(
        "/chat/reset", json={"npc_id": "merchant"}, headers=auth_header(token(client))
    )
    assert res.status_code == 204


def test_logout_clears_history(client, captured):
    register(client)
    tokens = login(client).json()
    header = auth_header(tokens["access_token"])
    client.post("/chat", json={"npc_id": "merchant", "message": "안녕"}, headers=header)
    assert chat_service._histories

    res = client.post(
        "/auth/logout", json={"refresh_token": tokens["refresh_token"]}, headers=header
    )
    assert res.status_code == 204
    assert chat_service._histories == {}
```

> 마지막 테스트는 Task 4에서 통과한다. Task 3 단계에서는 실패해도 정상이다.

- [ ] **Step 2: 테스트가 실패하는지 확인**

```bash
cd /c/Work/VibeCoding/server && python -m pytest tests/test_chat.py -v
```

Expected: 전부 FAIL — `/chat`이 없어 404가 돌아온다.

- [ ] **Step 3: 스키마 작성**

`server/app/schemas/chat.py`:

```python
from pydantic import BaseModel, Field


class ChatRequest(BaseModel):
    npc_id: str = Field(min_length=1, max_length=50)
    message: str = Field(min_length=1, max_length=2000)


class ChatResponse(BaseModel):
    reply: str


class ResetRequest(BaseModel):
    npc_id: str = Field(min_length=1, max_length=50)
```

- [ ] **Step 4: 라우터 작성**

`server/app/routers/chat.py`:

```python
from fastapi import APIRouter, Depends, HTTPException, status

from app.deps import get_current_user
from app.models import User
from app.schemas.chat import ChatRequest, ChatResponse, ResetRequest
from app.services import chat_service, npc

router = APIRouter(prefix="/chat", tags=["chat"])


# LLM 응답을 최대 110초 기다리므로 async 로 둔다.
# 동기 라우트면 그동안 FastAPI 스레드풀 워커 하나가 통째로 묶인다.
@router.post("", response_model=ChatResponse)
async def chat(
    data: ChatRequest, current_user: User = Depends(get_current_user)
) -> ChatResponse:
    try:
        reply = await chat_service.send(current_user.id, data.npc_id, data.message)
    except npc.UnknownNpc:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="Unknown NPC"
        )
    return ChatResponse(reply=reply)


@router.post("/reset", status_code=status.HTTP_204_NO_CONTENT)
def reset(
    data: ResetRequest, current_user: User = Depends(get_current_user)
) -> None:
    chat_service.reset(current_user.id, data.npc_id)
```

- [ ] **Step 5: 라우터 등록**

`server/app/main.py`를 다음으로 바꾼다:

```python
from fastapi import FastAPI

from app.routers import auth, chat

app = FastAPI(title="Auth Server", version="0.1.0")

app.include_router(auth.router)
app.include_router(chat.router)


@app.get("/health", tags=["health"])
def health() -> dict[str, str]:
    return {"status": "ok"}
```

- [ ] **Step 6: 테스트 확인**

```bash
cd /c/Work/VibeCoding/server && python -m pytest tests/test_chat.py -v
```

Expected: `test_logout_clears_history`만 FAIL, 나머지 6개 PASS

- [ ] **Step 7: 커밋**

```bash
cd /c/Work/VibeCoding && git add server/app/schemas/chat.py server/app/routers/chat.py server/app/main.py server/tests/test_chat.py && git commit -m "Add /chat and /chat/reset endpoints behind JWT auth"
```

---

### Task 4: 로그아웃 시 대화 이력 파기

**Files:**
- Modify: `server/app/routers/auth.py:57-64` (logout 핸들러)

**Interfaces:**
- Consumes: `chat_service.clear_user(user_id)`
- Produces: 없음 (동작 변경만)

- [ ] **Step 1: 실패하는 테스트 확인**

Task 3에서 이미 작성한 `test_logout_clears_history`가 실패 상태다.

```bash
cd /c/Work/VibeCoding/server && python -m pytest tests/test_chat.py::test_logout_clears_history -v
```

Expected: FAIL — `assert {(1, 'merchant'): [...]} == {}`

- [ ] **Step 2: import 추가**

`server/app/routers/auth.py`의 import 블록에서 `from app.services import auth_service`를 다음으로 바꾼다:

```python
from app.services import auth_service, chat_service
```

- [ ] **Step 3: logout 핸들러 수정**

`server/app/routers/auth.py`의 logout을 다음으로 바꾼다. `_`였던 의존성을 `current_user`로 이름 붙여 쓴다:

```python
@router.post("/logout", status_code=status.HTTP_204_NO_CONTENT)
def logout(
    data: LogoutRequest,
    db: Session = Depends(get_db),
    current_user: User = Depends(get_current_user),
) -> None:
    auth_service.logout(db, data.refresh_token)
    chat_service.clear_user(current_user.id)
```

- [ ] **Step 4: 전체 테스트 통과 확인**

```bash
cd /c/Work/VibeCoding/server && python -m pytest -v
```

Expected: 전부 PASS

- [ ] **Step 5: 커밋**

```bash
cd /c/Work/VibeCoding && git add server/app/routers/auth.py && git commit -m "Discard a user's chat history on logout"
```

---

### Task 5: UE5 클라이언트를 FastAPI 경유로 전환

C++ 변경은 서로 맞물려 있어(위젯이 `SetSystemPrompt`를 부르는 한 컴파일이 깨진다) 한 태스크에서 함께 처리한다. 자동 테스트가 없으므로 검증은 컴파일이다.

**Files:**
- Modify: `ue5/ViveCodingUE/Source/AuthClient/Public/AuthSubsystem.h:56-57`
- Modify: `ue5/ViveCodingUE/Source/AuthClient/Public/ChatSubsystem.h` (전면)
- Modify: `ue5/ViveCodingUE/Source/AuthClient/Private/ChatSubsystem.cpp` (전면)
- Modify: `ue5/ViveCodingUE/Source/AuthClient/Public/AuthChatWidget.h:65-70`
- Modify: `ue5/ViveCodingUE/Source/AuthClient/Private/AuthChatWidget.cpp:40-48`

**Interfaces:**
- Consumes: 서버의 `POST /chat`, `POST /chat/reset`
- Produces:
  - `UAuthSubsystem::GetBaseUrl() const -> const FString&`
  - `UAuthSubsystem::GetAccessToken() const -> const FString&`
  - `UChatSubsystem::SetNpcId(const FString&)`, `ResetConversation()`, `SendMessage(const FString&)`
  - `UAuthChatWidget::NpcId` (EditAnywhere FString)

- [ ] **Step 1: AuthSubsystem에 게터 추가**

`AuthSubsystem.h`의 `IsLoggedIn()` 선언 바로 아래(57행 뒤)에 추가한다:

```cpp
	/** 서버 base URL. ChatSubsystem 이 같은 주소를 쓰도록 여기가 단일 출처다. */
	const FString& GetBaseUrl() const { return BaseUrl; }

	/** 현재 access 토큰. 비어 있으면 미로그인. */
	const FString& GetAccessToken() const { return AccessToken; }
```

- [ ] **Step 2: ChatSubsystem.h 교체**

`ChatSubsystem.h` 전체를 다음으로 바꾼다:

```cpp
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
```

- [ ] **Step 3: ChatSubsystem.cpp 교체**

`ChatSubsystem.cpp` 전체를 다음으로 바꾼다:

```cpp
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
```

- [ ] **Step 4: AuthChatWidget.h 프로퍼티 교체**

`AuthChatWidget.h`의 65~70행(`NpcPersonality` 주석과 선언)을 다음으로 바꾼다:

```cpp
	/**
	 * 대화 상대 NPC id. 서버 app/npcs.yaml 의 키와 일치해야 한다(예: "merchant").
	 * 성격 텍스트 자체는 서버가 소유하므로 여기에는 id만 적는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Chat|NPC")
	FString NpcId;
```

- [ ] **Step 5: AuthChatWidget.cpp 호출 교체**

`AuthChatWidget.cpp`의 `NativeConstruct` 안 40~48행 블록을 다음으로 바꾼다:

```cpp
	if (UChatSubsystem* Chat = GetChat(this))
	{
		Chat->OnChatResponse.AddDynamic(this, &UAuthChatWidget::OnChatResult);

		// 이 NPC와의 새 대화 시작: 상대를 지정하고 서버에 남은 이전 이력을 비운다.
		// (한 번에 한 NPC와 대화하는 방식이라, 채팅 화면을 열 때마다 새 대화로 시작한다.)
		Chat->SetNpcId(NpcId);
		Chat->ResetConversation();
	}
```

`OnLogoutResult`의 `Chat->ClearHistory();`도 더 이상 없는 함수다. 해당 블록을 다음으로 바꾼다 (서버가 로그아웃 시 이력을 파기하므로 클라가 할 일이 없다):

```cpp
void UAuthChatWidget::OnLogoutResult(bool bSuccess, const FString& Message)
{
	// 서버가 로그아웃 시 이 유저의 대화 이력을 파기하므로 클라에서 따로 비울 것이 없다.
	SwapTo(LoginWidgetClass);
}
```

- [ ] **Step 6: 컴파일 확인**

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" ViveCodingUEEditor Win64 Development -project="C:\Work\VibeCoding\ue5\ViveCodingUE\ViveCodingUE.uproject" -waitmutex
```

Expected: 로그 끝에 `Result: Succeeded`. 실패하면 `[N/M] Compile` 뒤의 error 라인을 읽고 고친다.

- [ ] **Step 7: 커밋**

```bash
cd /c/Work/VibeCoding && git add ue5/ViveCodingUE/Source/AuthClient && git commit -m "Route NPC chat through FastAPI instead of calling the LLM directly"
```

---

### Task 6: 에디터 배선과 통합 검증

여기서부터는 사람이 해야 하는 작업이 섞인다. 에이전트는 서버 기동과 curl 검증까지 하고, 에디터 조작은 사용자에게 요청한다.

**Files:**
- Modify: `ue5/ViveCodingUE/Content/WBP/WBP_Chat.uasset` (에디터에서, 사람이)

**Interfaces:**
- Consumes: Task 1~5의 전부
- Produces: 동작하는 기능

- [ ] **Step 1: 서버 기동 (리눅스 PC)**

llama.cpp가 `127.0.0.1:8080`에 떠 있는지 확인하고, FastAPI를 워커 1개로 띄운다:

```bash
curl -s -o /dev/null -w "%{http_code}\n" http://127.0.0.1:8080/v1/models
uvicorn app.main:app --host 0.0.0.0 --port 8081 --workers 1
```

Expected: 첫 명령이 `200`. 아니면 llama.cpp부터 띄운다.

- [ ] **Step 2: 서버 왕복 검증 (개발 PC에서 curl)**

```bash
TOKEN=$(curl -s -X POST http://192.168.0.42:8081/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email":"<가입한 이메일>","password":"<비밀번호>"}' | python -c "import sys,json;print(json.load(sys.stdin)['access_token'])")

curl -s -X POST http://192.168.0.42:8081/chat \
  -H "Content-Type: application/json" -H "Authorization: Bearer $TOKEN" \
  -d '{"npc_id":"merchant","message":"안녕하세요"}'
```

Expected: `{"reply":"..."}` 형태의 NPC 답변. 여기서 실패하면 클라를 건드리기 전에 서버에서 원인을 잡는다.

- [ ] **Step 3: 에디터 배선 요청 (사용자 작업)**

사용자에게 다음을 요청한다:

> 에디터에서 `Content/WBP/WBP_Chat`을 열고, 디테일 패널의 **Chat|NPC → Npc Id**에 `merchant`를 입력한 뒤 컴파일·저장해 주세요.

- [ ] **Step 4: 플레이 검증 (사용자 작업)**

뷰포트 캡처는 쓰지 않는다. 사용자에게 다음 네 가지를 확인받는다:

1. 로그인 후 채팅 화면에서 질문 → NPC 답변이 표시된다.
2. 이어서 "방금 내가 뭐라고 했지?"를 물으면 앞 질문을 기억한다(서버 이력이 도는지).
3. 채팅 화면을 닫았다 다시 열고 같은 질문 → 기억하지 못한다(reset이 도는지).
4. 리눅스 PC에서 llama.cpp를 내리고 전송 → "NPC가 응답할 수 없는 상태입니다."가 뜬다.

- [ ] **Step 5: 커밋**

```bash
cd /c/Work/VibeCoding && git add ue5/ViveCodingUE/Content/WBP/WBP_Chat.uasset && git commit -m "Set WBP_Chat NPC id to merchant"
```

---

## 완료 조건

- `cd /c/Work/VibeCoding/server && python -m pytest -v` 전부 통과
- UE 빌드가 `Result: Succeeded`
- Task 6 Step 4의 네 가지 시나리오가 모두 확인됨
- llama.cpp는 여전히 `127.0.0.1`에만 바인딩되어 있고, 리눅스 PC에서 8080 포트를 LAN에 열지 않았다

## 알려진 이슈 (이 작업과 무관, 손대지 않음)

`Source/AuthClient/README.md`는 채팅을 아예 다루지 않으며, 이미 낡아 있다 — AuthClient를 플러그인이라 설명하고(실제로는 프로젝트 모듈), 기본 서버 주소를 `http://127.0.0.1:8000`으로 적어 둔다(실제 `192.168.0.42:8081`). 이번 변경과 무관한 기존 문제라 건드리지 않는다. 정리하려면 별도 작업으로 잡는다.
