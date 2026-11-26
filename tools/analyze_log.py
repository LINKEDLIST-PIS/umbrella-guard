"""logger 데이터 분석: 정지 구간 노이즈, 움직임 이벤트 자동 검출/분류, main 감지기 시뮬레이션.

입력
  - SD 카드의 LOGnn.CSV
  - pio device monitor 로그 (logger 의 d / p 명령으로 출력한 "----- LOGnn.CSV 시작 -----" 구간)
    실시간 보기(v) 출력은 10 Hz 라 분석에서 제외한다.

사용
  python tools/analyze_log.py logs/device-monitor-xxx.log
  python tools/analyze_log.py LOG04.CSV --plot              # PNG 그래프도 저장
  python tools/analyze_log.py a.log --rate 1500 --angle 25  # 감지기(fw 1.1) 파라미터 바꿔 보기

해석 기준 (MPU-6050 기본 범위)
  가속도 16384 LSB = 1 g,  자이로 131 LSB = 1 dps
  dAcc = |ax-기준| + |ay-기준| + |az-기준|
  gsum = |gx| + |gy| + |gz|

main 감지기 (fw 1.1.0, src/main/main.cpp 와 같은 정수 연산)
  길이 방향 축 = 기준 가속도에서 절댓값이 가장 큰 축
  rate  = 나머지 두 축 |g - 오프셋| 합의 EMA(1/32)          > --rate  이면 경보
  angle = 가속도 EMA(1/16) 방향과 기준 자세 사이 각도(도)   > --angle 이면 경보
  기준 자세/자이로 오프셋은 펌웨어의 SETTLING 대신 '가장 조용한 1 초' 평균을 쓴다.
  --acc/--gyro/--count 는 이전 방식(fw 1.0) 비교용.
"""
from __future__ import annotations

import argparse
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np

SAMPLE_MS = 20
ROW_RE = re.compile(r"^\s*(-?\d+(?:,-?\d+){7})\s*$")
HEADER_RE = re.compile(r"-----\s*(\S+)\s*시작\s*-----")
END_RE = re.compile(r"-----\s*끝\s*-----")

# main.cpp (fw 1.1.0) 기본값
DEFAULT_RATE = 1200      # TILT_RATE_THRESH
DEFAULT_ANGLE = 20.0     # TILT_ANGLE_DEG
RATE_SHIFT = 5           # TILT_RATE_SHIFT (EMA 1/32)
ACC_SHIFT = 4            # ACC_EMA_SHIFT   (EMA 1/16)

# 이전 방식 (fw 1.0.0) 기본값 - 비교용
DEFAULT_ACC = 2500
DEFAULT_GYRO = 2600
DEFAULT_COUNT = 3

# 이벤트 검출용 (main 감지기와 별개, '무언가 움직였다' 판정)
EVENT_GYRO = 1500     # 약 11 dps
EVENT_ACC = 1500      # 약 0.09 g
MERGE_GAP_MS = 500    # 이 간격 이내의 이벤트는 하나로 합침
CONTEXT_MS = 1000     # 이벤트 전/후 자세 비교 구간


@dataclass
class Segment:
    name: str
    data: np.ndarray  # (N, 8): ms, ax, ay, az, gx, gy, gz, mark


def parse(path: Path) -> list[Segment]:
    text = path.read_text(encoding="utf-8", errors="replace").splitlines()
    has_header = any(HEADER_RE.search(l) for l in text)
    segs: list[Segment] = []
    cur_name, rows = (None, []) if has_header else (path.name, [])
    for line in text:
        if has_header:
            m = HEADER_RE.search(line)
            if m:
                cur_name, rows = m.group(1), []
                continue
            if END_RE.search(line) and cur_name:
                if rows:
                    segs.append(Segment(cur_name, np.array(rows, dtype=np.int64)))
                cur_name, rows = None, []
                continue
            if cur_name is None:
                continue  # 구간 밖 (실시간 보기 등)
        m = ROW_RE.match(line)
        if m:
            rows.append([int(x) for x in m.group(1).split(",")])
    if not has_header and rows:
        segs.append(Segment(cur_name, np.array(rows, dtype=np.int64)))
    return segs


def rest_baseline(d: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """가장 조용한 1 초 창의 평균 가속도를 기준 자세로, 그 창 인덱스 반환."""
    g = np.abs(d[:, 4:7]).sum(axis=1)
    w = 50
    if len(d) <= w:
        return d[:, 1:4].mean(axis=0), np.arange(len(d))
    csum = np.convolve(g, np.ones(w), mode="valid")
    i = int(np.argmin(csum))
    return d[i:i + w, 1:4].mean(axis=0), np.arange(i, i + w)


def angle_deg(a: np.ndarray, b: np.ndarray) -> float:
    c = float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-9))
    return math.degrees(math.acos(max(-1.0, min(1.0, c))))


