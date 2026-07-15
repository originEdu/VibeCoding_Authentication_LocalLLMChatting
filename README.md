# Auth Server + UE5 Client

언리얼 엔진 5에서 사용할 JWT 기반 인증 백엔드와, 그 API를 호출하는 UE5 클라이언트.

- **`server/`** — FastAPI + MySQL 인증 서버 (회원가입/로그인/탈퇴, Access+Refresh JWT). 실행·API·테스트는 `server/README.md` 참고.
- **`ue5/AuthClient/`** — UE5 플러그인. `UAuthSubsystem`(HTTP 연동) + `UAuthLoginWidget`(로그인 UI 베이스). 설치·사용은 `ue5/AuthClient/README.md` 참고.

두 파트는 하나의 REST API 계약을 공유한다(서버 README의 API 표 = UE5의 요청/응답).

설계 문서: `C:\Users\USER\.claude\plans\cozy-weaving-raven.md`
