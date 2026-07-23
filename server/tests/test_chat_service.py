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


async def test_send_returns_reply(captured):
    reply = await chat_service.send(1, "merchant", "안녕")
    assert reply == "네, 나그네."


async def test_send_prepends_system_prompt(captured):
    await chat_service.send(1, "merchant", "안녕")
    assert captured[0][0]["role"] == "system"
    assert "상인" in captured[0][0]["content"]
    assert captured[0][-1] == {"role": "user", "content": "안녕"}


async def test_send_accumulates_history(captured):
    await chat_service.send(1, "merchant", "첫번째")
    await chat_service.send(1, "merchant", "두번째")
    second = captured[1]
    assert [m["content"] for m in second[1:]] == ["첫번째", "네, 나그네.", "두번째"]


async def test_reset_clears_history(captured):
    await chat_service.send(1, "merchant", "첫번째")
    chat_service.reset(1, "merchant")
    await chat_service.send(1, "merchant", "두번째")
    assert len(captured[1]) == 2  # system + user 만


async def test_history_is_per_user(captured):
    await chat_service.send(1, "merchant", "유저1의 말")
    await chat_service.send(2, "merchant", "유저2의 말")
    assert len(captured[1]) == 2  # 유저2에게는 유저1 이력이 없다


async def test_clear_user_removes_all_npcs(captured):
    await chat_service.send(1, "merchant", "안녕")
    chat_service.clear_user(1)
    assert chat_service._histories == {}


async def test_history_capped(captured):
    for i in range(15):  # 15턴 = user/assistant 30개
        await chat_service.send(1, "merchant", f"질문{i}")
    last = captured[-1]
    assert len(last) - 1 <= chat_service.MAX_HISTORY_MESSAGES  # system 제외


async def test_unknown_npc_raises(captured):
    with pytest.raises(npc.UnknownNpc):
        await chat_service.send(1, "no-such-npc", "안녕")


async def test_llm_connection_failure_is_502(monkeypatch):
    async def boom(self, *args, **kwargs):
        raise httpx.ConnectError("refused")

    monkeypatch.setattr(httpx.AsyncClient, "post", boom)
    with pytest.raises(HTTPException) as exc:
        await chat_service.send(1, "merchant", "안녕")
    assert exc.value.status_code == 502


async def test_llm_timeout_is_504(monkeypatch):
    async def slow(self, *args, **kwargs):
        raise httpx.ReadTimeout("too slow")

    monkeypatch.setattr(httpx.AsyncClient, "post", slow)
    with pytest.raises(HTTPException) as exc:
        await chat_service.send(1, "merchant", "안녕")
    assert exc.value.status_code == 504


async def test_failed_llm_call_does_not_pollute_history(monkeypatch):
    async def boom(self, *args, **kwargs):
        raise httpx.ConnectError("refused")

    monkeypatch.setattr(httpx.AsyncClient, "post", boom)
    with pytest.raises(HTTPException):
        await chat_service.send(1, "merchant", "안녕")
    assert chat_service._histories.get((1, "merchant"), []) == []