def classify(dur_ms: int, n_over: int, tilt: float) -> str:
    if n_over <= 2:
        return "스파이크 (1~2 샘플, 노이즈/순간 충격)"
    if tilt >= 15:
        return "자세 변화 (꺼냄/이동 의심)"
    if dur_ms < 1500:
        return "짧은 흔들림 (부딪힘/툭 침)"
    return "긴 흔들림, 자세 복귀 (흔들었다 내려놓음/진동)"


def find_events(d: np.ndarray, base: np.ndarray):
    t = d[:, 0]
    dacc = np.abs(d[:, 1:4] - base).sum(axis=1)
    gsum = np.abs(d[:, 4:7]).sum(axis=1)
    hot = (gsum > EVENT_GYRO) | (dacc > EVENT_ACC)
    idx = np.where(hot)[0]
    events = []
    if len(idx) == 0:
        return events, dacc, gsum
    start = prev = idx[0]
    for i in list(idx[1:]) + [None]:
        if i is not None and t[i] - t[prev] <= MERGE_GAP_MS:
            prev = i
            continue
        s, e = start, prev
        pre = d[(t >= t[s] - CONTEXT_MS) & (t < t[s]), 1:4]
        post = d[(t > t[e]) & (t <= t[e] + CONTEXT_MS), 1:4]
        tilt = angle_deg(pre.mean(axis=0), post.mean(axis=0)) if len(pre) and len(post) else float("nan")
        n_over = int(hot[s:e + 1].sum())
        events.append(dict(
            t0=int(t[s]), t1=int(t[e]), dur=int(t[e] - t[s]) + SAMPLE_MS, n=n_over,
            peak_g=int(gsum[s:e + 1].max()), peak_a=int(dacc[s:e + 1].max()),
            sat=bool((np.abs(d[s:e + 1, 1:7]) >= 32767).any()),
            tilt=tilt, kind=classify(int(t[e] - t[s]) + SAMPLE_MS, n_over, tilt),
            mark=int(np.bincount(d[s:e + 1, 7]).argmax()),
        ))
        if i is not None:
            start = prev = i
    return events, dacc, gsum


REARM_QUIET_SAMPLES = 100  # 시뮬레이션: 경보 후 2 초 조용하면 다시 ARMED 로 간주


def simulate_v11(d: np.ndarray, base: np.ndarray, goff: np.ndarray, rate_thr: int, angle_deg: float):
    """fw 1.1.0 의 ARMED 판정을 정수 연산까지 그대로 재현.
    실제 main 은 경보 후 DISARM 까지 유지되므로, 여기서는 경보 후 2 초 동안 조용해지면
    같은 기준으로 다시 무장한 것으로 보고 경보 '발생 횟수'를 센다.
    반환: (경보 시각 목록, rate EMA 배열, angle 배열)"""
    base_i = [int(round(v)) for v in base]
    goff_i = [int(round(v)) for v in goff]
    along = int(np.argmax(np.abs(base_i)))
    lateral = [i for i in range(3) if i != along]
    base_norm = max(1.0, math.sqrt(sum(v * v for v in base_i)))
    cos_thr = math.cos(math.radians(angle_deg))

    dacc = np.abs(d[:, 1:4] - base).sum(axis=1)
    gsum = np.abs(d[:, 4:7]).sum(axis=1)
    raw_quiet = (dacc <= 1500) & (gsum <= 1500)

    def reset():
        return 0, [v << ACC_SHIFT for v in base_i]

    rate_q, acc_q = reset()
    rates = np.zeros(len(d))
    angles = np.zeros(len(d))
    alarms, alarmed, quiet = [], False, 0
    for i in range(len(d)):
        a = [int(v) for v in d[i, 1:4]]
        g = [int(v) for v in d[i, 4:7]]
        if alarmed:
            quiet = quiet + 1 if raw_quiet[i] else 0
            if quiet >= REARM_QUIET_SAMPLES:
                alarmed = False
                rate_q, acc_q = reset()
            continue
        r = sum(abs(g[k] - goff_i[k]) for k in lateral)
        rate_q += r - (rate_q >> RATE_SHIFT)
        rate = rate_q >> RATE_SHIFT
        for k in range(3):
            acc_q[k] += a[k] - (acc_q[k] >> ACC_SHIFT)
        lp = [acc_q[k] >> ACC_SHIFT for k in range(3)]
        n = math.sqrt(sum(v * v for v in lp))
        c = (sum(lp[k] * base_i[k] for k in range(3)) / (n * base_norm)) if n >= 1 else 1.0
        rates[i] = rate
        angles[i] = math.degrees(math.acos(max(-1.0, min(1.0, c))))
        if rate > rate_thr or c < cos_thr:
            alarms.append(int(d[i, 0]))
            alarmed, quiet = True, 0
    return alarms, rates, angles


