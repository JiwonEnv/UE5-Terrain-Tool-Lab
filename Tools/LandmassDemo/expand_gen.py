# 영지 확장 방식 비교용 칸 땅 저장 파일 6개 (① 계단식 터 · ② 정사각형 +4칸) · 각 3단계
import json, os
W = H = 40
OUT = r"E:\Project\LandmassTest\Saved\CozyTiles"
os.makedirs(OUT, exist_ok=True)

def save(name, level, ground, locked):
    d = {"version": 1, "width": W, "height": H, "level": level, "ground": ground, "locked": [1 if v else 0 for v in locked]}
    with open(os.path.join(OUT, name + ".json"), "w", encoding="utf-8") as f: json.dump(d, f, separators=(",", ":"))

def rect(x0, x1, y0, y1): return lambda x, y: x0 <= x <= x1 and y0 <= y <= y1

# ① 계단식 터: 바깥은 +X+Y 쪽으로 오르는 비탈(잠김 · 0~4층) · 터는 비탈을 깎은 평평한 자리
S, T2, T3 = rect(6, 17, 14, 25), rect(18, 25, 14, 25), rect(20, 29, 26, 33)
terraces = [(S, 0), (T2, 1), (T3, 2)]
for stage in (1, 2, 3):
    lv, gr, lk = [], [], []
    for y in range(H):
        for x in range(W):
            slope = max(0, min(4, (x + y - 30) // 6))
            l, open_ = slope, False
            for i, (r, tl) in enumerate(terraces):
                if r(x, y): l = tl; open_ = i < stage
            lv.append(l); gr.append(0); lk.append(not open_)
    save(f"Expand1_{stage}", lv, gr, lk)

# ② 정사각형: 평지 · 가운데 12×12에서 단계마다 가로·세로 +4칸
for stage in (1, 2, 3):
    n = 12 + 4 * (stage - 1); a = (W - n) // 2; r = rect(a, a + n - 1, a, a + n - 1)
    lv, gr, lk = [], [], []
    for y in range(H):
        for x in range(W):
            lv.append(0); gr.append(0); lk.append(not r(x, y))
    save(f"Expand2_{stage}", lv, gr, lk)
print(sorted(os.listdir(OUT)))
