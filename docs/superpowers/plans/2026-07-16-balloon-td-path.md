# 풍선 TD 경로 및 풍선 이동 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 풍선이 시작지점에서 꼬불꼬불한 도로를 따라 끝지점까지 이동하다 사라지는 것을 `TDMap`에서 확인한다.

**Architecture:** 블루프린트 3개. `BP_BalloonPath`가 스플라인으로 길을 정의하고 컨스트럭션 스크립트가 스플라인 메시로 도로를 깐다. `BP_BalloonSpawner`가 타이머로 `BP_Balloon`을 스폰하며 길 참조를 주입한다. 풍선은 매 틱 길에게 "이 거리에서 내 트랜스폼은?"만 묻는다. 스포너와 풍선은 서로를 모른다.

**Tech Stack:** UE 5.8 블루프린트, `USplineComponent` / `USplineMeshComponent`, unreal-mcp(EditorToolset) `BlueprintTools.write_graph_dsl`.

**설계 스펙:** `docs/superpowers/specs/2026-07-16-balloon-td-path-design.md`

## Global Constraints

- 엔진 `C:\Program Files\Epic Games\UE_5.8`, 프로젝트 `C:\Work\VibeCoding\ue5\ViveCodingUE`. 레포 루트는 `C:\Work\VibeCoding`.
- 브랜치 `feature/balloon-td-path`. `.uasset`/`.umap`은 git 추적 대상이다.
- **`ViveCodingUE.uproject`는 절대 커밋하지 말 것.** `EngineAssociation`이 GUID로 덮어써진 미해결 수정본이 작업 트리에 있다. 이 작업과 무관하다.
- 블루프린트는 전부 `/Game/TD/` (= `Content/TD/`) 아래에 만든다.
- 좌표 단위는 cm. 바닥은 ±4000, 윗면 z = 0.
- **뷰포트 캡처로 검증하지 말 것.** 데이터로만 검증한다 (`find_actors`, `get_actor_transform`, `compile_blueprint`, `LogsToolset`).
- 맵 생성 같은 에디터 단순 작업은 사용자에게 부탁한다. `TDMap`은 이미 사용자가 만들었다.

## MCP 사용 필수 지식 (검증 완료 — 추측하지 말 것)

이 절의 내용은 임시 블루프린트로 **실제 확인한 사실**이다. 여기 없는 노드 ID는 반드시 `find_node_types`로 찾을 것.

**최대 함정 — 이름 충돌 시 조용히 엉뚱한 노드가 잡힌다.**
스플라인 컴포넌트를 `Spline`로 이름 지으면 `(Variables|Default|GetSpline)`이 엔진의 `Class|PCGLandscapeSplineData|GetSpline`으로 해석된다. **에러도 경고도 없고 `warnings_as_errors=true`로도 컴파일이 통과한다.** 그래서 이 계획은 컴포넌트를 `PathSpline`, `BalloonMesh`처럼 고유하게 명명한다.
→ **`write_graph_dsl` 후 반드시 `read_graph_dsl`로 읽어 노드가 의도한 것인지 확인한다. 컴파일 성공은 정확성의 증거가 아니다.**

**컨스트럭션 스크립트 진입점은 `ConstructionScript`.** 그래프 이름은 `UserConstructionScript`지만 DSL은 `(fn ConstructionScript () ...)`. `(fn UserConstructionScript ...)`는 `AddEvent|UserConstructionScript does not exist`로 실패한다.

**검증된 노드 ID / 핀:**

| 노드 ID | 핀 |
|---|---|
| `Spline|GetNumberOfSplinePoints` | `self` → Integer |
| `Spline|GetSplineLength` | `self` → Float |
| `Spline|GetLocationAtSplinePoint` | `self`, `PointIndex`(int), `CoordinateSpace`(기본 `"Local"`) → Vector |
| `Spline|GetTangentAtSplinePoint` | `self`, `PointIndex`, `CoordinateSpace` → Vector |
| `Spline|GetTransformAtDistanceAlongSpline` | `self`, `Distance`(float), `CoordinateSpace`(기본 `"Local"`), `bUseScale`(기본 false) → Transform |
| `AddComponent|Rendering|AddSplineMeshComponent` | `self`(Actor), `bManualAttachment`(기본 false), `RelativeTransform` → SplineMeshComponent |
| `SplineMesh|SetStartAndEnd` | `self`, `StartPos`, `StartTangent`, `EndPos`, `EndTangent`, `bUpdateMesh`(기본 true) |
| `Components|StaticMesh|SetStaticMesh` | `self`, `NewMesh` |
| `Transformation|SetActorLocationAndRotation` | (Task 3 Step 1에서 핀 확인) |
| `Utilities|Time|SetTimerbyFunctionName` | `Object`, `FunctionName`(String), `Time`(float), `bLooping`(기본 false), `bMaxOncePerFrame`, `InitialStartDelay`, `InitialStartDelayVariance` → TimerHandle |
| `Utilities|String|ToString(Float)` | Float → String. **`Math|Float|ToString`은 존재하지 않는다.** |
| `Development|PrintString` | `InString`, `Duration` … |

