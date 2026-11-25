"""logger 데이터 분석: 정지 구간 노이즈, 움직임 이벤트 자동 검출/분류, main 감지기 시뮬레이션.

입력
  - SD 카드의 LOGnn.CSV
  - pio device monitor 로그 (logger 의 d / p 명령으로 출력한 "----- LOGnn.CSV 시작 -----" 구간)
    실시간 보기(v) 출력은 10 Hz 라 분석에서 제외한다.

사용
  python tools/analyze_log.py logs/device-monitor-xxx.log
  python tools/analyze_log.py LOG04.CSV --plot          # PNG 그래프도 저장
  python tools/analyze_log.py a.log --acc 2500 --gyro 2600 --count 3   # 감지기 파라미터 바꿔 보기

해석 기준 (MPU-6050 기본 범위)
  가속도 16384 LSB = 1 g,  자이로 131 LSB = 1 dps
  dAcc = |ax-기준| + |ay-기준| + |az-기준|   (main.cpp 와 같은 정의)
  gsum = |gx| + |gy| + |gz|                  (main.cpp 와 같은 정의)
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

# main.cpp 기본값
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


def simulate_main(d: np.ndarray, base: np.ndarray, acc: int, gyro: int, count: int):
    """main.cpp 의 ARMED 판정을 재현. 실제 main 은 경보 후 DISARM 까지 유지되므로,
    여기서는 경보 후 2 초 동안 조용해지면 다시 무장한 것으로 보고 경보 '발생 횟수'를 센다."""
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

    alarms = simulate_main(d, base, args.acc, args.gyro, args.count)
    print(f"  main 감지기 시뮬레이션 (ACC={args.acc}, GYRO={args.gyro}, COUNT={args.count}): "
          f"경보 {len(alarms)} 회")
    for a in alarms:
        ev = next((e for e in events if e["t0"] - 100 <= a <= e["t1"] + 100), None)
        print(f"     {fmt_s(a)}  " + (f"<- 이벤트 '{ev['kind']}'" if ev else "<- 이벤트 밖 (오경보)"))

    if args.plot:
        plot(seg, d, dacc, gsum, events, alarms, args)


def plot(seg, d, dacc, gsum, events, alarms, args):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    t = d[:, 0] / 1000
    fig, ax = plt.subplots(3, 1, figsize=(12, 8), sharex=True)
    for i, n in enumerate(["ax", "ay", "az"]):
        ax[0].plot(t, d[:, 1 + i] / 16384, lw=0.8, label=n)
    ax[0].set_ylabel("가속도 (g)")
    ax[0].legend(loc="upper right")
    ax[1].plot(t, dacc, lw=0.8, color="tab:purple")
    ax[1].axhline(args.acc, color="red", ls="--", lw=0.8, label=f"ACC_THRESH {args.acc}")
    ax[1].set_ylabel("dAcc (LSB)")
    ax[1].set_yscale("log"); ax[1].set_ylim(50, 1.5e5)
    ax[1].legend(loc="upper right")
    ax[2].plot(t, gsum, lw=0.8, color="tab:green")
    ax[2].axhline(args.gyro, color="red", ls="--", lw=0.8, label=f"GYRO_THRESH {args.gyro}")
    ax[2].set_ylabel("gsum (LSB)")
    ax[2].set_yscale("log"); ax[2].set_ylim(50, 1.5e5)
    ax[2].set_xlabel("시간 (s)")
    ax[2].legend(loc="upper right")
    for ev in events:
        for a in ax:
            a.axvspan(ev["t0"] / 1000, ev["t1"] / 1000, color="orange", alpha=0.2)
    for al in alarms:
        for a in ax:
            a.axvline(al / 1000, color="red", lw=1)
    fig.suptitle(f"{seg.name}  (주황=이벤트, 빨간 선=main 경보)")
    fig.tight_layout()
    out = Path(args.outdir) / f"{Path(seg.name).stem}.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out, dpi=110)
    plt.close(fig)
    print(f"  그래프 저장: {out}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+", type=Path)
    ap.add_argument("--acc", type=int, default=DEFAULT_ACC, help="ACC_THRESH (기본 %(default)s)")
    ap.add_argument("--gyro", type=int, default=DEFAULT_GYRO, help="GYRO_THRESH (기본 %(default)s)")
    ap.add_argument("--count", type=int, default=DEFAULT_COUNT, help="TRIGGER_COUNT (기본 %(default)s)")
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
