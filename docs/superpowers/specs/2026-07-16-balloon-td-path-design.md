# 풍선 타워 디펜스 — 경로 및 풍선 이동 설계

작성일: 2026-07-16

## 목표

풍선 타워 디펜스의 첫 조각을 만든다. 풍선이 나오는 **시작지점**, 사라지는 **끝지점**, 둘을 잇는 **꼬불꼬불한 길**을 만들고, 풍선이 실제로 그 길을 따라 시작점에서 끝점까지 이동하는 것까지 확인한다.

타워, 웨이브, 라이프, 사격은 이번 범위가 아니다.

## 성공 기준

1. `TDMap`에서 PIE를 누르면 로그인 화면 없이 탑다운 시점으로 맵 전체가 보인다.
2. 바닥에 꼬불꼬불한 도로가 눈으로 보인다.
3. 풍선이 1초 간격으로 시작점에 나타나 도로를 따라 이동한다.
4. 풍선이 끝점에 닿으면 사라진다. 풍선 개수가 무한히 늘지 않는다.
5. 블루프린트 3개가 에러 없이 컴파일되고, PIE 로그에 경고/에러가 없다.

## 배경 / 제약

- 엔진은 UE 5.8 설치본(`C:\Program Files\Epic Games\UE_5.8`). 프로젝트는 `ue5/ViveCodingUE`.
- 기존 코드는 전부 로그인/채팅(`Source/AuthClient`)이며 TD와 겹치지 않는다.
- `MainMap`이 프로젝트 기본 맵이고, 게임모드 `BP_GM`의 BeginPlay가 다음을 한다:

  ```
  EventBeginPlay → CreateWidget(WBP_Login) → AddToViewport
                 → SetShowMouseCursor(true) → SetInputModeUIOnly
  ```

  `SetInputModeUIOnly` 때문에 이 게임모드를 쓰면 로그인 화면이 뜰 뿐 아니라 게임 입력이 막힌다. **TD 맵은 게임모드 오버라이드가 필수다.**
- `BP_GM`의 부모 `ABaseGM`(`Source/ViveCodingUE/BaseGM.h`)은 내용이 완전히 빈 `AGameModeBase`다. TD 맵은 이걸 직접 지정하면 새 코드 없이 깨끗한 게임모드를 얻는다.
- 기존 `MainMap`의 바닥은 80m × 80m(±4000cm), 윗면 z ≈ 0. TD 맵도 같은 규모로 간다.

## 결정 사항

| 항목 | 결정 | 이유 |
|---|---|---|
| 맵 | 새 맵 `/Game/Maps/TDMap` | 로그인 플로우와 완전 분리 |
| 게임모드 | `ABaseGM` 오버라이드 | 빈 게임모드, 새 코드 불필요 |
| 경로 표현 | `USplineComponent` | 부드러운 곡선, 에디터 드래그 수정, 경로상 거리 값을 공짜로 얻음 |
| 도로 외형 | `USplineMeshComponent` + 엔진 기본 Cube | 새 에셋 불필요 |
| 구현 | 블루프린트 | 에디터에서 직접 보고 수정 |
| 시점 | 고정 탑다운 `CameraActor` | TD 기본 시점, 검증이 쉬움 |
| 풍선 스폰 | 1초 간격 루핑 타이머 | 경로가 제대로 도는지 계속 보임 |

### 경로 표현을 스플라인으로 정한 이유

웨이포인트 + 직선 보간도 가능하지만 모서리가 각져서 "꼬불꼬불"이 안 나오고, 도로 코너를 손으로 메워야 한다. 결정적으로 스플라인은 **경로상 거리(distance along spline)** 를 공짜로 준다. 이 값 하나로 "어느 풍선이 더 앞섰나"(타워 타겟 우선순위)와 "끝점까지 얼마 남았나"가 풀린다. 웨이포인트 방식이면 나중에 직접 만들어야 한다.

## 구조

에셋 3개 + 레벨 액터들:

```
BP_BalloonSpawner ──(타이머로 스폰 + Path 주입)──▶ BP_Balloon
        │                                              │
        └──────────────▶ BP_BalloonPath ◀──────────────┘
                          (거리 → 트랜스폼 질의)
```