**로그 읽기:** `EditorToolset.LogsToolset.GetLogEntries(category, pattern, maxEntries)`.
`pattern`은 **빈 문자열이라도 필수 인자**다. `category`의 기본값이 `"LogsToolset"`이라 **생략하면 엉뚱한 카테고리만 나온다** — 전체 로그를 보려면 반드시 `category=""`를 명시할 것.
예: `GetLogEntries(category="", pattern="Error|Warning", maxEntries=100)`

**DSL 규칙:**
- 클래스 경로·enum·에셋 참조는 **반드시 따옴표**. 안 하면 "Undefined variable". 예: `"/Engine/BasicShapes/Cube.Cube"`, `"AlwaysSpawn"`, `"Local"`, `"World"`.
- `self`는 자동 바인딩된다.
- 노드 출력을 두 곳 이상에서 쓰면 반드시 `(bind ...)`. 반복 호출하면 노드가 중복 생성된다.
- 멀티exec 노드는 `(:PinName stmts…)` 형태의 연속부를 쓴다. 공백 있는 핀은 `(:"Is Valid" …)`.
- `find_node_types`는 `context_pins`가 빈 배열이라도 **필수 인자**다.
- 노드 ID는 대소문자 무시. `find_node_types`는 `Spline|GetTransformatDistanceAlongSpline`(소문자 at)로 돌려주지만 `Spline|GetTransformAtDistanceAlongSpline`도 동작한다.

**MCP 툴 시그니처:**
- `BlueprintTools.create(folder_path, asset_name, asset_type={"refPath":"/Script/Engine.Actor"})`
- `ActorTools.add_component(owner={"refPath":"<BP경로>"}, component_type={"refPath":"/Script/Engine.SplineComponent"}, name="PathSpline")` — 블루프린트에도 동작한다.
- `BlueprintTools.add_variable(blueprint, name, type_name)` / `add_object_variable(blueprint, name, object_class)`
- `BlueprintTools.set_variable_instance_editable(...)` — 레벨에서 편집 가능하게
- `ObjectTools.set_properties(obj, json)` / `list_properties(obj)`
- `SceneTools.add_to_scene_from_asset(asset_path, name, xform, parent, snap_to_ground)`
- `SceneTools.add_to_scene_from_class(actor_type, name, xform, parent, snap_to_ground)`
- `SceneTools.find_actors(name, tag, collision_channels, ...)` — `name`/`tag`/`collision_channels` 필수
- `ActorTools.get_actor_transform(actor)`
- `EditorAppToolset.StartPIE(Options)` / `StopPIE` / `IsPIERunning`
- `AssetTools.exists(path)` / `delete(path)` / `save_assets(paths)`

## File Structure

| 경로 | 책임 |
|---|---|
| `/Game/TD/BP_BalloonPath` | 길. 스플라인으로 모양을 정의하고 도로를 그린다. 거리→트랜스폼 질의에 답한다. |
| `/Game/TD/BP_Balloon` | 풍선 하나. 길을 따라 이동하고 끝에서 소멸한다. |
| `/Game/TD/BP_BalloonSpawner` | 타이머로 풍선을 만들고 길 참조를 주입한다. |
| `/Game/Maps/TDMap` | 레벨. 게임모드 오버라이드, 카메라, 위 액터 배치. (이미 존재 — 사용자 생성) |

### 스펙 대비 변경점

1. **바닥 생성 불필요.** 스펙은 Cube 스케일 (80,80,1)로 바닥을 만들라 했으나, 사용자가 만든 `TDMap`에 이미 `Floor_0`가 있고 실측 결과 바운드가 정확히 ±4000, 윗면 z = −0.5다. 스펙 의도를 이미 충족한다.
2. **컨스트럭션 스크립트에서 기존 컴포넌트 수동 제거 불필요.** 스펙은 "기존 SplineMeshComponent를 전부 제거하고 다시 만든다(중복 누적 방지)"라 했으나, UE는 컨스트럭션 스크립트가 `AddComponent` 노드로 만든 컴포넌트를 재실행 시 자동으로 파괴·재생성한다(`CreationMethod == UserConstructionScript`). 수동 제거는 불필요하다.
3. **풍선이 길의 스플라인을 직접 만지지 않는다.** 스펙은 풍선이 `Path.Spline.GetTransformAtDistanceAlongSpline(...)`을 호출한다고 썼으나, 다른 블루프린트의 컴포넌트 변수에 접근하는 DSL 형태가 불확실하다. 대신 `BP_BalloonPath`에 함수 두 개(`GetPathLength`, `GetTransformAtDistance`)를 만들어 풍선이 그것만 호출한다. 스펙의 "길은 질문에 답만 한다"는 의도에 오히려 더 맞고, 결합도가 낮아진다.

---

### Task 1: TDMap 게임모드 오버라이드 + 탑다운 카메라

**목표:** PIE를 누르면 로그인 화면 없이 탑다운으로 빈 바닥이 보인다.

