# 4인 협동 풍선 타워 디펜스 — 설계

작성일: 2026-07-30
대상: `ue5/ViveCodingUE` (UE 5.8)

## 배경

중앙에 HP를 가진 건물이 있고 동서남북 네 방향에서 풍선이 나온다. 풍선이 건물에 닿으면 HP가 깎이고 0이 되면 패배한다. 웨이브는 방장이 시작하며, 웨이브가 끝나면 타워를 배치하는 시간이 주어진다. 최대 4인 협동.

### 현재 프로젝트 상태

- 네트워킹 코드가 **0줄**. replication / RPC / GameState / PlayerState / OnlineSubsystem 전부 없음
- TD 프로토타입은 Blueprint 단일 플레이어만 존재: `BP_BalloonPath`(Spline), `BP_Balloon`(Tick 전진), `BP_BalloonSpawner`(Timer). HP·타워·웨이브 없음
- `ABaseGM : AGameModeBase`는 빈 클래스. 자식 `BP_GM`이 로그인 UI + `SetInputMode_UIOnlyEx`로 게임 입력을 막고 있어서 `TDMap`이 GameMode를 override해 회피 중
- `AuthClient` 모듈이 FastAPI(192.168.0.42:8081)에 JWT로 붙는 `UGameInstanceSubsystem` 패턴 보유 — 세션 서브시스템/위젯 작성 시 이 패턴을 그대로 따른다

## 확정 사항

| 항목 | 결정 |
|---|---|
| 아키텍처 | 리슨 서버 + 표준 UE replication (서버 권위) |
| 플레이어 조작 | 탑다운 카메라 폰 + 마우스 커서 배치. 캐릭터/네비메시 없음 |
| 접속 | `OnlineSubsystemSteam`. 단 코드는 `IOnlineSubsystem`/`IOnlineSession`만 사용 → Steam↔NULL 전환은 ini 한 줄 |
| 로그인 연동 | 이번 패스에서 제외. `BP_GM`/`MainMap`/`WBP_Login` 무수정 |
| 경제 | PlayerState별 개인 골드. 라스트힛 귀속 |
| 배치 시점 | Build 페이즈에만 |
| 풍선 | 단일 타입, 웨이브별 HP/속도/보상 스케일 (DataTable) |
| 웨이브 시작 | **무조건 방장이 Start.** 카운트다운 없음, 무한 대기 |
| 테이블 소진 | `ETDPhase::Victory` + 승리 화면 |
| 풍선 메시 | Fab SkeletalMesh 하나를 에디터에서 StaticMesh로 변환해 사용 |

### 검토했으나 채택하지 않은 대안

- **결정론적 풍선 시뮬레이션** — 풍선을 복제 액터로 만들지 않고 `SpawnServerTime` + `Speed`만 보내 클라가 위치를 재구성. 대역폭이 풍선 수와 무관하게 평탄해지지만 커스텀 코드가 훨씬 많고 슬로우/넉백 같은 속도 변경을 전부 복제 이벤트로 모델링해야 한다. 대역폭이 실제 문제로 드러나면 갈 업그레이드 경로로만 기록한다. 현재 설계의 1Hz 보정 채널이 그 전환 여지를 남겨둔다.
- **데디케이티드 서버 + FastAPI 로비** — 기존 JWT 인증 인프라 재활용, 호스트 어드밴티지 없음. 다만 Server target 추가 + 로비 엔드포인트 + 배포까지 범위가 훨씬 커서 4인 LAN 검증 후 별도 스펙으로 분리.

## 엔진 사실 (5.8 실물 검증 완료 — 튜토리얼과 다름)

1. **레거시 `SteamNetDriver` 클래스가 5.8에 없다.** `SteamSocketsNetDriver`만 존재. `[/Script/OnlineSubsystemSteam.SteamNetDriver]` ini 블록은 조용히 무시되므로 절대 넣지 말 것
2. **SteamSockets는 에디터에서 초기화되지 않는다** — `SteamSocketsModule.cpp:16`이 `IsRunningDedicatedServer() || IsRunningGame()`로 게이트. PIE는 자동으로 `DriverClassNameFallback`(`IpNetDriver`) 사용 → Steam 설정을 켜둔 채로 PIE 멀티 창 테스트가 그대로 된다
3. **`NetUpdateFrequency` 직접 대입은 컴파일 에러.** `Actor.h:903-910`에서 private + `UE_DEPRECATED(5.5)`, 두 Target 파일 모두 `BuildSettingsVersion.V7`. `SetNetUpdateFrequency()` / `SetMinNetUpdateFrequency()` 사용
4. `AActor::bReplicateMovement` 기본값이 **true**. 끄지 않으면 `FRepMovement`가 100Hz로 나가서 설계 대비 ~200배 대역폭을 먹는데 게임은 정상 동작하므로 버그로 안 보인다
5. `IOnlineSubsystem::Get()`은 5.8에서 deprecated 아님 (`OnlineSubsystem.h:168`)
6. EnhancedInput은 `EnabledByDefault: true` — `.uproject` 항목 불필요, Build.cs 의존성만 추가

