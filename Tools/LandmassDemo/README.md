# Landmass 기본 기능 시연 스크립트

시험 에디터(TerrainLab)를 켠 상태에서, 바깥 파이썬으로 에디터 안 파이썬을 원격 실행해 Landmass 브러시를 놓고 바꾼다.
사람이 쓸 때는 필요 없다. 녹화를 같은 순서로 다시 만들려고 남겨 둔다.

- `uepy.py` — 원격 실행 (`python uepy.py file 경로.py` · `python uepy.py code "..."`) · 엔진의 `remote_execution.py` 사용 · 프로젝트 설정에서 파이썬 원격 실행을 켜 둠
- `lmlib.py` — 브러시 만들기·스플라인 모양·지형 등록(`TerrainLabLibrary.AddBrushToLandscape`)
- `step.py` — 시연 단계 하나 실행 (`STEP` 변수: flat · hill · cap · up · down · soft · steep · mid · shape · pond · hill2 · move2 · delpond)
- `cam.py` — 뷰포트 카메라 위치 (`CAM=(x,y,z,보는x,보는y,보는z)`)

단계 실행 예 (bash):

```
python uepy.py code "STEP='hill'
$(cat step.py)"
```

## 같은 산 비교 (2026-10-27)

- `cmp_lm.py` — Landmass로 산 30m · 계단식 터 3개(0 / 2.4 / 4.8m) · 연못 만들기(`build`) · 고치기(`edit`: 둘째 터 2칸 넓히기 · 산 5m 낮추기) · 나무·회색 상자 시설·주인공 막대 꾸미기(`dress`)
- `cmp_cz.py` — 같은 설계를 CozyTerrainEditor 설계(`CozyTerrainLayout`)에 넣기(`setup`) · 적용은 편집기 창의 '지형에 적용' 버튼
- `qcam.py` — 게임과 같은 쿼터뷰 카메라 (원근 화각 30° · 내려다보는 각 -45° · 방향 45° · 거리 85m)
- 맵: `L_Compare_Landmass` · `L_Compare_CozyEditor` · 재질은 CozyRealm 본 게임 `Environment/Materials`·`Landscape` 사본