**Files:**
- Modify: `/Game/Maps/TDMap` (World Settings, CameraActor 추가)

**Interfaces:**
- Consumes: 없음
- Produces: 없음 (레벨 상태만)

**배경:** `BP_GM`(프로젝트 기본 게임모드)의 BeginPlay가 `CreateWidget(WBP_Login)` → `AddToViewport` → `SetShowMouseCursor(true)` → `SetInputModeUIOnly`를 호출한다. 오버라이드하지 않으면 TD 맵에서 로그인 화면이 뜨고 게임 입력이 막힌다. 부모 `ABaseGM`(`/Script/ViveCodingUE.BaseGM`)은 내용이 완전히 빈 `AGameModeBase`라 그대로 쓰면 된다.

- [ ] **Step 1: TDMap이 열려 있는지 확인**

```
SceneTools.get_current_level()
```
Expected: `"/Game/Maps/TDMap"`. 아니면 `SceneTools.load_level("/Game/Maps/TDMap")`.

- [ ] **Step 2: World Settings의 게임모드 오버라이드 확인 (변경 전)**

```
SceneTools.find_actors(name="WorldSettings", tag="", collision_channels=[])
```
반환된 WorldSettings refPath로:
```
ObjectTools.get_properties(<WorldSettings>, ["DefaultGameMode"])
```
Expected: 미지정(None) 또는 프로젝트 기본. 정확한 프로퍼티명이 다르면 `ObjectTools.list_properties(<WorldSettings>)`로 `GameMode` 포함 이름을 찾는다.

- [ ] **Step 3: 게임모드 오버라이드를 ABaseGM으로 설정**

```
ObjectTools.set_properties(<WorldSettings>, "{\"DefaultGameMode\": \"/Script/ViveCodingUE.BaseGM\"}")
```
(Step 2에서 확인한 실제 프로퍼티명을 쓸 것.)

- [ ] **Step 4: 탑다운 카메라 배치**

```
SceneTools.add_to_scene_from_class(
  actor_type={"refPath": "/Script/Engine.CameraActor"},
  name="TopDownCamera",
  xform={"location": {"x": 0, "y": 0, "z": 6000},
         "rotation": {"pitch": -90, "yaw": 0, "roll": 0},
         "scale": {"x": 1, "y": 1, "z": 1}})
```

- [ ] **Step 5: 카메라를 Player 0에 자동 활성화**

```
ObjectTools.list_properties(<TopDownCamera>)
```
`AutoActivateForPlayer` 를 찾아:
```
ObjectTools.set_properties(<TopDownCamera>, "{\"AutoActivateForPlayer\": \"Player0\"}")
```
Expected: true 반환. enum 값 문자열이 다르면 `list_properties` 출력의 실제 enum 표기를 쓴다.

- [ ] **Step 6: PIE로 검증**

```
EditorAppToolset.StartPIE({"bSimulate": false, "PlayMode": "PlayMode_InViewPort", "WarmupSeconds": 2.0})
```
그다음:
```
SceneTools.find_actors(name="TopDownCamera", tag="", collision_channels=[])
LogsToolset.<로그 읽기 툴>  ; 툴명은 describe_toolset으로 확인
```
Expected: 로그에 `WBP_Login` 관련 라인이 **없다**. 에러/경고 없음.
```
EditorAppToolset.StopPIE()
```

- [ ] **Step 7: 레벨 저장 후 커밋**

```
AssetTools.save_assets(["/Game/Maps/TDMap"])
```
```bash
cd C:/Work/VibeCoding
git add ue5/ViveCodingUE/Content/Maps/TDMap.umap
git commit -m "Add TD map game mode override and top-down camera"
```
**주의:** `git add .` 금지 — `.uproject`가 딸려간다.

---

### Task 2: BP_BalloonPath — 스플라인 길과 도로

**목표:** TDMap 바닥에 꼬불꼬불한 도로가 눈에 보인다.

**Files:**
- Create: `/Game/TD/BP_BalloonPath`
- Modify: `/Game/Maps/TDMap` (경로 액터 배치)

**Interfaces:**
- Consumes: 없음
- Produces:
  - 컴포넌트 `PathSpline` (`USplineComponent`)
  - 함수 `GetPathLength()` → Float — 스플라인 전체 길이(cm)
  - 함수 `GetTransformAtDistance(Distance: Float)` → Transform — 월드 공간, 스케일 미포함
  - 변수 `RoadWidth` (Float, 기본 300), `RoadThickness` (Float, 기본 20)

- [ ] **Step 1: 블루프린트 생성**

```
BlueprintTools.create(folder_path="/Game/TD", asset_name="BP_BalloonPath",
                      asset_type={"refPath": "/Script/Engine.Actor"})
```
Expected: `{"refPath": "/Game/TD/BP_BalloonPath.BP_BalloonPath"}`

- [ ] **Step 2: 스플라인 컴포넌트 추가**

```
ActorTools.add_component(
  owner={"refPath": "/Game/TD/BP_BalloonPath.BP_BalloonPath"},
  component_type={"refPath": "/Script/Engine.SplineComponent"},
  name="PathSpline")
```
**이름은 반드시 `PathSpline`.** `Spline`로 지으면 DSL이 `Class|PCGLandscapeSplineData|GetSpline`을 조용히 물어온다.

