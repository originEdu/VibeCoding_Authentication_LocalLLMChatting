# 4인 협동 풍선 타워 디펜스 — 구현 플랜 / 진행 상황

설계: [2026-07-30-4p-balloon-td-design.md](../specs/2026-07-30-4p-balloon-td-design.md)

> **상태: 작업 보류 중 (2026-07-30).**
> C++는 상당 부분 작성됐지만 **한 번도 컴파일된 적이 없다.** 아래 "재개 시 첫 단계"부터 시작할 것.

---

## 재개 시 첫 단계

1. **UE 에디터를 닫는다.** Live Coding이 활성이면 빌드가 `Unable to build while Live Coding is active`로 실패한다 (직전 시도에서 exit code 6).
2. 컴파일한다:
   ```powershell
   & "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" ViveCodingUEEditor Win64 Development -project="C:\Work\VibeCoding\ue5\ViveCodingUE\ViveCodingUE.uproject" -waitmutex
   ```
3. 컴파일 에러를 잡는다. 미검증 API 두 가지가 1순위 용의자:
   - `SetNetUpdateFrequency()` / `SetMinNetUpdateFrequency()` — 5.8에 존재한다고 조사됐으나 실제 호출은 미검증. 직접 대입(`NetUpdateFrequency = x`)은 private + `UE_DEPRECATED(5.5)` + V7 빌드세팅이라 확실히 에러다
   - `UDataTable::GetAllRows<FTDWaveRow>()` 시그니처
4. 미작성 파일을 마저 쓴다 (아래 목록).

---

## 진행 상황

### 완료 (작성됨, 미컴파일)

| 파일 | 내용 |
|---|---|
| `Source/ViveCodingUE/ViveCodingUE.Build.cs` | EnhancedInput/UMG public, Slate/SlateCore/OnlineSubsystem/OnlineSubsystemUtils private. 죽은 주석 블록 제거 |
| `ViveCodingUE.uproject` | OnlineSubsystem, OnlineSubsystemUtils, OnlineSubsystemNull, OnlineSubsystemSteam, SteamSockets 플러그인 추가. `EngineAssociation: "5.8"` 그대로 |
| `Config/DefaultEngine.ini` | GameSession MaxPlayers=4, OnlineSubsystem Steam(AppId 480), OnlineSubsystemNULL, 기존 `[/Script/Engine.Engine]`에 NetDriverDefinitions 3줄 |
| `TDTypes.h` / `.cpp` | `LogTD`, `ETDPhase`, `ETDPlaceResult`, `FTDWaveRow` |
| `TDGameState.h` / `.cpp` | 복제 Phase/WaveNumber/CoreHealth/CoreMaxHealth/BalloonsRemaining. BeginPlay에서 Lanes + Core 로컬 수집. 서버 setter는 private + `friend ATDGameMode` |
| `TDPlayerState.h` / `.cpp` | 복제 Gold / bIsHost, `OnGoldChanged` |
| `TDLane.h` / `.cpp` | LaneSpline, `GetLength`/`GetLocationAtDistance`/`GetDistanceToSpline`, OnConstruction 도로 생성(콜리전 off) |
| `TDCore.h` / `.cpp` | 메시 + MaxHealth + Radius, 비복제 |
| `TDBalloon.h` / `.cpp` | 서버 스플라인 전진, `DistanceAlongPath` 1Hz 복제, `SetReplicateMovement(false)`, 클라 로컬 적분 + PendingError 흡수, `ApplyDamage` |
| `TDTower.h` / `.cpp` | `SnapToGrid`/`CheckPlacement` static, 사거리 내 `DistanceAlongPath` 최대값 타겟팅, 서버 사격, 로컬 조준 |
| `TDGameMode.h` / `.cpp` | 페이즈 머신, PostLogin 방장 판정, `RequestStartWave`, 웨이브 스폰 타이머, `OnBalloonRemoved`, `RequestPlaceTower` 전체 검증 |
| `TDPlayerController.h` | 헤더만. RPC 3종 + 배치 모드 + 고스트 선언 |