def simulate_legacy(d: np.ndarray, base: np.ndarray, acc: int, gyro: int, count: int):
    """이전 방식(fw 1.0.0)의 ARMED 판정 재현 (비교용).
    경보 후 2 초 동안 조용해지면 다시 무장한 것으로 보고 경보 '발생 횟수'를 센다."""
    dacc = np.abs(d[:, 1:4] - base).sum(axis=1)
    gsum = np.abs(d[:, 4:7]).sum(axis=1)
    over = (dacc > acc) | (gsum > gyro)
    alarms, run, alarmed, quiet = [], 0, False, 0
    for i, o in enumerate(over):
        if alarmed:
            quiet = 0 if o else quiet + 1
            if quiet >= REARM_QUIET_SAMPLES:
                alarmed, run = False, 0
            continue
        run = run + 1 if o else 0
        if run >= count:
            alarms.append(int(d[i, 0]))
            alarmed, quiet = True, 0
    return alarms


def fmt_s(ms: int) -> str:
    return f"{ms / 1000:6.2f}s"


def report(seg: Segment, args) -> None:
    d = seg.data
    t = d[:, 0]
    print(f"\n=== {seg.name}: {len(d)} 행, {fmt_s(int(t[0]))} ~ {fmt_s(int(t[-1]))} "
          f"({(t[-1] - t[0]) / 1000:.1f} 초) ===")
    dt = np.diff(t)
    late = int((dt > SAMPLE_MS * 2).sum())
    if late:
        print(f"  ! 샘플 간격 {SAMPLE_MS * 2} ms 초과 {late} 회 (최대 {int(dt.max())} ms)")
    marks = np.unique(d[:, 7])
    print(f"  mark 값: {', '.join(map(str, marks))}" + ("  (라벨 없음 - 실험 시 0~9 로 구간 표시 권장)" if len(marks) == 1 else ""))

    base, rest_idx = rest_baseline(d)
    g = math.degrees(math.atan2(math.hypot(base[1], base[2]), base[0]))
    print(f"  기준 자세 (가장 조용한 1 초): ax={base[0]:.0f} ay={base[1]:.0f} az={base[2]:.0f} "
          f"|a|={np.linalg.norm(base) / 16384:.3f} g, X축과 중력 사이 {g:.1f}°")

    events, dacc, gsum = find_events(d, base)
    quiet = np.ones(len(d), bool)
    for ev in events:
        quiet &= ~((t >= ev["t0"] - CONTEXT_MS) & (t <= ev["t1"] + CONTEXT_MS))
    if quiet.sum() > 50:
        qa, qg = dacc[quiet], gsum[quiet]
        print(f"  정지 구간 노이즈 ({quiet.sum()} 샘플): "
              f"dAcc p50={np.median(qa):.0f} p99={np.percentile(qa, 99):.0f} max={qa.max():.0f} | "
              f"gsum p50={np.median(qg):.0f} p99={np.percentile(qg, 99):.0f} max={qg.max():.0f}")
        gyro_off = d[quiet, 4:7].mean(axis=0)
        print(f"  자이로 오프셋 (정지 평균): gx={gyro_off[0]:.0f} gy={gyro_off[1]:.0f} gz={gyro_off[2]:.0f} "
              f"(합 {np.abs(gyro_off).sum():.0f} LSB = {np.abs(gyro_off).sum() / 131:.1f} dps)")

    print(f"  움직임 이벤트 {len(events)} 개 (gsum>{EVENT_GYRO} 또는 dAcc>{EVENT_ACC}):")
    for k, ev in enumerate(events, 1):
        print(f"   {k:2d}. {fmt_s(ev['t0'])} ~ {fmt_s(ev['t1'])}  {ev['dur']:5d} ms  "
              f"초과 {ev['n']:3d} 샘플  peak gsum={ev['peak_g']:6d} ({ev['peak_g'] / 131:5.0f} dps) "
              f"dAcc={ev['peak_a']:6d} ({ev['peak_a'] / 16384:4.2f} g)  기울기 변화 {ev['tilt']:5.1f}°"
              f"{'  [센서 포화]' if ev['sat'] else ''}  mark={ev['mark']}")
        print(f"       -> {ev['kind']}")

    rest = rest_idx
    goff = d[rest, 4:7].mean(axis=0)

    def show(alarms):
        for a in alarms:
            ev = next((e for e in events if e["t0"] - 100 <= a <= e["t1"] + 500), None)
            m = int(d[np.searchsorted(t, a), 7])
            print(f"     {fmt_s(a)}  mark={m}  " + (f"<- 이벤트 '{ev['kind']}'" if ev else "<- 이벤트 밖"))

    alarms, rates, angles = simulate_v11(d, base, goff, args.rate, args.angle)
    print(f"  main 감지기 시뮬레이션 [fw 1.1] (RATE>{args.rate}, ANGLE>{args.angle:g}°): 경보 {len(alarms)} 회"
          f"   | 정지 구간 rate 최대 {rates[quiet].max() if quiet.any() else float('nan'):.0f}")
    show(alarms)
    legacy = simulate_legacy(d, base, args.acc, args.gyro, args.count)
    print(f"  (비교) 이전 방식 [fw 1.0] (ACC={args.acc}, GYRO={args.gyro}, COUNT={args.count}): 경보 {len(legacy)} 회")
    show(legacy)

    if args.plot:
        plot(seg, d, rates, angles, events, alarms, args)