- [ ] **Step 3: 도로 변수 추가**

```
BlueprintTools.add_variable(blueprint={"refPath":"/Game/TD/BP_BalloonPath.BP_BalloonPath"},
                            name="RoadWidth", type_name="float")
BlueprintTools.add_variable(blueprint={"refPath":"/Game/TD/BP_BalloonPath.BP_BalloonPath"},
                            name="RoadThickness", type_name="float")
```
기본값 설정:
```
ObjectTools.set_properties(<BP_BalloonPath CDO>, "{\"RoadWidth\": 300.0, \"RoadThickness\": 20.0}")
```
CDO는 `BlueprintTools.get_default_object(blueprint)`로 얻는다.
인스턴스 편집 허용:
```
BlueprintTools.set_variable_instance_editable(<blueprint>, "RoadWidth", true)
BlueprintTools.set_variable_instance_editable(<blueprint>, "RoadThickness", true)
```

- [ ] **Step 4: 스케일/축 노드의 핀 이름 확인**

아래 세 노드는 핀을 확인하지 않았다. 반드시 먼저 조사할 것:
```
BlueprintTools.find_node_types(graph={"refPath":"/Game/TD/BP_BalloonPath.BP_BalloonPath:UserConstructionScript"},
                               type_id_filter="MakeVector2D", context_pins=[])
BlueprintTools.get_node_type_pins(graph=<위 그래프>, type_id="SplineMesh|SetStartScale")
BlueprintTools.get_node_type_pins(graph=<위 그래프>, type_id="SplineMesh|SetForwardAxis")
```
Expected: `SetStartScale`은 `self`, `StartScale`(Vector2D), `bUpdateMesh`. `SetForwardAxis`는 `self`, `ForwardAxis`(ESplineMeshAxis enum), `bUpdateMesh`. 실제 핀 이름을 Step 5 코드에 반영한다.

- [ ] **Step 5: 컨스트럭션 스크립트 작성**

```
BlueprintTools.write_graph_dsl(
  graph={"refPath":"/Game/TD/BP_BalloonPath.BP_BalloonPath:UserConstructionScript"},
  code=<아래>)
```

```lisp
(fn ConstructionScript ()
  (bind sp (Variables|Default|GetPathSpline))
  (bind num (Spline|GetNumberOfSplinePoints :self sp))
  (if (>= num 2)
    (bind w (/ (Variables|Default|GetRoadWidth) 100.0))
    (bind t (/ (Variables|Default|GetRoadThickness) 100.0))
    (bind sc (Math|Vector2D|MakeVector2D :X w :Y t))
    (for i (range (- num 1))
      (bind smc (AddComponent|Rendering|AddSplineMeshComponent
                  :self self :bManualAttachment false
                  :RelativeTransform (Math|Transform|MakeTransform)))
      (Components|StaticMesh|SetStaticMesh :self smc :NewMesh "/Engine/BasicShapes/Cube.Cube")
      (SplineMesh|SetForwardAxis :self smc :ForwardAxis "X" :bUpdateMesh false)
      (SplineMesh|SetStartScale :self smc :StartScale sc :bUpdateMesh false)
      (SplineMesh|SetEndScale :self smc :EndScale sc :bUpdateMesh false)
      (SplineMesh|SetStartAndEnd :self smc
        :StartPos (Spline|GetLocationAtSplinePoint :self sp :PointIndex i :CoordinateSpace "Local")
        :StartTangent (Spline|GetTangentAtSplinePoint :self sp :PointIndex i :CoordinateSpace "Local")
        :EndPos (Spline|GetLocationAtSplinePoint :self sp :PointIndex (+ i 1) :CoordinateSpace "Local")
        :EndTangent (Spline|GetTangentAtSplinePoint :self sp :PointIndex (+ i 1) :CoordinateSpace "Local")
        :bUpdateMesh true))))
```

스플라인 점이 2개 미만이면 `(if (>= num 2) …)`가 통째로 건너뛴다 — 스펙의 "점 2개 미만이면 도로 생성 건너뜀" 요구사항.

- [ ] **Step 6: 읽어서 노드가 제대로 잡혔는지 확인 — 이 단계를 건너뛰지 말 것**

```
BlueprintTools.read_graph_dsl(graph={"refPath":"/Game/TD/BP_BalloonPath.BP_BalloonPath:UserConstructionScript"})
```
Expected: 첫 줄이 `(bind _pathspline (|GetPathSpline))`. **`Class|PCGLandscapeSplineData|GetSpline`이 보이면 실패**다 — 컴포넌트 이름 충돌이므로 이름을 고쳐야 한다.

- [ ] **Step 7: 컴파일**

```
BlueprintTools.compile_blueprint(blueprint={"refPath":"/Game/TD/BP_BalloonPath.BP_BalloonPath"},
                                 warnings_as_errors=true)
```
Expected: 에러 없음. (컴파일 성공만으로는 Step 6을 대신할 수 없다.)