---

## 클래스 구성

전부 `ViveCodingUE` 모듈, `BaseGM.h` 옆에 평면 배치, `TD` 접두사. (`AuthClient`는 HTTP/JWT 전용이라 섞지 않는다.)

**`ABaseGM`은 손대지 않고 형제 클래스 `ATDGameMode`를 추가한다.** `ABaseGM`은 로그인 GameMode `BP_GM`의 조상이라, 여기에 웨이브 로직을 넣으면 로그인 흐름까지 오염된다. TD 쪽 변경은 `TDMap` World Settings를 `BP_TDGameMode`로 바꾸는 것 하나뿐.

`AGameMode`가 아니라 `AGameModeBase` — 자체 페이즈 머신을 쓰므로 `MatchState`는 경쟁하는 두 번째 상태 머신이 된다.

| 클래스 | 부모 | 복제 | 역할 |
|---|---|---|---|
| `ATDGameMode` | `AGameModeBase` | 서버 전용 | 웨이브 진행, 스폰, 페이즈 전환, 모든 권위 검증 |
| `ATDGameState` | `AGameStateBase` | 복제 | HUD가 읽는 유일한 복제 스냅샷 |
| `ATDPlayerState` | `APlayerState` | 복제 | 개인 골드, 방장 플래그 |
| `ATDPlayerController` | `APlayerController` | 소유 클라 + 서버 RPC | 입력, 커서 트레이스, 배치 RPC |
| `ATDCameraPawn` | `APawn` | 존재만 복제, 이동 복제 X | 탑다운 카메라 + 로컬 팬/줌 |
| `ATDLane` | `AActor` | **복제 안 함** (레벨 액터) | 스플라인 경로, 도로 메시, 거리→위치 질의 |
| `ATDCore` | `AActor` | **복제 안 함** | 메시 + 반경. HP는 GameState에 |
| `ATDBalloon` | `AActor` | 복제 | 서버 이동, 클라 추측항법 |
| `ATDTower` | `AActor` | 복제 | 서버 전용 사격, 타겟만 복제 |
| `UTDSessionSubsystem` | `UGameInstanceSubsystem` | — | `IOnlineSession` 래퍼. `UAuthSubsystem` 형태 그대로 |
| `UTDHUDWidget` / `UTDGameOverWidget` / `UTDSessionMenuWidget` / `UTDSessionRowWidget` | `UUserWidget` | — | `BindWidget` 베이스 |
| `TDTypes.h` | — | — | `ETDPhase`, `ETDPlaceResult`, `FTDWaveRow`, `LogTD` |

### 복제 프로퍼티

```cpp
// ATDGameState
UPROPERTY(ReplicatedUsing=OnRep_Phase) ETDPhase Phase;   // Build / Wave / Defeat / Victory
UPROPERTY(Replicated) int32 WaveNumber;
UPROPERTY(ReplicatedUsing=OnRep_CoreHealth) int32 CoreHealth;
UPROPERTY(Replicated) int32 CoreMaxHealth;      // COND_InitialOnly
UPROPERTY(Replicated) int32 BalloonsRemaining;
// 비복제, 서버·클라 양쪽 BeginPlay에서 로컬 수집
TArray<TObjectPtr<ATDLane>> Lanes;              // TActorIterator, LaneIndex 정렬
TObjectPtr<ATDCore> Core;
```
카운트다운을 쓰지 않으므로 `PhaseEndServerTime` / `GetPhaseTimeRemaining()` / `TimerText`는 **넣지 않는다.**

**코어 HP는 GameState에만 둔다.** `ATDCore`에도 두면 진실이 둘이 되고, `ATDCore`는 어차피 `bAlwaysRelevant`가 필요해진다. `ATDCore`는 메시 + `EditAnywhere MaxHealth`(GameMode가 1회 읽음) + `Radius`만 가진 비복제 배치 액터.

