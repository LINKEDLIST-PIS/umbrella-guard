"""setup_ble 이 출력한 라벨 정보로 QR 라벨 PNG 를 만든다.

QR 형식 (docs/APP_SPEC.md 4.4):  UG1:<MAC 12자리 16진수>:<PIN 6자리>
라벨 구성: QR 코드 + 기기 ID(MAC 끝 6자리) + PIN 숫자 (QR 을 못 찍는 경우 수동 입력용)

사용
  python tools/make_qr.py                 # 실행 후 QR 문자열을 붙여넣기 (권장: 셸 기록에 PIN 이 남지 않음)
  python tools/make_qr.py --out D:/labels # 저장 폴더 지정 (기본: labels/)

출력 파일에는 PIN 이 들어 있다. labels/ 는 .gitignore 에 포함되어 있으며,
저장소/메신저/클라우드에 올리지 말 것. 인쇄 후에는 파일을 지워도 된다.
"""
from __future__ import annotations

import argparse
import getpass
import re
import sys
from pathlib import Path

PAYLOAD_RE = re.compile(r"^UG1:([0-9A-F]{12}):(\d{6})$")


def load_font(size: int):
    from PIL import ImageFont

    for name in ("arialbd.ttf", "arial.ttf", "DejaVuSans-Bold.ttf", "DejaVuSans.ttf",
                 "/System/Library/Fonts/Supplemental/Arial Bold.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default(size=size)


def make_label(payload: str, out_dir: Path) -> Path:
    import qrcode
    from PIL import Image, ImageDraw

    m = PAYLOAD_RE.match(payload)
    if not m:
        raise ValueError("형식이 다릅니다. 예: UG1:884AEA72C66C:xxxxxx (MAC 12자리 대문자 16진수, PIN 6자리)")
    mac, pin = m.groups()
    dev_id = mac[-6:]

    qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, box_size=10, border=2)
    qr.add_data(payload)
    qr.make(fit=True)
    qr_img = qr.make_image(fill_color="black", back_color="white").convert("RGB")

    pad = 30
    title_f, body_f, pin_f = load_font(34), load_font(26), load_font(44)
    lines = [
        ("Umbrella Guard", title_f),
        (f"UMBRELLA  ·  ID {dev_id}", body_f),
        (f"PIN  {pin[:3]} {pin[3:]}", pin_f),
    ]
    probe = ImageDraw.Draw(Image.new("RGB", (1, 1)))
    heights = [probe.textbbox((0, 0), t, font=f)[3] for t, f in lines]
    widths = [probe.textbbox((0, 0), t, font=f)[2] for t, f in lines]
    w = max(qr_img.width, *widths) + pad * 2
    h = pad + heights[0] + 16 + qr_img.height + 16 + heights[1] + 12 + heights[2] + pad

    img = Image.new("RGB", (w, h), "white")
    d = ImageDraw.Draw(img)
    y = pad
    d.text(((w - widths[0]) // 2, y), lines[0][0], font=title_f, fill="black")
    y += heights[0] + 16
    img.paste(qr_img, ((w - qr_img.width) // 2, y))
    y += qr_img.height + 16
    d.text(((w - widths[1]) // 2, y), lines[1][0], font=body_f, fill="black")
    y += heights[1] + 12
    d.text(((w - widths[2]) // 2, y), lines[2][0], font=pin_f, fill="black")
    d.rectangle([4, 4, w - 5, h - 5], outline="black", width=2)  # 자르는 선

    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / f"UMBRELLA-{dev_id}.png"
    img.save(out, dpi=(300, 300))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, default=Path("labels"), help="저장 폴더 (기본 %(default)s)")
    args = ap.parse_args()

    try:
        import qrcode  # noqa: F401
        import PIL  # noqa: F401
    except ImportError:
        print("필요한 패키지가 없습니다: pip install -r tools/requirements.txt", file=sys.stderr)
        return 1

    # 입력 화면에 PIN 이 다시 보이지 않도록 getpass 로 받는다
    raw = getpass.getpass("setup_ble 의 'QR:' 뒤 문자열을 붙여넣고 Enter (화면에 표시되지 않음): ")
    payload = raw.strip().removeprefix("QR:").strip().upper()
    try:
        out = make_label(payload, args.out)
    except ValueError as e:
        print(e, file=sys.stderr)
        return 1
    print(f"라벨 저장: {out}")
    print("PIN 이 들어 있는 파일입니다. 인쇄 후 삭제하거나 안전한 곳에 보관하세요. (저장소에는 올라가지 않음)")
    return 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main())