- [ ] **Step 8: GetPathLength 함수 추가**

```
BlueprintTools.add_function_graph(blueprint=<BP>, name="GetPathLength")
BlueprintTools.add_function_param(...)  ; 출력 ReturnValue: float
```
정확한 시그니처는 `describe_toolset`으로 확인. 그래프 내용:
```lisp
(fn GetPathLength ()
  (return (Spline|GetSplineLength :self (Variables|Default|GetPathSpline))))
```

- [ ] **Step 9: GetTransformAtDistance 함수 추가**

입력 `Distance` (float), 출력 `ReturnValue` (Transform).
```lisp
(fn GetTransformAtDistance (Distance)
  (return (Spline|GetTransformAtDistanceAlongSpline
            :self (Variables|Default|GetPathSpline)
            :Distance Distance
            :CoordinateSpace "World"
            :bUseScale false)))
```
`CoordinateSpace`는 반드시 `"World"` — 기본값이 `"Local"`이라 생략하면 풍선이 엉뚱한 곳으로 간다.

- [ ] **Step 10: 두 함수 읽고 컴파일**

```
BlueprintTools.read_graph_dsl(graph=<GetPathLength 그래프>)
BlueprintTools.read_graph_dsl(graph=<GetTransformAtDistance 그래프>)
BlueprintTools.compile_blueprint(blueprint=<BP>, warnings_as_errors=true)
```
Expected: `(|GetPathSpline)`이 보이고, `CoordinateSpace`가 `"World"`로 남아 있다.

- [ ] **Step 11: 레벨에 배치**

```
SceneTools.add_to_scene_from_asset(
  asset_path="/Game/TD/BP_BalloonPath", name="BalloonPath",
  xform={"location":{"x":0,"y":0,"z":0},
         "rotation":{"pitch":0,"yaw":0,"roll":0},
         "scale":{"x":1,"y":1,"z":1}})
```

- [ ] **Step 12: 스플라인 점 9개 설정**

배치된 액터의 `PathSpline` 컴포넌트에 아래 점을 설정한다. 모두 로컬 공간, z = 20.

| # | X | Y | Z |
|---|---|---|---|
| 0 | −3500 | −3000 | 20 |
| 1 | −1000 | −3000 | 20 |
| 2 | 1500 | −2000 | 20 |
| 3 | 2500 | 0 | 20 |
| 4 | 1000 | 1500 | 20 |
| 5 | −1500 | 1000 | 20 |
| 6 | −2500 | 2500 | 20 |
| 7 | 0 | 3200 | 20 |
| 8 | 3500 | 3000 | 20 |

`ObjectTools.set_properties`로 `SplineCurves`를 직접 쓰는 건 구조가 복잡하다. `ActorTools.get_components(<BalloonPath>, <SplineComponent 클래스>)`로 컴포넌트를 얻은 뒤, 스플라인 점 설정 노드(`Spline|SetLocationAtSplinePoint`, `Spline|AddSplinePoint`, `Spline|ClearSplinePoints`)를 찾아 처리한다. 기본 스플라인은 점 2개로 시작하므로 `ClearSplinePoints` 후 `AddSplinePoint` × 9가 가장 단순하다.
정확한 노드 ID는 `find_node_types(graph=<아무 그래프>, type_id_filter="SplinePoint", context_pins=[])`로 찾는다.

- [ ] **Step 13: 도로 생성 검증**

```
ActorTools.get_components(actor=<BalloonPath>, component_type={"refPath":"/Script/Engine.SplineMeshComponent"})
```
Expected: **8개** (점 9개 → 구간 8개).
```
SceneTools.find_actors(name="BalloonPath", tag="", collision_channels=[])
ActorTools.get_actor_bounds(actor=<BalloonPath>)
```
Expected: 바운드가 대략 x ∈ [−3500, 3500], y ∈ [−3000, 3200] 범위를 덮는다. 바운드가 0에 가까우면 도로가 안 깔린 것이다.

- [ ] **Step 14: 저장 후 커밋**

```
AssetTools.save_assets(["/Game/TD/BP_BalloonPath", "/Game/Maps/TDMap"])
```
```bash
cd C:/Work/VibeCoding
git add ue5/ViveCodingUE/Content/TD/BP_BalloonPath.uasset ue5/ViveCodingUE/Content/Maps/TDMap.umap
git commit -m "Add BP_BalloonPath with spline road generation"
```

---

### Task 3: BP_Balloon — 길 따라 이동 후 소멸

**목표:** 수동 배치한 풍선 하나가 길을 따라 이동하고 끝점에서 사라진다.

**Files:**
- Create: `/Game/TD/BP_Balloon`
- Modify: `/Game/Maps/TDMap` (검증용 풍선 1개 임시 배치)

**Interfaces:**
- Consumes:
  - `BP_BalloonPath.GetPathLength()` → Float
  - `BP_BalloonPath.GetTransformAtDistance(Distance: Float)` → Transform (월드, 스케일 미포함)