리슨 서버에서는 서버 자신에게 OnRep이 돌지 않으므로, GameState/PlayerState의 서버 쓰기 경로(`SetPhase`, `SetCoreHealth`, `SetGold` 등)가 직접 OnRep 함수를 호출해 델리게이트를 브로드캐스트한다. 이 setter들은 `private` + `friend class ATDGameMode`로 막아 서버 권위를 구조적으로 강제한다.

```cpp
// ATDPlayerState
UPROPERTY(ReplicatedUsing=OnRep_Gold) int32 Gold;
UPROPERTY(Replicated) bool bIsHost;

// ATDBalloon
UPROPERTY(Replicated) uint8 LaneIndex;                    // COND_InitialOnly
UPROPERTY(ReplicatedUsing=OnRep_Tier) uint8 Tier;         // COND_InitialOnly
UPROPERTY(Replicated) float Speed;                        // COND_InitialOnly
UPROPERTY(ReplicatedUsing=OnRep_Distance) float DistanceAlongPath;
// 생성자: bReplicates=true; SetReplicateMovement(false); bAlwaysRelevant=true;
//         SetNetUpdateFrequency(1.f); SetMinNetUpdateFrequency(1.f);
// 콜리전 컴포넌트 없음. Health는 서버 전용 비복제 (HP바 없음)

// ATDTower
UPROPERTY(Replicated) TObjectPtr<ATDPlayerState> OwningPlayerState;  // COND_InitialOnly
UPROPERTY(Replicated) TObjectPtr<ATDBalloon> CurrentTarget;          // 클라가 로컬로 조준
// SetNetUpdateFrequency(5.f). 스탯(Cost/Range/Damage/FireInterval)은 EditDefaultsOnly,
// DataTable 안 씀 — 타워 1종뿐이라 1행짜리 테이블은 투기적 추상화

// ATDPlayerController
UFUNCTION(Server, Reliable, WithValidation)
void ServerPlaceTower(TSubclassOf<ATDTower> TowerClass, FVector_NetQuantize10 Location);
UFUNCTION(Server, Reliable) void ServerRequestStartWave();
UFUNCTION(Client, Reliable) void ClientPlacementResult(ETDPlaceResult Result);
```

### BP 처리

| 자산 | 처리 | 이유 |
|---|---|---|
| `BP_Balloon` | → C++ `ATDBalloon`, BP 폐기 | `GetLifetimeReplicatedProps`/`OnRep`/`HasAuthority` 필요 + 100마리 매 프레임 핫루프 |
| `BP_BalloonPath` | → C++ `ATDLane`, BP 폐기 | 서버가 매 프레임 풍선마다 거리 질의 — 게임에서 가장 뜨거운 호출 |
| `BP_BalloonSpawner` | TDMap에서 제거, `ATDGameMode`에 흡수 | 스폰 타이밍이 웨이브 데이터 + 페이즈 함수가 됨 |
| `BP_GM`/`WBP_Login`/`MainMap` | 무수정 | 로그인 흐름 분리 결정 |
| `BP_TDGameMode`, `BP_TDLane`, `BP_TDCore`, `BP_TDTower`, `BP_TDBalloon` | 신규 BP 자식 (얇게, 그래프 로직 없음) | 자산 참조 보관용. `ConstructorHelpers` 하드코딩 경로는 이 저장소에 전례 없고 취약 |

**`BP_BalloonPath`를 `ATDLane`으로 reparent 하지 말 것.** BP의 스플라인 컴포넌트 이름이 `PathSpline`이라 C++ 부모가 동명 컴포넌트를 선언하면 SCS 이름 충돌로 저작한 스플라인 포인트가 조용히 날아간다. C++ 컴포넌트명은 `LaneSpline`으로 하고 `BP_TDLane` 4개를 새로 배치한다. `BP_BalloonPath`는 삭제하지 말고 dead reference로 남긴다 (CLAUDE.md §3).

`ViveCodingUEServer.Target.cs`는 **추가하지 않는다** — 리슨 서버이므로 불필요.

---

## 빌드 / 설정 변경

### `Source/ViveCodingUE/ViveCodingUE.Build.cs`
```
Public:  Core, CoreUObject, Engine, InputCore, EnhancedInput, UMG
Private: Slate, SlateCore, OnlineSubsystem, OnlineSubsystemUtils
```
**`OnlineSubsystemSteam`은 모듈 의존성으로 넣지 않는다** — 링크하면 "ini 한 줄 전환" 요건이 깨진다. Steam을 아는 유일한 줄은 `DefaultEngine.ini`에만 존재해야 한다.

