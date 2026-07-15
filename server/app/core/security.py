import hashlib
import uuid
from datetime import datetime, timedelta, timezone

import jwt
from passlib.context import CryptContext

from app.core.config import settings

_pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")


# --- 비밀번호 ---
def hash_password(plain: str) -> str:
    return _pwd_context.hash(plain)


def verify_password(plain: str, hashed: str) -> bool:
    return _pwd_context.verify(plain, hashed)


# --- JWT ---
def _create_token(subject: str, token_type: str, expires: timedelta, jti: str) -> str:
    now = datetime.now(timezone.utc)
    payload = {
        "sub": subject,
        "type": token_type,
        "jti": jti,
        "iat": now,
        "exp": now + expires,
    }
    return jwt.encode(payload, settings.jwt_secret, algorithm=settings.jwt_algorithm)


def create_access_token(user_id: int) -> str:
    expires = timedelta(minutes=settings.access_token_expire_minutes)
    return _create_token(str(user_id), "access", expires, uuid.uuid4().hex)


def create_refresh_token(user_id: int) -> tuple[str, datetime]:
    """refresh 토큰 원문과 만료 시각을 함께 반환."""
    expires = timedelta(days=settings.refresh_token_expire_days)
    token = _create_token(str(user_id), "refresh", expires, uuid.uuid4().hex)
    # DB(DATETIME) 저장·비교용으로 tz 정보를 제거한 UTC 시각을 반환
    expires_at = (datetime.now(timezone.utc) + expires).replace(tzinfo=None)
    return token, expires_at


def decode_token(token: str, expected_type: str) -> dict:
    """토큰을 검증·디코드. 서명/만료/타입 불일치 시 예외 발생."""
    payload = jwt.decode(
        token, settings.jwt_secret, algorithms=[settings.jwt_algorithm]
    )
    if payload.get("type") != expected_type:
        raise jwt.InvalidTokenError("unexpected token type")
    return payload


def hash_token(token: str) -> str:
    """refresh 토큰을 DB 저장/조회용으로 해시(SHA-256)."""
    return hashlib.sha256(token.encode()).hexdigest()
