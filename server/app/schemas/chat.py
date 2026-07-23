from pydantic import BaseModel, Field


class ChatRequest(BaseModel):
    npc_id: str = Field(min_length=1, max_length=50)
    message: str = Field(min_length=1, max_length=2000)


class ChatResponse(BaseModel):
    reply: str


class ResetRequest(BaseModel):
    npc_id: str = Field(min_length=1, max_length=50)