### 미작성

| 파일 | 내용 |
|---|---|
| `TDPlayerController.cpp` | BeginPlay 입력 모드/IMC, `SetupInputComponent`, `PlayerTick` 고스트 갱신, `ServerPlaceTower` + `_Validate`(경계 `\|X\|,\|Y\|<=10000`, `\|Z\|<=1000`, null 클래스 거부) + 0.1s 레이트 리밋, `ServerRequestStartWave`, `ClientPlacementResult`, `GetCursorGroundLocation`(LinePlaneIntersection) |
| `TDCameraPawn.h` / `.cpp` | SpringArm(4000, pitch −60) + Camera, `IA_Pan`/`IA_Zoom`, 로컬 클램프 `\|X\|,\|Y\|<=4500` / arm 2000~8000 |
| `TDDebug.cpp` | `TD.DumpState` — `FAutoConsoleCommandWithWorld`. 넷모드/페이즈/웨이브/코어HP/풍선수/레인길이/PlayerState별(이름·골드·방장)/타워수/`NetDriver InBytes·OutBytes·InPackets·OutPackets` |
| `TDHUDWidget.h` / `.cpp` | `BindWidget`: `CoreHealthBar`, `CoreHealthText`, `WaveText`, `GoldText`, `PhaseText`, `BalloonsText`, `StartWaveButton` |
| `TDGameOverWidget.h` / `.cpp` | `BindWidget`: `ResultText`, `WaveReachedText`, `ReturnButton` (승/패 겸용) |
| `TDSessionSubsystem.h` / `.cpp` | `IOnlineSession` 래퍼. Create/Find/Join/Destroy + `VCTD` 필터 키 |
| `TDSessionMenuWidget` / `TDSessionRowWidget` | `BindWidget`: `HostButton`/`RefreshButton`/`SessionList`/`StatusText`/`LanCheckBox`, 행은 `NameText`/`PlayersText`/`JoinButton` |

---

## 사용자가 에디터에서 해야 할 일 (아직 아무것도 안 함)

C++가 컴파일된 뒤에 시작할 것. `BP_TDGameMode`가 `DT_Waves`를 참조하고 `DT_Waves`는 `FTDWaveRow`를 필요로 하므로 순서가 중요하다.

**자산 생성**
1. `DT_Waves` DataTable — 행 구조체 `FTDWaveRow`, 약 10행
2. 입력 자산 6개 (`/Game/TD/Input/`): `IMC_TD`, `IA_Pan`(Axis2D, WASD), `IA_Zoom`(Axis1D, 휠), `IA_PlaceTower`(좌클릭), `IA_CancelPlacement`(우클릭/Esc), `IA_ToggleBuild`(`1`) — IMC 매핑 배열은 중첩 구조체 + 모디파이어 오브젝트라 MCP로 밀어넣으면 조용히 실패한다
3. `BP_TDGameMode` (`ATDGameMode` 자식) — `WaveTable`, `BalloonClass`, `AllowedTowerClasses=[BP_TDTower]`, `StartingGold`, `DefaultPawnClass=ATDCameraPawn` 지정
4. `BP_TDLane` / `BP_TDCore` / `BP_TDTower` / `BP_TDBalloon` — 메시·머티리얼만, **그래프 로직 금지**
5. `WBP_TDHUD` / `WBP_TDGameOver` / `WBP_TDSessionMenu` / `WBP_TDSessionRow` — 위젯 이름을 `BindWidget`과 정확히 일치
6. 맵 `/Game/Maps/TDLobby` — 빈 맵, `AGameModeBase`, `WBP_TDSessionMenu` 표시
7. Fab `Content/Fab/balloon/balloon/SkeletalMeshes/Object_*` 중 하나 우클릭 → **Convert to Static Mesh**, 티어별 머티리얼(파랑/빨강/하늘/흰/노랑) 연결