스포너와 풍선은 서로를 모른다. 둘 다 길만 안다. 길은 아무도 모르고 질문에 답만 한다. 덕분에 길 모양을 바꿔도 나머지는 손댈 필요가 없다.

### `BP_BalloonPath` (부모: `Actor`)

길 그 자체. 시작점과 끝점을 별도 액터로 두지 않고 **스플라인의 첫 점이 시작지점, 마지막 점이 끝지점**이다.

- **컴포넌트**
  - `Spline` (`SplineComponent`) — 루트. 점들이 꼬불꼬불한 곡선을 정의한다.
  - 컨스트럭션 스크립트가 생성하는 `SplineMeshComponent` 다수 — 도로 외형.
- **노출 변수 (인스턴스 편집 가능)**
  - `RoadWidth` (float, 기본 300) — 도로 폭 cm
  - `RoadThickness` (float, 기본 20) — 도로 두께 cm
  - `RoadMaterial` (`MaterialInterface`, 기본 미지정 → 엔진 기본 머티리얼)
- **컨스트럭션 스크립트**
  - 기존 `SplineMeshComponent`를 전부 제거하고 다시 만든다(중복 누적 방지).
  - 스플라인 점이 2개 미만이면 아무것도 안 하고 끝낸다.
  - 각 구간 `i`(0 ~ `NumberOfSplinePoints - 2`)마다 `SplineMeshComponent`를 하나 추가하고:
    - 메시 = `/Engine/BasicShapes/Cube`
    - `SetStartAndEnd(구간 시작 위치·탄젠트, 구간 끝 위치·탄젠트)` — 위치/탄젠트는 `GetLocationAtSplinePoint` / `GetTangentAtSplinePoint`를 **Local** 공간으로 받는다.
    - `ForwardAxis = X`
    - `SetStartScale` / `SetEndScale` = `(RoadWidth / 100, RoadThickness / 100)` — 기본 Cube가 100cm 정육면체라 100으로 나눠 스케일을 낸다.
    - 콜리전 없음 (`NoCollision`) — 이번 범위에선 충돌이 필요 없다.

### `BP_Balloon` (부모: `Actor`)

- **컴포넌트**
  - `Mesh` (`StaticMeshComponent`) — `/Engine/BasicShapes/Sphere`, 스케일 0.6 (지름 약 60cm). 콜리전 없음.
- **변수**
  - `Path` (`BP_BalloonPath` 오브젝트 레퍼런스) — 스포너가 주입
  - `Speed` (float, 기본 400) — cm/s
  - `DistanceAlongPath` (float, 기본 0)
- **BeginPlay**
  - `Path`가 유효하지 않으면 경고 로그를 남기고 즉시 `DestroyActor`.
- **Tick**
  - `DistanceAlongPath += Speed * DeltaSeconds`
  - `DistanceAlongPath >= Path.Spline.GetSplineLength()` 이면 `DestroyActor` 후 종료.
  - 아니면 `Path.Spline.GetTransformAtDistanceAlongSpline(DistanceAlongPath, World)`를 받아
    **`SetActorLocationAndRotation`** 으로 위치·회전만 적용한다.
    - `SetActorTransform`을 쓰면 안 된다. 스플라인 트랜스폼에는 스케일이 들어 있어 풍선의 0.6 스케일을 덮어쓴다.

### `BP_BalloonSpawner` (부모: `Actor`)

- **변수 (인스턴스 편집 가능)**
  - `Path` (`BP_BalloonPath` 오브젝트 레퍼런스) — 레벨에서 지정
  - `BalloonClass` (`BP_Balloon` 클래스, 기본 `BP_Balloon`)
  - `SpawnInterval` (float, 기본 1.0) — 초
- **BeginPlay**
  - `Path`가 유효하지 않으면 **에러 로그를 남기고 타이머를 걸지 않는다.** 조용히 아무 일도 안 일어나는 것보다 낫다.
  - 유효하면 `SpawnInterval` 간격 루핑 타이머로 `SpawnBalloon` 호출.