### `ViveCodingUE.uproject` — Plugins 배열에 추가
`OnlineSubsystem`, `OnlineSubsystemUtils`, `OnlineSubsystemNull`, `OnlineSubsystemSteam`, `SteamSockets`.

**저장소 규칙 "`.uproject` 커밋 금지"와 충돌한다.** 플러그인을 ini로 켜는 방법은 없어서 편집이 불가피. 절차: 편집 후 `git diff -- ue5/ViveCodingUE/ViveCodingUE.uproject`로 hunk가 Plugins 배열 하나뿐이고 `"EngineAssociation": "5.8"`이 그대로인지 확인. 에디터가 GUID로 바꿔놨으면 그 줄만 복구. 경로 지정으로만 stage, `git add .` 절대 금지.

### `Config/DefaultEngine.ini` — 추가
```ini
[/Script/Engine.GameSession]
MaxPlayers=4

[OnlineSubsystem]
DefaultPlatformService=Steam

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480
bUseSteamNetworking=false
bAllowP2PPacketRelay=true
P2PConnectionTimeout=90

[OnlineSubsystemNULL]
bEnabled=true

; 기존 [/Script/Engine.Engine] 섹션에 이어서
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SteamSockets.SteamSocketsNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="BeaconNetDriver",DriverClassName="/Script/OnlineSubsystemUtils.IpNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="DemoNetDriver",DriverClassName="/Script/Engine.DemoNetDriver",DriverClassNameFallback="/Script/Engine.DemoNetDriver")
```
`[/Script/SteamSockets.SteamSocketsNetDriver] NetConnectionClassName`은 넣지 않는다 — `BaseEngine.ini:2524-2525`에 이미 있음.

완전 Steam-free 로컬 실행은 `DefaultPlatformService=NULL` 또는 실행 인자 `-OnlineSubsystem=NULL`. Steam init 실패 로그가 뜨면 실행 파일 옆 `steam_appid.txt`에 `480` 확인.

---

## 페이즈 머신

데이터는 `ATDGameState`, 로직은 `ATDGameMode`.

```cpp
UENUM(BlueprintType)
enum class ETDPhase : uint8 { Build, Wave, Defeat, Victory };
```
`BeginPlay`에서 `Build`로 시작 → 웨이브 1도 방장 Start를 기다린다. 별도 PreGame 상태 불필요.

**웨이브 클리어 판정** — GameMode가 `int32 PendingSpawns`와 `TArray<TObjectPtr<ATDBalloon>> ActiveBalloons`를 보유. 풍선 퇴장은 **단 하나의 함수**로만 통과시킨다 (`EndPlay`/`OnDestroyed`는 레벨 teardown에도 발화하므로 쓰지 않음):
```cpp
void ATDGameMode::OnBalloonRemoved(ATDBalloon* B, bool bReachedCore, ATDPlayerState* Killer);
```
`ActiveBalloons`에서 제거 → `Killer` 있으면 `Killer->AddGold(Reward)` → `bReachedCore`면 코어 데미지 → `GS->BalloonsRemaining` 갱신(변경 시에만) → `Phase==Wave && PendingSpawns==0 && ActiveBalloons.Num()==0`이면 `FinishWave()`. 다음 행이 없으면 `Victory`, 있으면 `Build`.

`ActiveBalloons`는 타워 타겟팅 목록으로도 재사용된다.

**방장 판정** — `PostLogin`에서 `NewPlayer->IsLocalPlayerController()`. 리슨 서버에서 원격 클라의 PC는 절대 로컬이 아니므로 명확하다. `HostPlayerState` 캐시. 방장이 나가면 경고 로그 후 `HostPlayerState = nullptr`.

`ServerRequestStartWave` 거부 조건: `Phase != Build`, 또는 `Requester->PlayerState != HostPlayerState`. RPC가 `ATDPlayerController`에 있으므로 UE 소유권 규칙상 클라는 자기 컨트롤러로만 호출 가능 — 신원 위조는 구조적으로 불가. 모든 거부는 요청자 이름과 함께 `LogTD Warning`으로 남기며, **이 로그가 검증 산출물이다.**

---

## 레인 / 경로