- Produces:
  - 변수 `Path` (`BP_BalloonPath` 오브젝트 레퍼런스, 인스턴스 편집 가능) — Task 4의 스포너가 주입한다
  - 변수 `Speed` (Float, 기본 400)

- [ ] **Step 1: 필요한 노드 핀 확인**

```
BlueprintTools.find_node_types(graph=<임시로 BP_Balloon EventGraph>, type_id_filter="IsValid", context_pins=[])
BlueprintTools.find_node_types(graph=<동일>, type_id_filter="DestroyActor", context_pins=[])
BlueprintTools.get_node_type_pins(graph=<동일>, type_id="Transformation|SetActorLocationAndRotation")
```
Expected: `Utilities|IsValid`(멀티exec, `(:"Is Valid")` / `(:"Is Not Valid")`), `Game|Actor|DestroyActor` 계열, `SetActorLocationAndRotation`의 `NewLocation`/`NewRotation`/`bSweep`/`bTeleport` 핀. 실제 이름을 이후 코드에 반영한다.

또한 `BP_BalloonPath`의 함수 노드 ID를 찾는다:
```
BlueprintTools.find_node_types(graph=<동일>, type_id_filter="GetTransformAtDistance", context_pins=[])
BlueprintTools.find_node_types(graph=<동일>, type_id_filter="GetPathLength", context_pins=[])
```
Expected: `Class|BP_BalloonPath|GetTransformAtDistance` 같은 형태.

- [ ] **Step 2: 블루프린트 생성 + 메시 컴포넌트**

```
BlueprintTools.create(folder_path="/Game/TD", asset_name="BP_Balloon",
                      asset_type={"refPath": "/Script/Engine.Actor"})
ActorTools.add_component(owner={"refPath":"/Game/TD/BP_Balloon.BP_Balloon"},
                         component_type={"refPath":"/Script/Engine.StaticMeshComponent"},
                         name="BalloonMesh")
```
**이름은 `BalloonMesh`** — `Mesh`는 충돌 위험이 있다.

- [ ] **Step 3: 메시/스케일/콜리전 설정**

CDO의 `BalloonMesh`에 `/Engine/BasicShapes/Sphere.Sphere`를 지정하고 상대 스케일 0.6, 콜리전 `NoCollision`으로 둔다.
```
BlueprintTools.get_default_object(blueprint={"refPath":"/Game/TD/BP_Balloon.BP_Balloon"})
ObjectTools.list_properties(<BalloonMesh 컴포넌트>)
ObjectTools.set_properties(<BalloonMesh>, "{\"StaticMesh\": \"/Engine/BasicShapes/Sphere.Sphere\", \"RelativeScale3D\": {\"x\":0.6,\"y\":0.6,\"z\":0.6}}")
```
정확한 프로퍼티명은 `list_properties` 출력을 따른다.

- [ ] **Step 4: 변수 추가**

```
BlueprintTools.add_object_variable(blueprint=<BP_Balloon>, name="Path",
  object_class={"refPath":"/Game/TD/BP_BalloonPath.BP_BalloonPath_C"})
BlueprintTools.add_variable(blueprint=<BP_Balloon>, name="Speed", type_name="float")
BlueprintTools.add_variable(blueprint=<BP_Balloon>, name="DistanceAlongPath", type_name="float")
BlueprintTools.set_variable_instance_editable(<BP_Balloon>, "Path", true)
BlueprintTools.set_variable_instance_editable(<BP_Balloon>, "Speed", true)
```
```
ObjectTools.set_properties(<BP_Balloon CDO>, "{\"Speed\": 400.0, \"DistanceAlongPath\": 0.0}")
```

- [ ] **Step 5: 이벤트 그래프 작성**

```lisp
(event EventBeginPlay
  (Utilities|IsValid (Variables|Default|GetPath)
    (:"Is Valid")
    (:"Is Not Valid"
      (Development|PrintString "BP_Balloon: Path is not set - destroying")
      (Game|Actor|DestroyActor :self self))))

(event EventTick (DeltaSeconds)
  (bind p (Variables|Default|GetPath))
  (Utilities|IsValid p
    (:"Is Valid"
      (bind d (+ (Variables|Default|GetDistanceAlongPath) (* (Variables|Default|GetSpeed) DeltaSeconds)))
      (Variables|Default|SetDistanceAlongPath d)
      (if (>= d (Class|BP_BalloonPath|GetPathLength p))
        (Game|Actor|DestroyActor :self self)
        (else
          (bind xf (Class|BP_BalloonPath|GetTransformAtDistance p d))
          (Transformation|SetActorLocationAndRotation
            :self self
            :NewLocation (.location xf)
            :NewRotation (.rotation xf)
            :bSweep false
            :bTeleport true))))
    (:"Is Not Valid")))
```
`SetActorTransform`을 쓰면 안 된다 — 스플라인 트랜스폼의 스케일이 풍선의 0.6을 덮어쓴다. (Step 1에서 확인한 실제 노드 ID/핀 이름으로 치환할 것.)

- [ ] **Step 6: 읽고 컴파일**