- **`SpawnBalloon` (커스텀 이벤트)**
  - 스폰 위치 = `Path.Spline.GetLocationAtDistanceAlongSpline(0, World)`
  - `SpawnActor(BalloonClass)` → 스폰된 풍선의 `Path`에 자신의 `Path`를 주입.
  - 스폰 콜리전 핸들링은 `AlwaysSpawn` — 겹쳐도 스폰돼야 한다.

### 레벨 구성 (`/Game/Maps/TDMap`)

| 액터 | 설정 |
|---|---|
| 바닥 | `StaticMeshActor`, 메시 `/Engine/BasicShapes/Cube`, 스케일 (80, 80, 1), 위치 (0, 0, −50) → 8000 × 8000 × 100cm, 윗면 z = 0 |
| `DirectionalLight` | 기본 각도 |
| `SkyLight` + `SkyAtmosphere` | 최소 조명 |
| `BP_BalloonPath` | 원점 배치. 스플라인 점은 아래 "길 모양" 참조 |
| `BP_BalloonSpawner` | 아무데나. `Path` = 위 경로 액터 |
| `CameraActor` | 위치 (0, 0, 6000), 회전 pitch −90. **`Auto Activate for Player = Player 0`** |
| World Settings | `GameMode Override = ABaseGM` |

카메라는 `Auto Activate for Player`로 잡는다. 코드 없이 PIE 시작과 동시에 시점이 잡힌다.

### 길 모양

바닥 ±4000 안에서 뱀처럼 감는 9개 점. 모두 z = 20 (바닥에서 살짝 띄움).

| # | X | Y | 비고 |
|---|---|---|---|
| 0 | −3500 | −3000 | **시작지점** |
| 1 | −1000 | −3000 | |
| 2 | 1500 | −2000 | |
| 3 | 2500 | 0 | |
| 4 | 1000 | 1500 | |
| 5 | −1500 | 1000 | |
| 6 | −2500 | 2500 | |
| 7 | 0 | 3200 | |
| 8 | 3500 | 3000 | **끝지점** |

스플라인 자동 탄젠트가 부드러운 곡선으로 이어준다. 점 위치는 에디터에서 드래그로 수정하면 컨스트럭션 스크립트가 도로를 즉시 다시 깐다.

## 예외 처리

| 상황 | 동작 |
|---|---|
| 풍선에 `Path` 레퍼런스 없음 | 경고 로그 + 즉시 소멸 |
| 스포너에 `Path` 레퍼런스 없음 | 에러 로그 + 타이머 미설정 |
| 스플라인 점 2개 미만 | 도로 생성 건너뜀 |

## 검증

뷰포트 캡처는 쓰지 않는다.

1. **컴파일** — `BP_BalloonPath`, `BP_Balloon`, `BP_BalloonSpawner`를 각각 `compile_blueprint`로 컴파일. 에러 0.
2. **PIE 기동** — `StartPIE` 후 `LogsToolset`으로 로그 확인. 경고/에러 0.
3. **이동 확인** — `find_actors`로 풍선을 찾아 **1초 간격으로 `get_actor_transform`을 두 번 읽어 위치가 실제로 변했는지** 확인. 이동 방향이 경로를 따르는지 좌표로 검증.
4. **소멸 확인** — 몇 초 뒤 풍선 개수를 다시 세어 무한히 늘지 않고 안정되는지 확인. 끝점 소멸이 도는지에 대한 증거.
5. **PIE 종료** — `StopPIE`.

## 범위 밖 (다음 단계)

- 타워 배치 및 사격
- 웨이브 / 라운드
- 라이프 (끝점 도달 시 감소)
- 풍선 체력·종류·속도 등급
- 길 위 타워 배치 금지 규칙

## 알려진 이슈 (이 작업과 무관, 별도 처리 필요)

- `ViveCodingUE.uproject`의 `EngineAssociation`이 `"5.8"` → GUID(`{28CFFBFB-...}`)로 에디터에 의해 덮어써져 있다. 커밋 `fe62254`에서 한 번 되돌린 항목. 커밋 전 원복 필요.
- `AuthSubsystem`의 기본 서버 주소가 `http://127.0.0.1:8000`인데, UE MCP 서버도 같은 포트 8000을 쓴다. MCP가 떠 있는 동안 FastAPI 인증 서버를 8000에 띄울 수 없다.
