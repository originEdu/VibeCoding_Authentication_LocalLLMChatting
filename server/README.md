# Auth Server (FastAPI + MySQL + JWT)

회원가입 / 로그인 / 탈퇴(소프트 삭제)를 제공하는 인증 서버. Access + Refresh JWT를 사용하며 refresh 토큰은 DB에 해시로 저장해 무효화할 수 있다.

## 요구 사항

- Python 3.11+
- MySQL 8.x (실행 중이어야 함)

## 설정

```bash
cd server
python -m venv .venv
.venv\Scripts\activate            # Windows (PowerShell: .venv\Scripts\Activate.ps1)
pip install -r requirements.txt

copy .env.example .env             # 그리고 .env 의 값을 채운다
```

`.env` 예:
```
DATABASE_URL=mysql+pymysql://user:password@localhost:3306/auth_db
JWT_SECRET=<길고 임의의 시크릿>
```
MySQL에 `auth_db` 스키마를 미리 생성해 둔다:
```sql
CREATE DATABASE auth_db CHARACTER SET utf8mb4;
```

## 마이그레이션 & 실행

```bash
alembic upgrade head               # users, refresh_tokens 테이블 생성
uvicorn app.main:app --reload
```

Swagger UI: http://127.0.0.1:8000/docs

## API

| 메서드 | 경로 | 인증 | 설명 |
|--------|------|------|------|
| POST | `/auth/signup` | - | 회원가입 |
| POST | `/auth/login` | - | access/refresh 토큰 발급 |
| POST | `/auth/refresh` | - | refresh 토큰 회전 |
| POST | `/auth/logout` | Bearer(access) | refresh 토큰 무효화 |
| GET | `/auth/me` | Bearer(access) | 내 정보 |
| DELETE | `/auth/me` | Bearer(access) | 탈퇴(소프트 삭제 + 모든 refresh 무효화) |

## 테스트

로직 검증은 MySQL 없이 인메모리 SQLite로 실행된다:
```bash
pytest -q
```
