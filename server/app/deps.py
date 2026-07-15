import jwt
from fastapi import Depends, HTTPException, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from sqlalchemy.orm import Session

from app.core import security
from app.core.database import get_db
from app.models import User

_bearer = HTTPBearer(auto_error=True)


def get_current_user(
    credentials: HTTPAuthorizationCredentials = Depends(_bearer),
    db: Session = Depends(get_db),
) -> User:
    """access 토큰을 검증하고 활성 유저를 반환하는 의존성."""
    invalid = HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Invalid or expired token",
        headers={"WWW-Authenticate": "Bearer"},
    )
    try:
        payload = security.decode_token(credentials.credentials, "access")
    except jwt.PyJWTError:
        raise invalid

    user = db.get(User, int(payload["sub"]))
    if user is None or not user.is_active:
        raise invalid
    return user
