#!/usr/bin/env python3
"""Render docs/wiring-diagram.png straight from diagram.json.

The schematic is derived, never hand-drawn, so it cannot drift out of step with
the simulator.  Run after any edit to diagram.json:

    python3 tools/generate_wiring_diagram.py

Requires Pillow (`python3 -c "import PIL"`).  Output is deterministic.
"""

import json
import pathlib
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:  # pragma: no cover - environment guard
    sys.exit("Pillow is required: install python-pillow")

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "diagram.json"
DST = ROOT / "docs" / "wiring-diagram.png"

SCALE = 2  # supersample factor for crisper text
W, H = 840, 710
BG = (245, 246, 248)
PANEL = (255, 255, 255)
BORDER = (203, 208, 214)
TEXT = (28, 32, 38)
MUTED = (108, 116, 126)
ACCENT = (11, 92, 173)

# Board pin reachable on the Blue Pill's short-label header, in the order the
# simulator addresses them.  Kept explicit rather than inferred so a typo in a
# label is visible here as a missing row.
MCU_PINS = [
    "3V3.1", "5V.1", "GND.1",
    "A0", "A1", "A2", "A3", "A4",
    "A9", "A10", "B0", "B6", "B7", "B8", "C13",
]

# Column of module boxes: id -> (caption, subtitle)
MODULES = {
    "dht22": ("DHT22", "Temp / humidity"),
    "ldr": ("LDR", "Ambient light"),
    "pir": ("PIR", "Motion"),
    "oled": ("SSD1306", "128x64 I2C"),
    "encoder": ("KY-040", "Rotary encoder"),
    "buzzer": ("Buzzer", "Alarm output"),
    "led": ("LED + 330R", "Fault indicator"),
}
MODULE_ORDER = ["dht22", "ldr", "pir", "oled", "encoder", "buzzer", "led"]

# Signal name per module pin, for the connection labels.
FUNCTIONS = {
    "dht22": {"VCC": "3V3", "GND": "GND", "SDA": "DATA"},
    "ldr": {"VCC": "3V3", "GND": "GND", "AO": "ANALOG"},
    "pir": {"VCC": "5V", "GND": "GND", "OUT": "MOTION"},
    "oled": {"VCC": "3V3", "GND": "GND", "SCL": "I2C1 SCL", "SDA": "I2C1 SDA"},
    "encoder": {"VCC": "3V3", "GND": "GND", "CLK": "ENC A", "DT": "ENC B", "SW": "ENC BTN"},
    "buzzer": {"1": "GND", "2": "ALARM"},
    "led": {"A": "ANODE", "C": "CATHODE"},
}


def font(size, bold=False):
    names = (
        ["DejaVuSans-Bold.ttf", "LiberationSans-Bold.ttf"]
        if bold
        else ["DejaVuSans.ttf", "LiberationSans.ttf"]
    )
    for name in names:
        for base in ("/usr/share/fonts/TTF", "/usr/share/fonts/truetype/dejavu"):
            p = pathlib.Path(base) / name
            if p.exists():
                return ImageFont.truetype(str(p), size * SCALE)
    return ImageFont.load_default()