```
BlueprintTools.read_graph_dsl(graph={"refPath":"/Game/TD/BP_Balloon.BP_Balloon:EventGraph"})
BlueprintTools.compile_blueprint(blueprint=<BP_Balloon>, warnings_as_errors=true)
```
Expected: `(|GetPath)`, `(|GetSpeed)` 등 컴포넌트/변수 게터가 올바르게 잡히고, 엉뚱한 `Class|…` 노드가 없다.

- [ ] **Step 7: 검증용 풍선 1개 배치 후 Path 연결**

```
SceneTools.add_to_scene_from_asset(asset_path="/Game/TD/BP_Balloon", name="TestBalloon",
  xform={"location":{"x":-3500,"y":-3000,"z":20},
         "rotation":{"pitch":0,"yaw":0,"roll":0},
         "scale":{"x":1,"y":1,"z":1}})
ObjectTools.set_properties(<TestBalloon>, "{\"Path\": \"/Game/Maps/TDMap.TDMap:PersistentLevel.BalloonPath\"}")
```
(BalloonPath의 실제 refPath는 `find_actors`로 확인.)

- [ ] **Step 8: 이동 검증 — 트랜스폼 2회 비교**

```
EditorAppToolset.StartPIE({"bSimulate": false, "PlayMode": "PlayMode_InViewPort", "WarmupSeconds": 2.0})
SceneTools.find_actors(name="TestBalloon", tag="", collision_channels=[])
ActorTools.get_actor_transform(actor=<PIE 풍선>)      ; 1회차 기록
```
약 1초 뒤:
```
ActorTools.get_actor_transform(actor=<PIE 풍선>)      ; 2회차
```
Expected: 위치가 변했고, 이동 거리가 `Speed × 경과시간` ≈ 400cm 근처다. 시작점 (−3500, −3000)에서 스플라인을 따라 +X 방향으로 진행한다.

- [ ] **Step 9: 소멸 검증**

전체 길이 ÷ 400cm/s 만큼(대략 40~50초) 기다린 뒤:
```
SceneTools.find_actors(name="TestBalloon", tag="", collision_channels=[])
```
Expected: 빈 배열 — 끝점에서 소멸했다.
```
EditorAppToolset.StopPIE()
```
시간이 오래 걸리면 `Speed`를 임시로 2000으로 올려 확인하고 되돌린다.

- [ ] **Step 10: 검증용 풍선 제거 후 커밋**

```
SceneTools.remove_from_scene(actor=<에디터의 TestBalloon>)
AssetTools.save_assets(["/Game/TD/BP_Balloon", "/Game/Maps/TDMap"])
```
```bash
cd C:/Work/VibeCoding
git add ue5/ViveCodingUE/Content/TD/BP_Balloon.uasset ue5/ViveCodingUE/Content/Maps/TDMap.umap
git commit -m "Add BP_Balloon that follows the path and despawns at the end"
```

---

### Task 4: BP_BalloonSpawner — 타이머 스폰

**목표:** 풍선이 1초 간격으로 계속 나와 길을 따라가고, 개수가 무한히 늘지 않는다.

**Files:**
- Create: `/Game/TD/BP_BalloonSpawner`
- Modify: `/Game/Maps/TDMap` (스포너 배치)

**Interfaces:**
- Consumes:
  - `BP_Balloon`의 변수 `Path` (스폰 후 주입)
  - `BP_BalloonPath.GetTransformAtDistance(Distance: Float)` → Transform (거리 0 = 시작지점)
- Produces: 없음

- [ ] **Step 1: 블루프린트 생성 + 변수**

```
BlueprintTools.create(folder_path="/Game/TD", asset_name="BP_BalloonSpawner",
                      asset_type={"refPath": "/Script/Engine.Actor"})
BlueprintTools.add_object_variable(blueprint=<BP_BalloonSpawner>, name="Path",
  object_class={"refPath":"/Game/TD/BP_BalloonPath.BP_BalloonPath_C"})
BlueprintTools.add_variable(blueprint=<BP_BalloonSpawner>, name="SpawnInterval", type_name="float")
BlueprintTools.set_variable_instance_editable(<BP_BalloonSpawner>, "Path", true)
BlueprintTools.set_variable_instance_editable(<BP_BalloonSpawner>, "SpawnInterval", true)
ObjectTools.set_properties(<BP_BalloonSpawner CDO>, "{\"SpawnInterval\": 1.0}")
```
`BalloonClass` 변수는 만들지 않는다 — 스펙에 있었으나 지금 쓰이는 클래스가 하나뿐이라 YAGNI다. 나중에 풍선 종류가 생기면 그때 추가한다.

- [ ] **Step 2: SpawnBalloon 커스텀 이벤트 작성**

`SetTimerbyFunctionName`은 이름으로 호출하므로 **커스텀 이벤트 이름이 정확히 `SpawnBalloon`** 이어야 한다.

