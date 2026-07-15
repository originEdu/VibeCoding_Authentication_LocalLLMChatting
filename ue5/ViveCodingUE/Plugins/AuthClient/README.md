# AuthClient (UE5 플러그인)

FastAPI 인증 서버(`../server`)와 통신하는 UE5 클라이언트. 회원가입/로그인/토큰 갱신/로그아웃/탈퇴를 제공하는 `UAuthSubsystem`과, 이를 사용하는 로그인 베이스 위젯 `UAuthLoginWidget`으로 구성된다.

## 설치

1. 이 `AuthClient` 폴더를 UE5 프로젝트의 `Plugins/` 아래로 복사한다.
   `MyProject/Plugins/AuthClient/`
2. `.uproject`를 우클릭 → **Generate Visual Studio project files**.
3. IDE(또는 에디터의 Live Coding)에서 빌드. `HTTP/Json/JsonUtilities/UMG` 모듈에 의존한다(Build.cs에 이미 선언됨).
4. 에디터 → **Edit > Plugins**에서 `AuthClient`가 활성화됐는지 확인.

## 사용 (C++ 또는 Blueprint)

`UAuthSubsystem`은 게임 인스턴스 서브시스템이라 어디서든 가져올 수 있다.

- C++: `GetGameInstance()->GetSubsystem<UAuthSubsystem>()`
- Blueprint: `Get Game Instance Subsystem (AuthSubsystem)` 노드

호출 함수: `Signup`, `Login`, `Refresh`, `Logout`, `Withdraw`, `GetMe`, `SetBaseUrl`.
결과 델리게이트(BlueprintAssignable): `OnLoginCompleted`, `OnSignupCompleted`, `OnLogoutCompleted`, `OnWithdrawCompleted`, `OnMeCompleted` — 각각 `(bool bSuccess, FString Message)`.

기본 서버 주소는 `http://127.0.0.1:8000`. 다르면 사용 초기에 `SetBaseUrl("http://<host>:<port>")` 호출.

토큰: 로그인/갱신 성공 시 access·refresh 토큰을 메모리에 보관하고 refresh 토큰을 SaveGame(`AuthClientTokens`)에 저장한다. 서브시스템 초기화 시 저장된 refresh 토큰이 있으면 자동으로 `Refresh()`를 시도한다.

## 로그인 위젯(WBP_Login) 만들기

`UAuthLoginWidget`은 UI 로직만 담은 C++ 베이스다. 실제 화면은 에디터에서 구성한다.

1. 콘텐츠 브라우저 → **Widget Blueprint** 생성 → 부모 클래스로 **AuthLoginWidget** 선택 → 이름 `WBP_Login`.
2. 디자이너 탭에서 아래 이름 그대로 위젯을 배치한다(이름이 정확히 일치해야 자동 바인딩됨):
   - `EmailBox`, `PasswordBox`, `UsernameBox` — **Editable Text Box**
     (PasswordBox는 Details에서 `Is Password` 체크 권장)
   - `LoginButton`, `SignupButton` — **Button** (안에 텍스트 라벨)
   - `StatusText` — **Text Block** (결과 메시지 표시용)
3. 레벨 블루프린트나 HUD에서 `Create Widget(WBP_Login)` → `Add to Viewport`, 마우스 커서/입력 모드 설정.

버튼 클릭 → 서브시스템 호출 → 델리게이트 결과가 `StatusText`에 표시된다.

## 수동 검증

1. 서버를 먼저 실행한다(`../server/README.md` 참고). MySQL 기동 + `alembic upgrade head` + `uvicorn`.
2. UE5 에디터에서 `WBP_Login`을 화면에 띄운다.
3. 이메일/유저네임/비밀번호 입력 → **회원가입** → StatusText에 "회원가입 성공" 확인.
4. **로그인** → "로그인 성공" 확인. `IsLoggedIn()`이 true.
5. 에디터 재시작(또는 PIE 재시작) 후, 저장된 refresh 토큰으로 자동 세션 갱신("세션 갱신됨")이 뜨는지 확인.
6. `Withdraw` 호출 후 재로그인 시 실패("Account is inactive")하는지 확인.

> 참고: HTTP는 비동기다. 결과는 반드시 델리게이트 콜백에서 처리하고, 호출 직후 토큰 상태를 즉시 조회하지 말 것.