def main():
    spec = json.loads(SRC.read_text())
    parts = {p["id"]: p for p in spec["parts"]}
    conns = spec["connections"]

    missing = [pid for pid in MODULE_ORDER if pid not in parts]
    if missing:
        sys.exit(f"diagram.json is missing module(s): {', '.join(missing)}")

    # Build module -> {pin: label} from the actual connection list.
    wiring = {pid: {} for pid in MODULE_ORDER}
    for a, b, *_ in conns:
        for src, dst in ((a, b), (b, a)):
            dev, _, pin = src.partition(":")
            if dev in wiring and dst.startswith("mcu:"):
                mcu_pin = dst.split(":", 1)[1].rstrip(".")
                wiring[dev][pin] = mcu_pin

    # Discrete pull-up resistors.  A wokwi-resistor that bridges a module signal
    # line to an MCU power rail sits *between* two nets, so the module -> MCU
    # map above cannot see it: the DHT22's data pin looks like an unpulled GPIO
    # even though diagram.json declares the bias resistor.  Recover them here so
    # the figure cannot quietly drop a part the simulator depends on.
    POWER_PINS = {"3V3.1", "5V.1", "GND.1"}
    pullups = []
    for part in spec["parts"]:
        pid = part["id"]
        if part["type"] != "wokwi-resistor" or pid in wiring:
            continue
        nets = [
            dst
            for a, b, *_ in conns
            for src, dst in ((a, b), (b, a))
            if src.partition(":")[0] == pid
        ]
        sig = pwr = None
        for net in nets:
            dev, _, pin = net.partition(":")
            if dev in wiring and wiring[dev].get(pin) not in (None, *POWER_PINS):
                sig = net
            elif dev == "mcu" and pin in POWER_PINS:
                pwr = net
        if sig and pwr:
            value = part.get("attrs", {}).get("value", "")
            try:
                value = f"{int(value) / 1000:g}k"
            except (TypeError, ValueError):
                pass
            pullups.append((sig, pwr, value))

    img = Image.new("RGB", (W * SCALE, H * SCALE), BG)
    d = ImageDraw.Draw(img)
    f_title = font(15, True)
    f_pin = font(9)
    f_mod = font(11, True)
    f_sub = font(8)
    f_leg = font(9)
    f_note = font(9)

    d.text((20 * SCALE, 14 * SCALE), "Blue Pill room monitor - Wokwi wiring",
           font=f_title, fill=TEXT)
    d.text((20 * SCALE, 34 * SCALE),
           f"derived from diagram.json - {len(spec['parts'])} parts, "
           f"{len(conns)} connections",
           font=f_note, fill=MUTED)

    # --- MCU pin strip on the left -------------------------------------
    x_pin_lbl = 24
    row_h = 24
    strip_top = 62
    strip_h = len(MCU_PINS) * row_h + 34
    d.rounded_rectangle(
        [(x_pin_lbl - 8) * SCALE, strip_top * SCALE,
         (x_pin_lbl + 232) * SCALE, (strip_top + strip_h) * SCALE],
        radius=6 * SCALE, fill=PANEL, outline=BORDER, width=SCALE)
    d.text((x_pin_lbl * SCALE, (strip_top + 10) * SCALE),
           "STM32F103C8T6 (Blue Pill)", font=f_mod, fill=ACCENT)

    pin_y = {}
    for i, pin in enumerate(MCU_PINS):
        y = strip_top + 34 + i * row_h
        pin_y[pin] = y
        d.text((x_pin_lbl * SCALE, y * SCALE), f"mcu:{pin}",
               font=f_pin, fill=TEXT)

    # --- module boxes on the right -------------------------------------
    x_mod = 560
    box_w, box_h = 250, 50
    gap = 18
    top = 74
    mod_y = {}
    for i, pid in enumerate(MODULE_ORDER):
        y = top + i * (box_h + gap)
        mod_y[pid] = y
        d.rounded_rectangle(
            [(x_mod - 10) * SCALE, y * SCALE,
             (x_mod + box_w) * SCALE, (y + box_h) * SCALE],
            radius=6 * SCALE, fill=PANEL, outline=BORDER, width=SCALE)
        caption, subtitle = MODULES[pid]
        d.text((x_mod * SCALE, (y + 9) * SCALE), caption, font=f_mod, fill=TEXT)
        d.text((x_mod * SCALE, (y + 28) * SCALE), subtitle,
               font=f_sub, fill=MUTED)

    # --- orthogonal wires ----------------------------------------------
    used_pins = sorted(
        {p for w in wiring.values() for p in w.values()},
        key=lambda p: pin_y.get(p, 0),
    )
    track = {p: x_pin_lbl + 96 + i * 11 for i, p in enumerate(used_pins)}

    box_links = {}
    for pid in MODULE_ORDER:
        pins = sorted(wiring[pid])
        y0 = mod_y[pid] + 14
        # stack the arrival rows so wires do not overlap on the box edge
        box_links[pid] = {
            pin: (x_mod - 10, y0 + i * 11) for i, pin in enumerate(pins)
        }

    for pid in MODULE_ORDER:
        for pin, (x_in, y_in) in box_links[pid].items():
            if pin not in wiring[pid]:
                continue
            mcu_pin = wiring[pid][pin]
            if mcu_pin not in pin_y:
                continue
            x_track = track[mcu_pin]
            y_src = pin_y[mcu_pin] + 8
            pts = [
                (x_pin_lbl + 70, y_src),
                (x_track, y_src),
                (x_track, y_in),
                (x_in, y_in),
            ]
            flat = [(px * SCALE, py * SCALE) for px, py in pts]
            d.line(flat, fill=BORDER, width=SCALE, joint="curve")
            d.ellipse([(flat[0][0] - 2 * SCALE, flat[0][1] - 2 * SCALE),
                       (flat[0][0] + 2 * SCALE, flat[0][1] + 2 * SCALE)],
                      fill=ACCENT)
            d.ellipse([(flat[-1][0] - 2 * SCALE, flat[-1][1] - 2 * SCALE),
                       (flat[-1][0] + 2 * SCALE, flat[-1][1] + 2 * SCALE)],
                      fill=ACCENT)
            fn = FUNCTIONS.get(pid, {}).get(pin, pin)
            d.text(((x_in - 96) * SCALE, (y_in - 11) * SCALE), fn,
                   font=f_pin, fill=MUTED)

    # --- pull-up resistors ---------------------------------------------
    # Drawn as a small resistor in the gap below the module's signal rows, with
    # junction dots on the two rails it ties together.
    for sig_net, pwr_net, value in pullups:
        mdev, _, mpin = sig_net.partition(":")
        _, _, ppin = pwr_net.partition(":")
        if mdev not in box_links or mpin not in box_links[mdev]:
            continue
        x_in, y_sig = box_links[mdev][mpin]
        y_pwr = next(
            (y for pin, (_, y) in box_links[mdev].items()
             if wiring[mdev].get(pin) == ppin),
            None,
        )
        if y_pwr is None:
            continue
        ymid = max(y_sig, y_pwr) + 22
        cx = x_in - 180
        half_w, half_h = 32, 6
        x_pwr_leg, x_sig_leg = cx - 48, cx + 48
        # leads and legs
        for (x0, y0), (x1, y1) in (
            ((cx - half_w, ymid), (x_pwr_leg, ymid)),
            ((cx + half_w, ymid), (x_sig_leg, ymid)),
            ((x_pwr_leg, ymid), (x_pwr_leg, y_pwr)),
            ((x_sig_leg, ymid), (x_sig_leg, y_sig)),
        ):
            d.line([(x0 * SCALE, y0 * SCALE), (x1 * SCALE, y1 * SCALE)],
                   fill=BORDER, width=SCALE)
        # body
        d.rectangle(
            [(cx - half_w) * SCALE, (ymid - half_h) * SCALE,
             (cx + half_w) * SCALE, (ymid + half_h) * SCALE],
            fill=PANEL, outline=ACCENT, width=SCALE)
        # junction dots where the legs meet the rails
        for jx, jy in ((x_pwr_leg, y_pwr), (x_sig_leg, y_sig)):
            d.ellipse([(jx * SCALE - 2 * SCALE, jy * SCALE - 2 * SCALE),
                       (jx * SCALE + 2 * SCALE, jy * SCALE + 2 * SCALE)],
                      fill=ACCENT)
        d.text(((cx - half_w - 78) * SCALE, (ymid - 6) * SCALE),
               f"{value} pull-up", font=f_pin, fill=ACCENT)

    # --- serial monitor -------------------------------------------------
    y_ser = 74 + len(MODULE_ORDER) * (box_h + gap) + 6
    d.rounded_rectangle(
        [(x_mod - 10) * SCALE, y_ser * SCALE,
         (x_mod + box_w) * SCALE, (y_ser + box_h) * SCALE],
        radius=6 * SCALE, fill=PANEL, outline=ACCENT, width=SCALE)
    d.text((x_mod * SCALE, (y_ser + 9) * SCALE), "$serialMonitor",
           font=f_mod, fill=ACCENT)
    d.text((x_mod * SCALE, (y_ser + 28) * SCALE), "USART1 @ 115200",
           font=f_sub, fill=MUTED)
    for i, (mcu_pin, tag) in enumerate((("A9", "RX"), ("A10", "TX"))):
        y_in = y_ser + 14 + i * 11
        x_track = track.get(mcu_pin, x_pin_lbl + 200)
        y_src = pin_y[mcu_pin] + 8
        d.line([((x_pin_lbl + 70) * SCALE, y_src * SCALE),
                (x_track * SCALE, y_src * SCALE),
                (x_track * SCALE, y_in * SCALE),
                ((x_mod - 10) * SCALE, y_in * SCALE)],
               fill=BORDER, width=SCALE, joint="curve")
        d.text(((x_mod - 96) * SCALE, (y_in - 11) * SCALE),
               f"UART {tag}", font=f_pin, fill=MUTED)

    # --- polarity + provenance notes ------------------------------------
    y_note = 630
    d.text((20 * SCALE, y_note * SCALE),
           "LED polarity: 3V3 -> 330R -> anode -> cathode -> mcu:C13 (active-low).",
           font=f_leg, fill=TEXT)
    d.text((20 * SCALE, (y_note + 16) * SCALE),
           "PC13 is driven HIGH at boot, so the LED is off in normal operation and",
           font=f_leg, fill=TEXT)
    d.text((20 * SCALE, (y_note + 32) * SCALE),
           "blinks only inside the FreeRTOS stack-overflow / malloc-failed hooks.",
           font=f_leg, fill=TEXT)
    d.text((20 * SCALE, (y_note + 56) * SCALE),
           "Reduce before committing:  python3 tools/generate_wiring_diagram.py",
           font=f_note, fill=MUTED)

    out = img.resize((W, H), Image.LANCZOS)
    DST.parent.mkdir(parents=True, exist_ok=True)
    out.save(DST, "PNG", optimize=True)
    print(f"wrote {DST.relative_to(ROOT)}  "
          f"({len(spec['parts'])} parts, {len(conns)} connections)")


if __name__ == "__main__":
    main()
