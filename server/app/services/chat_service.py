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
