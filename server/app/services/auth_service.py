"""인증 관련 비즈니스 로직.

라우터가 HTTP로 변환할 수 있도록 도메인 예외를 발생시킨다.
"""

import jwt
from sqlalchemy import select
from sqlalchemy.orm import Session

from app.core import security
from app.models import RefreshToken, User
from app.schemas.auth import SignupRequest


class AuthError(Exception):
    """인증 도메인 오류의 기반 클래스."""


class EmailAlreadyExists(AuthError):
    pass


class InvalidCredentials(AuthError):
    pass


class InactiveUser(AuthError):
    pass


class InvalidToken(AuthError):
    pass


def signup(db: Session, data: SignupRequest) -> User:
    exists = db.scalar(select(User).where(User.email == data.email))
    if exists is not None:
        raise EmailAlreadyExists()

    user = User(
        email=data.email,
        username=data.username,
        hashed_password=security.hash_password(data.password),
    )
    db.add(user)
    db.commit()
    db.refresh(user)
    return user


def authenticate(db: Session, email: str, password: str) -> User:
    user = db.scalar(select(User).where(User.email == email))
    if user is None or not security.verify_password(password, user.hashed_password):
        raise InvalidCredentials()
    if not user.is_active:
        raise InactiveUser()
    return user


def issue_tokens(db: Session, user: User) -> tuple[str, str]:
    """access/refresh 토큰을 발급하고 refresh 해시를 저장."""
    access = security.create_access_token(user.id)
    refresh, expires_at = security.create_refresh_token(user.id)
    db.add(
        RefreshToken(
            user_id=user.id,
            token_hash=security.hash_token(refresh),
            expires_at=expires_at,
        )
    )
    db.commit()
    return access, refresh


def _find_active_refresh(db: Session, refresh_token: str) -> RefreshToken:
    """전달된 refresh 토큰을 검증하고 유효한 DB 레코드를 반환."""
    try:
        payload = security.decode_token(refresh_token, "refresh")
    except jwt.PyJWTError as exc:  # 서명/만료/타입 오류
        raise InvalidToken() from exc

    record = db.scalar(
        select(RefreshToken).where(
            RefreshToken.token_hash == security.hash_token(refresh_token)
        )
    )
    if record is None or record.revoked:
        raise InvalidToken()
    if str(record.user_id) != payload.get("sub"):
        raise InvalidToken()
    return record


def refresh_tokens(db: Session, refresh_token: str) -> tuple[str, str]:
    """refresh 토큰 회전: 기존 무효화 후 새 access/refresh 발급."""
    record = _find_active_refresh(db, refresh_token)

    user = db.get(User, record.user_id)
    if user is None or not user.is_active:
        raise InvalidToken()

    record.revoked = True  # 회전: 기존 refresh 무효화
    access = security.create_access_token(user.id)
    new_refresh, expires_at = security.create_refresh_token(user.id)
    db.add(
        RefreshToken(
            user_id=user.id,
            token_hash=security.hash_token(new_refresh),
            expires_at=expires_at,
        )
    )
    db.commit()
    return access, new_refresh


def logout(db: Session, refresh_token: str) -> None:
    """전달된 refresh 토큰을 무효화. 이미 없거나 무효면 조용히 통과."""
    record = db.scalar(
        select(RefreshToken).where(
            RefreshToken.token_hash == security.hash_token(refresh_token)
        )
    )
    if record is not None and not record.revoked:
        record.revoked = True
        db.commit()


def withdraw(db: Session, user: User) -> None:
    """소프트 삭제: 비활성화 + 해당 유저의 모든 refresh 무효화."""
    user.is_active = False
    for token in user.refresh_tokens:
        token.revoked = True
    db.commit()
