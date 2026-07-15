import os

# app 모듈이 로드되기 전에 필수 설정을 환경변수로 주입한다.
os.environ.setdefault("DATABASE_URL", "sqlite://")
os.environ.setdefault("JWT_SECRET", "test-secret")
os.environ.setdefault("ACCESS_TOKEN_EXPIRE_MINUTES", "30")

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool

from app.core.database import Base, get_db
from app.main import app

# 단일 연결을 공유하는 인메모리 SQLite (테스트 세션 동안 스키마 유지)
engine = create_engine(
    "sqlite://",
    connect_args={"check_same_thread": False},
    poolclass=StaticPool,
)
TestingSessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False)


@pytest.fixture()
def client():
    Base.metadata.create_all(bind=engine)

    def override_get_db():
        db = TestingSessionLocal()
        try:
            yield db
        finally:
            db.close()

    app.dependency_overrides[get_db] = override_get_db
    with TestClient(app) as c:
        yield c
    app.dependency_overrides.clear()
    Base.metadata.drop_all(bind=engine)


# --- 헬퍼 ---
def register(client, email="alice@example.com", username="alice", password="password123"):
    return client.post(
        "/auth/signup",
        json={"email": email, "username": username, "password": password},
    )


def login(client, email="alice@example.com", password="password123"):
    return client.post("/auth/login", json={"email": email, "password": password})
