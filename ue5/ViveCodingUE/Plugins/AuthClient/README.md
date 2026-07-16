# AuthClient (UE5 플러그인)

FastAPI 인증 서버(`../server`)와 통신하는 UE5 클라이언트. 회원가입/로그인/토큰 갱신/로그아웃/탈퇴를 제공하는 `UAuthSubsystem`과, 이를 사용하는 3개의 화면 위젯(로그인 / 회원가입 / 프로필)으로 구성된다.

화면 흐름:

```
WBP_Login ──[회원가입 버튼]──▶ WBP_Signup ──[가입 성공/뒤로]──▶ WBP_Login
    │                                                              
    └──[로그인 성공 / 자동 로그인]──▶ WBP_Profile ──[로그아웃]──▶ WBP_Login
```

화면 전환은 C++ 베이스 `UAuthScreenWidget::SwapTo()`가 담당한다. 각 위젯은 디테일 패널에서 이동할 다음 위젯 클래스를 지정한다.

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

## 화면 위젯 3개 만들기

각 C++ 클래스는 UI 로직만 담은 베이스다. 실제 화면은 에디터에서 구성한다.
위젯 이름은 아래와 **정확히 일치**해야 자동 바인딩된다. 배치 후 각 위젯의 **디테일 패널 → Auth|Navigation** 카테고리에서 이동할 위젯 클래스를 지정한다.

### 1) WBP_Login (부모: **AuthLoginWidget**)
- `EmailBox`, `PasswordBox` — **Editable Text Box** (PasswordBox는 `Is Password` 체크 권장)
- `LoginButton`, `SignupButton` — **Button**
- `StatusText` — **Text Block**
- 디테일 → `SignupWidgetClass` = **WBP_Signup**, `ProfileWidgetClass` = **WBP_Profile**

### 2) WBP_Signup (부모: **AuthSignupWidget**)
- `EmailBox`, `UsernameBox`, `PasswordBox` — **Editable Text Box**
- `SignupButton` — **Button**
- `BackButton` — **Button** (선택: 취소하고 로그인으로 복귀)
- `StatusText` — **Text Block**
- 디테일 → `LoginWidgetClass` = **WBP_Login**

### 3) WBP_Profile (부모: **AuthProfileWidget**)
- `InfoText` — **Text Block** (내 정보 표시. 생성 시 `GetMe` 자동 호출)
- `LogoutButton` — **Button**
- 디테일 → `LoginWidgetClass` = **WBP_Login**

### 시작 위젯 띄우기
GameMode(또는 레벨 블루프린트)에서 `Create Widget(WBP_Login)` → `Add to Viewport` + 마우스 커서/입력 모드 설정. 이후 전환은 C++가 자동 처리한다.

> 새 C++ 클래스(AuthScreenWidget/AuthSignupWidget/AuthProfileWidget)가 추가되었으므로 **Live Coding이 아니라 에디터를 닫고 전체 리빌드**해야 새 부모 클래스가 위젯 블루프린트 생성 목록에 나타난다.

## 수동 검증

1. 서버를 먼저 실행한다(`../server/README.md` 참고). MySQL 기동 + `alembic upgrade head` + `uvicorn`.
2. UE5 에디터에서 `WBP_Login`을 화면에 띄운다.
3. **회원가입** 버튼 → `WBP_Signup`으로 전환 → 이메일/유저네임/비밀번호 입력 → 가입 → "회원가입 성공" 후 `WBP_Login`으로 복귀.
4. 로그인 → 성공 시 `WBP_Profile`로 전환, `InfoText`에 내 정보(유저명 <이메일>) 표시.
5. **로그아웃** 버튼 → 세션 종료 후 `WBP_Login`으로 복귀.
6. PIE/에디터 재시작 후, 저장된 refresh 토큰으로 자동 로그인되면 `WBP_Login`이 곧바로 `WBP_Profile`로 넘어가는지 확인.

> 참고: HTTP는 비동기다. 결과는 반드시 델리게이트 콜백에서 처리하고, 호출 직후 토큰 상태를 즉시 조회하지 말 것.