```cpp
// ATDLane — bReplicates = false
UPROPERTY(EditAnywhere) int32 LaneIndex;        // 0=N 1=E 2=S 3=W
UPROPERTY(VisibleAnywhere) TObjectPtr<USplineComponent> LaneSpline;
UPROPERTY(EditAnywhere) float RoadWidth, RoadThickness;
UPROPERTY(EditAnywhere) TObjectPtr<UStaticMesh> RoadMesh;

float   GetLength() const;
FVector GetLocationAtDistance(float D) const;         // GetLocationAtDistanceAlongSpline(World)
float   GetDistanceToSpline(const FVector& W) const;  // FindLocationClosestToWorldLocation
```
`FTransform`이 아니라 `FVector`를 반환 — 풍선은 둥글어서 회전이 필요 없고, 이게 풍선당 매 프레임 호출이다. `OnConstruction`이 `BP_BalloonPath`의 construction script와 동일하게 `USplineMeshComponent` 도로를 재생성한다. 도로 세그먼트는 콜리전을 끈다 — 켜두면 타워 배치의 Occupied 검사에 걸린다.

**레인/코어 레지스트리는 `ATDGameState::BeginPlay`에서 서버·클라 양쪽이 로컬 수집.** 레벨 액터는 모든 머신에서 동일하므로 복제하지 않는다.
```cpp
for (TActorIterator<ATDLane> It(GetWorld()); It; ++It) { Lanes.Add(*It); }
Lanes.Sort([](const ATDLane& A, const ATDLane& B){ return A.LaneIndex < B.LaneIndex; });
```
풍선은 `uint8 LaneIndex`를 복제하고 `GS->Lanes[LaneIndex]`로 해석한다. 포인터 복제보다 바이트가 싸고 "startup actor 미해결" 순서 문제가 없다.

4레인은 순수 레벨 저작: `BP_TDLane` 4개, 각 스플라인이 맵 가장자리(±3800)에서 시작해 마지막 포인트가 원점 `(0,0,0)`. 풍선이 `GetLength()`에 도달하는 것이 곧 코어 도달.

---

## 풍선 이동 / 데미지

```cpp
// ATDBalloon::Tick — HasAuthority() 분기
DistanceAlongPath += Speed * DeltaSeconds;
if (DistanceAlongPath >= Lane->GetLength()) {
    GM->OnBalloonRemoved(this, /*bReachedCore=*/true, nullptr);
    Destroy(); return;
}
SetActorLocation(Lane->GetLocationAtDistance(DistanceAlongPath), /*bSweep=*/false);

// 시뮬레이티드 프록시: 로컬 적분 + 오차 흡수
LocalDistance += Speed * DeltaSeconds;
if (!FMath::IsNearlyZero(PendingError)) {
    const float Step = PendingError * FMath::Min(1.f, DeltaSeconds / 0.2f);
    LocalDistance += Step;
    PendingError -= Step;
}
SetActorLocation(Lane->GetLocationAtDistance(LocalDistance), false);

// OnRep_Distance: Error = DistanceAlongPath - LocalDistance.
//   |Error| > 50cm 면 즉시 스냅, 아니면 PendingError 에 쌓아 Tick 이 0.2초에 걸쳐 흡수.
//   속도 블렌딩 없음 — 자유도가 1인 이동에 예측기를 얹을 이유가 없다.
```

`float` 하나만 복제하고 `SetReplicateMovement(false)`를 **명시적으로** 호출. 풍선의 자유도는 1이고 나머지는 `LaneIndex` + 로컬 스플라인으로 재구성된다.

예산: 100마리 × 1Hz × ~6바이트 ≈ **600 B/s/클라**. `bReplicateMovement` 켜두면 기본 100Hz로 ~150 KB/s.

`bAlwaysRelevant = true` — 연결 4개, 맵 ±4000에서 거리 관련성 검사는 아끼는 대역폭보다 CPU를 더 쓴다. 맵이 커지면 제일 먼저 재검토할 지점.

코어 도달 시 `GS->CoreHealth -= WaveRow.CoreDamage`. 0 이하면 0으로 clamp, `Phase = Defeat`, 모든 타이머 정리, `ActiveBalloons` 전부 파괴. 콜리전/`TakeDamage`/오버랩 이벤트를 어디에도 쓰지 않는다 — 스칼라 비교 하나.

---

## 타워 배치

**200cm 그리드 스냅 + 자유 배치 + 기하 배제 검사.** 사전 저작 배치 그리드 자산 없음(레벨 디자인 의존성), 레인 배제에 물리 오버랩 미사용(도로가 `SplineMeshComponent`라 콜리전 튜닝이 성가심).