**TDMap 편집**
8. `TopDownCamera`, `BalloonSpawner`, `BalloonPath` **삭제**
9. World Settings `DefaultGameMode`를 `BP_TDGameMode`로 (현재 `/Script/ViveCodingUE.BaseGM`)
10. `BP_TDLane` 4개 배치, `LaneIndex` 0/1/2/3, 각 스플라인을 맵 가장자리(±3800)→`(0,0,0)`
11. `BP_TDCore` 1개를 원점에
12. `PlayerStart` **4개** 배치 (현재 `PlayerStart_0` 하나뿐 — 4명이 한 지점에 스폰하면 콜리전 경고)

---

## 검증 순서 (각 단계 데이터 기반)

`TD.DumpState`를 만든 뒤 두 PIE 창에서 실행해 로그 블록을 diff하는 것이 전 단계 공통 도구다.

| # | 검증 대상 | 방법 |
|---|---|---|
| 0 | 빌드 설정 | `Build.bat` exit 0. 에디터 `-log`에서 `LogOnline` Steam init / `LogNet` 넷드라이버 정의 확인. `.uproject` diff가 Plugins hunk 하나뿐이고 `"EngineAssociation": "5.8"` 온전 |
| 1 | 프레임워크 골격 | PIE 2인 **Play As Listen Server**. 두 창 `TD.DumpState`: 같은 `Phase`, 다른 `PlayerName`, `bIsHost` 정확히 하나만 true, 골드 = `StartingGold` |
| 2 | 레인/코어 | 서버와 클라의 레인 4개 길이가 동일 (로컬 레지스트리 방식 증명) |
| 3 | 페이즈/방장 Start | **비방장** 클라에서 `ServerRequestStartWave` → `LogTD Warning: start denied (not host)`, 페이즈 불변 |
| 4 | 풍선 복제 | 서버 vs 클라 `DistanceAlongPath` 차이 `< Speed * 1.0s`. 100마리 웨이브 전후 `OutBytes` 델타가 **수백 B/s** (수백 KB/s면 `bReplicateMovement`를 안 끈 것) |
| 5 | 코어 데미지/승패 | 웨이브를 일부러 흘림 → 양쪽 머신에서 `CoreHealth` 감소, 0에서 `Phase -> Defeat`. 마지막 웨이브 클리어 → `Phase -> Victory` |
| 6 | 타워/배치/골드 | Build 중 배치 → 양쪽 타워 +1, **해당 플레이어만** 골드 −Cost. Wave 중 배치 → `WrongPhase`. allowlist 밖 클래스로 RPC → `IllegalClass`. 풍선 처치 → 타워 소유자 골드만 상승 |
| 7 | 입력/카메라 | 클릭마다 deproject/스냅 좌표 로그 → 두 좌표 모두 200의 배수 |
| 8 | HUD | WBP 컴파일 클린(모든 `BindWidget` 이름 일치 증명). 위젯 값 로그 vs 같은 머신 `TD.DumpState` diff |
| 9 | 세션 | `DefaultPlatformService=NULL`로 standalone 2개: `LogOnline`에 Create/Find/Join 성공, 호스트 `LogTD`에 `PostLogin` 2회 |
| 10 | Steam 실기기 | `RunUAT BuildCookRun` 패키징, PC 2대 + Steam 계정 2개. `LogNet`에 `IpNetDriver`가 아니라 `SteamSocketsNetDriver` 연결 |
| 11 | 정리 | `BP_BalloonSpawner`/`BP_BalloonPath`가 TDMap에서 미참조 확인. 삭제하지 않고 dead로 기록 (CLAUDE.md §3) |

2인 이상 테스트 3단계와 각 단계에서 Steam이 필요한지는 설계 문서의 "검증 전략" 참조.
