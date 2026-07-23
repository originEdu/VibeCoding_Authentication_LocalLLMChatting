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