```cpp
// static — 클라 프리뷰와 서버 RPC가 완전히 동일한 코드를 돈다
static ETDPlaceResult ATDTower::CheckPlacement(UWorld*, const FVector& SnappedLoc, float TowerRadius);
```
1. 경계: `|X|,|Y| <= 4000` (기존 `Floor_0` 크기)
2. 레인 위 아님: 각 `ATDLane`에 대해 `GetDistanceToSpline`, `>= RoadWidth*0.5 + TowerRadius`
3. 코어 위 아님: `Dist2D >= Core->Radius + TowerRadius`
4. 칸 비어있음: `OverlapAnyTestByChannel(ECC_WorldStatic, MakeSphere(TowerRadius))` — 타워가 복제 액터라 양쪽에서 동일하게 동작, `OccupiedCells` 동기화 불필요

같은 함수를 공유하는 게 핵심이다. 초록 고스트인데 서버가 거부하는 건 최악의 버그인데, 이 구조에서는 골드/페이즈 레이스 말고는 발생이 불가능하다.

**클라 흐름** (`PlayerTick`, `bPlacementMode`일 때만): `DeprojectMousePositionToWorld` → `FMath::LinePlaneIntersection`(평면 해석 교차, 라인 트레이스 아님 — 바닥 콜리전 의존 없고 절대 빗나가지 않음) → `FMath::GridSnap(200.f)` → 로컬 고스트 액터(`bReplicates=false`) 이동 + `CheckPlacement` 결과로 초록/빨강 → `IA_PlaceTower`에서 `ServerPlaceTower`.

**서버 검증 순서** (`ATDGameMode::RequestPlaceTower`):
1. `_Validate`가 벡터 경계(`|X|,|Y|<=10000`, `|Z|<=1000`)와 null 클래스만 검사 — `_Validate` 실패는 연결을 끊으므로 구조적으로 불가능한 입력에만
2. `Phase == Build` 아니면 `WrongPhase` ← 배치 시점 규칙의 유일한 강제 지점
3. **`TowerClass`가 `AllowedTowerClasses`에 있어야 함.** 아니면 `IllegalClass`. **가장 중요한 검사** — `TSubclassOf`는 공격자 통제 값이라 allowlist 없으면 프로젝트 내 임의 `ATDTower` 자식을 스폰할 수 있다
4. **Cost/반경은 allowlist 클래스의 CDO에서 읽는다.** 클라가 준 값은 절대 안 씀
5. `PS->GetGold() >= Cost` 아니면 `NotEnoughGold`
6. **서버에서 위치를 다시 스냅** 후 `CheckPlacement` 결과 전파
7. 컨트롤러당 `0.1s` 레이트 리밋 → `TooFast`. 오버랩 질의를 스팸 루프로 서버에 못 박게 하는 보험

성공: 스폰(`Owner = PC`) → `Tower->SetOwningPlayerState(PS)` → `ForceNetUpdate()` → `PS->AddGold(-Cost)` → `ClientPlacementResult(Allowed)`. 모든 거부는 사유 + 플레이어명으로 `LogTD Warning`.

**타겟팅과 킬 귀속** — `ATDTower::Tick`(권위 전용)이 `GM->GetActiveBalloons()` 중 사거리 안에서 **`DistanceAlongPath`가 가장 큰**(= 코어에 가장 가까운) 풍선을 고른다. 고전적인 TD 우선순위이고, 거리값이 이미 있어서 추가 비용이 0이다. 풍선 100 × 타워 20 = 프레임당 ~2000 float 비교로 사실상 공짜이며 커스텀 콜리전 채널이 통째로 불필요하다. 쿨다운마다 `Balloon->ApplyDamage(Damage, OwningPlayerState)` → 사망 시 `OnBalloonRemoved(B, false, Killer)` → `Killer->AddGold(Reward)`. 라스트힛 = 추가 상태가 필요 없는 유일한 규칙.

---

## 카메라 / 입력

`ATDCameraPawn`: `USceneComponent` → `USpringArmComponent`(`TargetArmLength=4000`, 상대 피치 `-60`, `bDoCollisionTest=false`, `bInheritPitch/Yaw/Roll=false`) → `UCameraComponent`. −90°보다 −60°가 TD로 훨씬 잘 읽힌다. `bReplicates=true`(소유 클라 possession 복제에 필요) + `SetReplicateMovement(false)`(카메라는 순수 로컬, 배치 좌표는 RPC 인자로 오므로 서버가 의존하지 않음). 메시 없음. 로컬 클램프: `|X|,|Y| <= 4500`, arm 2000~8000.

**입력 자산이 현재 0개 — 6개 전부 신규.** `/Game/TD/Input/`:

| 자산 | 타입 | 바인딩 |
|---|---|---|
| `IMC_TD` | InputMappingContext | 아래 전부 |
| `IA_Pan` | Axis2D | WASD (Negate / SwizzleAxis 모디파이어) |
| `IA_Zoom` | Axis1D | 마우스 휠 |
| `IA_PlaceTower` | Digital | 좌클릭 |
| `IA_CancelPlacement` | Digital | 우클릭, Esc |
| `IA_ToggleBuild` | Digital | `1` |

`IA_Pan`/`IA_Zoom`은 **폰**의 `SetupPlayerInputComponent`, `IA_PlaceTower`/`IA_CancelPlacement`/`IA_ToggleBuild`는 **컨트롤러**의 `SetupInputComponent`(컨트롤러가 배치 상태와 고스트를 소유하므로).

```cpp
// ATDPlayerController::BeginPlay, IsLocalController() 가드
bShowMouseCursor = true;
SetInputMode(FInputModeGameAndUI()
    .SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock)
    .SetHideCursorDuringCapture(false));
GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()->AddMappingContext(TDMappingContext, 0);
```
**`UIOnly`가 아니라 `GameAndUI`** — `BP_GM`이 `SetInputMode_UIOnlyEx`로 게임 입력을 죽인 게 바로 TDMap이 `BP_GM`을 우회하는 이유다. `GameAndUI`면 HUD가 떠 있어도 WASD 팬이 살아있다.

TDMap의 기존 `TopDownCamera`(`AutoActivateForPlayer=Player0`)는 폰 카메라와 뷰 타깃을 놓고 다투므로 **삭제해야 한다.**

---

## UI

`AuthClient` 패턴 그대로: C++ `UUserWidget` 베이스 + `meta=(BindWidget)` 멤버 + 에디터 저작 WBP 자식. `UAuthLoginWidget.h`처럼 필요한 위젯 이름을 클래스 주석에 명시 — 그 주석이 사용자가 보고 만드는 계약서다.

| C++ 베이스 | WBP | `BindWidget` 멤버 |
|---|---|---|
| `UTDHUDWidget` | `WBP_TDHUD` | `CoreHealthBar`(ProgressBar), `CoreHealthText`, `WaveText`, `GoldText`, `PhaseText`, `BalloonsText`, `StartWaveButton` |
| `UTDGameOverWidget` | `WBP_TDGameOver` | `ResultText`, `WaveReachedText`, `ReturnButton` — 승/패 양쪽 겸용 |
| `UTDSessionMenuWidget` | `WBP_TDSessionMenu` | `HostButton`, `RefreshButton`, `SessionList`(ScrollBox), `StatusText`, `LanCheckBox` |
| `UTDSessionRowWidget` | `WBP_TDSessionRow` | `NameText`, `PlayersText`, `JoinButton` |

`UListView`가 아니라 `UScrollBox` + row 위젯 — `IUserObjectListEntry` + UObject 아이템 소스는 항목 3개짜리 목록에 과한 배관이다.

`UTDHUDWidget::NativeConstruct`가 `OnPhaseChanged` / `OnCoreHealthChanged` / `OnGoldChanged`를 바인드. 카운트다운이 없으므로 `NativeTick`은 **`StartWaveButton` 가시성 재평가**(`PS->IsHost() && Phase == Build`)만 한다 — 로컬 `PlayerState`가 위젯 생성 후에 복제되어 들어오는 실제 레이스를 여기서 흡수한다. Build 페이즈에 방장 아닌 플레이어는 `PhaseText`에 "방장이 시작하기를 기다리는 중".

**세션 메뉴 위치**: 신규 빈 맵 `/Game/Maps/TDLobby`(기본 `AGameModeBase`). Host → `CreateSession` → `ServerTravel("/Game/Maps/TDMap?listen")`. Join → `ClientTravel(ResolvedConnectString, TRAVEL_Absolute)`. 호스팅에 레벨 리로드가 필요하므로 로비 맵이 자연스럽고, `MainMap` 재사용은 분리하기로 한 로그인 흐름과 얽힌다.

`UTDSessionSubsystem`은 `UAuthSubsystem` 형태를 그대로 따른다 (`UGameInstanceSubsystem` + `DECLARE_DYNAMIC_MULTICAST_DELEGATE` 결과 통지). 세션 설정: `NumPublicConnections=4`, `bShouldAdvertise`, `bUsesPresence`, `bAllowJoinViaPresence`, `bUseLobbiesIfAvailable=true`, 커스텀 키 `Set(FName("VCTD"), FString("1"), ViaOnlineService)`. 검색: `MaxSearchResults=200`, `SEARCH_PRESENCE` + **`VCTD` 키 필터**.

