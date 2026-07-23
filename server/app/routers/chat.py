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
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Unknown NPC")
    return ChatResponse(reply=reply)


@router.post("/reset", status_code=status.HTTP_204_NO_CONTENT)
def reset(data: ResetRequest, current_user: User = Depends(get_current_user)) -> None:
    chat_service.reset(current_user.id, data.npc_id)
