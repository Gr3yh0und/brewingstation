# Brewing Station 3 — Case

A 3D-printable enclosure is provided in [`case/brewingstation3_case.scad`](case/brewingstation3_case.scad) (OpenSCAD).

---

## Design

Two-part box — print each part separately, no supports needed:

| Part | Print orientation | Key features |
|---|---|---|
| **Tray** (bottom + walls) | Upright, open top up | 4× screw bosses with M3 heat-set insert holes; 4× PCB standoffs (12 mm) |
| **Lid** (top plate) | Face up, flat | 2× OLED windows (26 × 14 mm); 4× M3 countersunk screw holes |

---

## Dimensions

| Property | Value |
|---|---|
| Outer footprint | 126 × 106 mm |
| Total height (assembled) | 90 mm |
| Internal cavity | 120 × 100 × 84 mm |
| Clearance around PCB | 10 mm each side |
| PCB height above floor | 15 mm (3 mm floor + 12 mm standoffs) |
| Wire space below PCB | 12 mm |
| Wire space above PCB | ~70 mm |

---

## Fasteners

| Item | Spec | Qty |
|---|---|---|
| Lid screws | M3 × 10 mm flat-head (countersunk) | 4 |
| Heat-set inserts | M3 × 4 mm brass knurl inserts | 4 |

Press inserts into the tray bosses with a soldering iron. Screws pass through the countersunk lid holes and thread into the inserts.

---

## Panel Cutouts

All side and lid holes (USB, connectors, buttons, sensors) are drilled by the user after printing. Pre-modelled openings:
- 2× rectangular OLED windows on the lid (positions adjustable via `oled1_cx` / `oled2_cx` in the `.scad` file)

---

## Recommended Panel Layout

| Face | Contents |
|---|---|
| **Top (lid)** | 2× OLED displays, 6× metal pushbuttons with LED rings (Ø16 mm hole each) |
| **Right side** | 3× DS18B20 connectors (J7/J8/J9), IDS2 cable (J1), button+LED panel (J15–J20), spare LEDs (J21/J22) |
| **Back** | Power switch (SW1), PExt power input (J3), USB-C (ESP32 DevKit) |
| **Left / front** | Optional: UART header (J11), GPIO breakouts (J12–J14) |