```lisp
(event SpawnBalloon
  (bind p (Variables|Default|GetPath))
  (bind xf (Class|BP_BalloonPath|GetTransformAtDistance p 0.0))
  (bind b (Game|SpawnActorfromClass
            :Class "/Game/TD/BP_Balloon.BP_Balloon_C"
            :SpawnTransform xf
            :CollisionHandlingOverride "AlwaysSpawn"))
  (Class|BP_Balloon|SetPath b p))
```
스폰된 풍선의 `Path` 변수 설정 노드 ID는 Step 1 이후 다음으로 확인한다:
```
BlueprintTools.find_node_types(graph=<BP_BalloonSpawner EventGraph>, type_id_filter="SetPath", context_pins=[])
```
`SpawnActorfromClass`의 정확한 핀은:
```
BlueprintTools.get_node_type_pins(graph=<동일>, type_id="Game|SpawnActorfromClass")
```
로 확인한다. `:Class`는 `_C` 접미사가 붙은 **클래스 경로 문자열**이며 따옴표 필수다.

- [ ] **Step 3: BeginPlay 작성**

```lisp
(event EventBeginPlay
  (Utilities|IsValid (Variables|Default|GetPath)
    (:"Is Valid"
      (Utilities|Time|SetTimerbyFunctionName
        :Object self
        :FunctionName "SpawnBalloon"
        :Time (Variables|Default|GetSpawnInterval)
        :bLooping true))
    (:"Is Not Valid"
      (Development|PrintString "BP_BalloonSpawner: Path is not set - spawner disabled"))))
```
Path가 없으면 **타이머를 걸지 않는다.** 조용히 아무 일도 안 일어나는 것보다 낫다.

- [ ] **Step 4: 읽고 컴파일**

```
BlueprintTools.read_graph_dsl(graph={"refPath":"/Game/TD/BP_BalloonSpawner.BP_BalloonSpawner:EventGraph"})
BlueprintTools.compile_blueprint(blueprint=<BP_BalloonSpawner>, warnings_as_errors=true)
```
Expected: `FunctionName`이 `"SpawnBalloon"` 문자열로 남아 있고, 커스텀 이벤트 이름과 정확히 일치한다.

- [ ] **Step 5: 레벨에 배치하고 Path 연결**

```
SceneTools.add_to_scene_from_asset(asset_path="/Game/TD/BP_BalloonSpawner", name="BalloonSpawner",
  xform={"location":{"x":-3500,"y":-3000,"z":100},
         "rotation":{"pitch":0,"yaw":0,"roll":0},
         "scale":{"x":1,"y":1,"z":1}})
ObjectTools.set_properties(<BalloonSpawner>, "{\"Path\": \"/Game/Maps/TDMap.TDMap:PersistentLevel.BalloonPath\"}")
```

- [ ] **Step 6: 스폰 검증 — 개수 추이**

```
EditorAppToolset.StartPIE({"bSimulate": false, "PlayMode": "PlayMode_InViewPort", "WarmupSeconds": 3.0})
SceneTools.find_actors(name="BP_Balloon", tag="", collision_channels=[])
```
Expected: 3초 워밍업 후 대략 2~4개.
약 5초 뒤 다시:
```
SceneTools.find_actors(name="BP_Balloon", tag="", collision_channels=[])
```
Expected: 개수가 늘었지만 스폰 간격에 맞는 수준(8초 ≈ 8개 이하). 폭주하면 타이머가 잘못 걸린 것이다.

- [ ] **Step 7: 소멸로 개수가 안정되는지 검증**

전체 길이 ÷ 400cm/s(약 40~50초)보다 오래 기다린 뒤:
```
SceneTools.find_actors(name="BP_Balloon", tag="", collision_channels=[])
```
Expected: 개수가 대략 `경로통과시간 ÷ SpawnInterval` 근처에서 **안정된다**(계속 증가하지 않는다). 이것이 끝점 소멸이 돌고 있다는 증거다.
로그 확인 후 종료:
```
EditorAppToolset.StopPIE()
```
Expected: 경고/에러 없음.

- [ ] **Step 8: 저장 후 커밋**

```
AssetTools.save_assets(["/Game/TD/BP_BalloonSpawner", "/Game/Maps/TDMap"])
```
```bash
cd C:/Work/VibeCoding
git add ue5/ViveCodingUE/Content/TD/BP_BalloonSpawner.uasset ue5/ViveCodingUE/Content/Maps/TDMap.umap
git commit -m "Add BP_BalloonSpawner that spawns balloons on a looping timer"
```

---

## 완료 조건

스펙의 성공 기준 5개가 전부 충족된다:

1. `TDMap` PIE → 로그인 화면 없이 탑다운 (Task 1 Step 6)
2. 꼬불꼬불한 도로가 보인다 (Task 2 Step 13 — 스플라인 메시 8개 + 바운드)
3. 풍선이 1초 간격으로 나와 길을 따라간다 (Task 4 Step 6)
4. 끝점에서 사라지고 개수가 안정된다 (Task 3 Step 9, Task 4 Step 7)
5. 블루프린트 3개가 에러 없이 컴파일되고 PIE 로그가 깨끗하다 (각 Task의 컴파일 스텝, Task 4 Step 7)