`VCTD` 키는 선택이 아니다. AppId 480은 Spacewar라 전 세계 UE/Unity 개발자가 공유한다 — 필터 없으면 무관한 로비 수백 개가 딸려온다. Steam 헤더는 어디에도 include하지 않는다.

---

## 검증 전략

**`TD.DumpState` 콘솔 명령이 이 프로젝트 전체의 멀티플레이 검증 도구다.** `FAutoConsoleCommandWithWorld`로, 실행된 머신 기준: 넷 모드, 페이즈, 웨이브, 코어 HP, 풍선 수, 각 PlayerState의 이름/골드/방장 플래그, 타워 수, `GetWorld()->GetNetDriver()->InBytes/OutBytes/InPackets/OutPackets`를 로그한다. 두 PIE 창에서 실행해 로그 블록을 diff한다. ~25줄이고, 이후 모든 검증이 스크린샷 대신 텍스트 diff가 된다.

### 2인 이상 테스트 3단계 — Steam이 필요한 건 3단계뿐

`IOnlineSession` 인터페이스로만 작성하는 이유가 여기 있다.

1. **PIE 멀티 창.** Editor Preferences → Play → 2~4 players, Net Mode = *Play As Listen Server*. SteamSockets는 에디터에서 init 자체가 안 되므로 설정 변경 없이 항상 `IpNetDriver`. **모든 replication / RPC / 페이즈 / 골드 / 배치를 여기서 커버한다**
2. **한 PC에 standalone 2개.** PIE가 감추는 travel/relevancy 버그용. `DefaultPlatformService=NULL` 후:
   ```
   UnrealEditor.exe <uproject> /Game/Maps/TDMap -game -listen -log -windowed -resx=1280 -resy=720
   UnrealEditor.exe <uproject> /Game/Maps/TDMap -game -log -windowed -resx=1280 -resy=720
   ```
   두 번째 창 콘솔에 `open 127.0.0.1`. PIE 단일 프로세스가 흉내만 내는 실제 `ServerTravel`/`ClientTravel`/`PostLogin` 경로다
3. **PC 2대 + Steam 계정 2개 + 패키징 빌드.** Steam 세션 생성/검색/조인과 SteamSockets 전송을 검증하는 유일한 방법. 한 PC에는 Steam 계정 하나만 로그인되므로 같은 PC PIE로는 불가능하다. 앞 단계가 전부 초록이어야 여기 오므로 실패는 명백히 Steam/세션 문제다

---

## 리스크

**높음**
1. **5.8에서 `SteamNetDriver` 제거됨.** UE4~5.3 시대 튜토리얼 ini를 복사하면 존재하지 않는 클래스를 가리키고, 실패 양상이 "조용히 평문 IP로 잘 동작"이라 인터넷 너머 테스트 전까지 성공처럼 보인다. 최종 ini에 `OnlineSubsystemSteam.SteamNetDriver` 문자열이 없는지 grep으로 확인
2. **`.uproject`를 편집해야 하지만 부주의하게 커밋하면 안 됨.** 플러그인을 ini로 켤 방법이 없어 불가피. hunk 단위 stage 절차 준수
3. **AppId 480 로비 오염.** `VCTD` 커스텀 검색 키 없으면 무관한 Spacewar 로비가 쏟아지고 조인이 무작위로 깨진 것처럼 보인다. 480 로비는 전역 공개라 아무나 개발 세션에 들어올 수도 있음
4. **`NetUpdateFrequency`는 경고가 아니라 컴파일 에러.** `SetNetUpdateFrequency()` 사용

**중간**

5. **`bReplicateMovement` 기본 true.** 안 끄면 설계 대비 ~200배 대역폭인데 게임은 정상 동작해서 버그로 안 보인다 — 검증에서 실제 `OutBytes` 델타를 기록하는 이유
6. **`TDLobby` → `TDMap?listen` `ServerTravel`.** GameInstance 서브시스템은 travel을 넘어 살아남지만(세션 서브시스템 유지에 유리) 모든 액터는 재생성된다. 세션 상태는 반드시 `UTDSessionSubsystem`에만 두고 액터에 두지 말 것
7. 풍선 HP바 없음(사망 = 파괴)으로 설계함. 나중에 HP바를 넣으려면 `Health`를 풍선마다 복제해야 하고 풍선당 대역폭이 대략 2배 — 여전히 괜찮지만 표류가 아니라 결정으로 처리할 것
8. 방장이 나가면 아무도 웨이브를 시작할 수 없다. 현재는 경고 로그만 남긴다. 방장 이양은 범위 밖 — 필요하면 별도 결정
