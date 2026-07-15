from collections.abc import Generator

from sqlalchemy import create_engine
from sqlalchemy.orm import DeclarativeBase, Session, sessionmaker

from app.core.config import settings

engine = create_engine(settings.database_url, pool_pre_ping=True)
SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False)


class Base(DeclarativeBase):
    """모든 ORM 모델의 기반 클래스."""


def get_db() -> Generator[Session, None, None]:
    """요청 단위 DB 세션을 제공하는 FastAPI 의존성."""
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()
