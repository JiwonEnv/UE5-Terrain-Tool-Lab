# UE5 Terrain Tool Lab

언리얼 엔진 5.8 지형 도구 시험 프로젝트입니다.
[CozyRealm](https://github.com/JiwonEnv/Unreal-Engine-5-AI-Assisted-Cozy-Realm)(힐링 방치형 영지 경영 게임)에 쓸 지형 도구를 본 게임과 떼어 놓고 시험하고 발전시킵니다.

## 왜 만들었나

CozyRealm의 핵심은 "소박한 땅이 내 손으로 아름다운 정원이 되어 가는 것"입니다. 평평한 판보다 높낮이 있는 영지가 배치하는 재미가 크다고 보고 언덕·경사로를 넣었습니다. 그런데 지형을 숫자와 코드로 만들다 보니 전체 모양을 읽기 어렵고, 원하는 높이·모양을 만들기도 어려웠습니다.

그래서 땅을 둘로 나눴습니다.

| | B. 플레이어 땅 고치기 (주인공) | A. 산·터 설계 도구 (보조) |
|---|---|---|
| 쓰는 사람 | 게임을 하는 사람 | 게임을 만드는 사람 (다른 개발자도 플러그인으로) |
| 쓰는 때 | 게임 플레이 중 | 언리얼 에디터, 출시 전 |
| 범위 | 영지 + 넓혀 갈 땅 | 영지를 둘러싼 산 풍경 · 영지 터 자리 |
| 방식 | 칸 단위 층 올리기·내리기, 물·길 (모동숲 방식) | Landscape 높이 |

이 저장소에서는 다음을 시험합니다.

1. 에픽의 **Landmass** 플러그인이 A에 필요한 일을 얼마나 해 주는지 (Landmass로 충분하면 직접 만들 필요가 없음)
2. 같은 산(계단식 터 포함)을 Landmass와 우리 편집기로 만들어 시간·고치기·모양 비교
3. 영지 확장 방식: 산비탈 계단식 터가 하나씩 열리는 방식 vs 정사각형으로 넓어지는 방식

## 들어 있는 것

- `Plugins/CozyTerrainEditor` — CozyRealm에서 만든 지형 편집기 사본입니다. 위에서 보는 지도 화면, 평지·산·길, 영지 칸 맞춤, Landscape 편집 레이어 적용·복구 기능이 있습니다.
- 엔진의 **Landmass** 플러그인이 켜져 있습니다 (UE 5.8 기준 Beta).
- `Source/TerrainLab` — 빈 게임 모듈입니다. 플러그인을 빌드하는 데만 씁니다.

## 여는 방법

1. Unreal Engine 5.8을 설치합니다.
2. `TerrainLab.uproject`를 오른쪽 클릭해 "Generate Visual Studio project files"를 실행하거나, 아래 명령으로 빌드합니다.
   ```
   Engine\Build\BatchFiles\Build.bat TerrainLabEditor Win64 Development -Project="<경로>\TerrainLab.uproject"
   ```
3. `TerrainLab.uproject`를 엽니다. 지형 편집기는 메뉴 **창(Window) → 지형 편집기**에 있습니다 (콘솔 명령 `Cozy.TerrainEditor`). 자세한 사용법은 `Plugins/CozyTerrainEditor/사용방법.md`.

## 기록

작업 과정은 노션 작업 카드와 개발 블로그에 남깁니다. 이 프로젝트는 AI(Claude Code)와 함께 만들었습니다.