def plot(seg, d, rates, angles, events, alarms, args):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    t = d[:, 0] / 1000
    fig, ax = plt.subplots(3, 1, figsize=(12, 8), sharex=True)
    for i, n in enumerate(["ax", "ay", "az"]):
        ax[0].plot(t, d[:, 1 + i] / 16384, lw=0.8, label=n)
    ax[0].set_ylabel("가속도 (g)")
    ax[0].legend(loc="upper right")
    # mark 구간 표시 (0 이 아닌 구간에 숫자)
    marks = d[:, 7]
    start = 0
    for i in range(1, len(d) + 1):
        if i == len(d) or marks[i] != marks[start]:
            if marks[start]:
                ax[0].text(t[start], 1.85, f"mark {marks[start]}", fontsize=8, va="top")
            start = i
    ax[1].plot(t, np.maximum(rates, 1), lw=0.8, color="tab:green")
    ax[1].axhline(args.rate, color="red", ls="--", lw=0.8, label=f"TILT_RATE_THRESH {args.rate}")
    ax[1].set_ylabel("기울이는 회전 EMA (LSB)")
    ax[1].set_yscale("log"); ax[1].set_ylim(10, 1e5)
    ax[1].legend(loc="upper right")
    ax[2].plot(t, angles, lw=0.8, color="tab:purple")
    ax[2].axhline(args.angle, color="red", ls="--", lw=0.8, label=f"TILT_ANGLE_DEG {args.angle:g}")
    ax[2].set_ylabel("기준 대비 기울기 (°)")
    ax[2].set_ylim(0, max(args.angle * 1.5, float(np.nanmax(angles)) * 1.1 if len(angles) else 1))
    ax[2].set_xlabel("시간 (s)")
    ax[2].legend(loc="upper right")
    for ev in events:
        for a in ax:
            a.axvspan(ev["t0"] / 1000, ev["t1"] / 1000, color="orange", alpha=0.2)
    for al in alarms:
        for a in ax:
            a.axvline(al / 1000, color="red", lw=1)
    fig.suptitle(f"{seg.name}  (주황=움직임 이벤트, 빨간 선=fw 1.1 경보)")
    fig.tight_layout()
    out = Path(args.outdir) / f"{Path(seg.name).stem}.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out, dpi=110)
    plt.close(fig)
    print(f"  그래프 저장: {out}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+", type=Path)
    ap.add_argument("--rate", type=int, default=DEFAULT_RATE, help="TILT_RATE_THRESH (기본 %(default)s)")
    ap.add_argument("--angle", type=float, default=DEFAULT_ANGLE, help="TILT_ANGLE_DEG (기본 %(default)s)")
    ap.add_argument("--acc", type=int, default=DEFAULT_ACC, help="이전 방식 ACC_THRESH (기본 %(default)s)")
    ap.add_argument("--gyro", type=int, default=DEFAULT_GYRO, help="이전 방식 GYRO_THRESH (기본 %(default)s)")
    ap.add_argument("--count", type=int, default=DEFAULT_COUNT, help="이전 방식 TRIGGER_COUNT (기본 %(default)s)")
    ap.add_argument("--plot", action="store_true", help="구간별 PNG 그래프 저장")
    ap.add_argument("--outdir", default="logs/plots", help="그래프 저장 폴더 (기본 %(default)s)")
    args = ap.parse_args()

    if args.plot:
        import matplotlib
        from matplotlib import font_manager
        installed = {f.name for f in font_manager.fontManager.ttflist}
        for name in ("Malgun Gothic", "AppleGothic", "NanumGothic"):
            if name in installed:
                matplotlib.rcParams["font.family"] = name
                break
        matplotlib.rcParams["axes.unicode_minus"] = False

    for f in args.files:
        segs = parse(f)
        if not segs:
            print(f"{f}: 데이터 구간을 찾지 못함", file=sys.stderr)
            continue
        print(f"# {f}: 구간 {len(segs)} 개 ({', '.join(s.name for s in segs)})")
        for s in segs:
            report(s, args)
    return 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main())
