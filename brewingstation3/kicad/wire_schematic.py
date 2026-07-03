#!/usr/bin/env python3
"""
BrewingStation v3 — full schematic generator: placement + wiring.
Run: python wire_schematic.py → brewingstation3.kicad_sch
"""
import os
import re
import json

_n = 0
def uid():
    global _n; _n += 1
    h = f"{_n:032x}"
    return f"{h[0:8]}-{h[8:12]}-{h[12:16]}-{h[16:20]}-{h[20:32]}"

def load_existing_uuids(sch_path):
    """Parse an existing .kicad_sch and return (root_uuid, sym_uuids, pin_uuids).
    sym_uuids : {ref: sym_uuid}
    pin_uuids : {ref: {pin_num_str: uuid}}
    Returns (None, {}, {}) if file is missing.
    Handles both our compact format and KiCad's multi-line saved format."""
    if not os.path.exists(sch_path):
        return None, {}, {}
    with open(sch_path, encoding='utf-8') as f:
        text = f.read()
    # Root UUID: first (uuid "...") in the file header (before any placed symbols)
    root_m = re.search(r'\(uuid "([0-9a-f-]+)"\)', text[:1000])
    root_uuid = root_m.group(1) if root_m else None
    sym_uuids, pin_uuids = {}, {}
    for block in re.split(r'\n  (?=\(symbol \(lib_id )', text):
        if '(in_bom yes)' not in block:
            continue
        # \s* handles both our inline "(dnp no) (uuid ...)" and KiCad's multi-line format
        sm = re.search(r'\(dnp no\)\s*\(uuid "([0-9a-f-]+)"\)', block)
        if not sm:
            continue
        rm = re.search(r'\(reference "([^"]+)"\)', block)
        if not rm or rm.group(1).startswith('#'):
            continue
        ref = rm.group(1)
        sym_uuids[ref] = sm.group(1)
        pin_uuids[ref] = {pm.group(1): pm.group(2)
                         for pm in re.finditer(r'\(pin "([^"]+)" \(uuid "([0-9a-f-]+)"\)\)', block)}
    return root_uuid, sym_uuids, pin_uuids

P = 2.54
PROJECT = "brewingstation3"
_DIR      = os.path.dirname(os.path.abspath(__file__))
_SCH_PATH = os.path.join(_DIR, f"{PROJECT}.kicad_sch")
_MAP_PATH = os.path.join(_DIR, ".uuid_map.json")

def _load_uuids():
    """Load UUIDs from JSON store (stable) → fall back to parsing .kicad_sch."""
    if os.path.exists(_MAP_PATH):
        try:
            with open(_MAP_PATH, encoding='utf-8') as f:
                d = json.load(f)
            return d.get('root'), d.get('syms', {}), d.get('pins', {})
        except Exception:
            pass
    return load_existing_uuids(_SCH_PATH)

_exist_root, _exist_sym, _exist_pins = _load_uuids()
ROOT = _exist_root or uid()

def esc(s): return s.replace('"', '\\"')

# ── grid snap ─────────────────────────────────────────────────────────────────

def snap(v):
    return round(v / (P / 2)) * (P / 2)

def snap_y(y, n):
    return snap(y)

def snap_bw(bw):
    return max(1, round(bw / (2 * P))) * 2 * P

# ── symbol builder ─────────────────────────────────────────────────────────────

def sym(lib_name, ref, val, pins_l, pins_r, bw=15.24):
    bw  = snap_bw(bw)
    n   = max(len(pins_l), len(pins_r))
    bh  = (n + 1) * P
    hw  = bw / 2; hh = bh / 2
    sn  = lib_name.split(":")[-1]
    def pl(nm, num, ty, x, y, a):
        return (f'      (pin {ty} line (at {x:.3f} {y:.3f} {a}) (length {P:.3f})\n'
                f'        (name "{esc(nm)}" (effects (font (size 1.27 1.27))))\n'
                f'        (number "{esc(str(num))}" (effects (font (size 1.27 1.27)))))')
    lp = [pl(nm,nu,ty, -(hw+P), hh-P-i*P,   0) for i,(nm,nu,ty) in enumerate(pins_l)]
    rp = [pl(nm,nu,ty,  (hw+P), hh-P-i*P, 180) for i,(nm,nu,ty) in enumerate(pins_r)]
    return f"""    (symbol "{lib_name}" (pin_numbers (hide yes)) (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)
      (property "Reference" "{ref}" (at 0 {hh+P:.3f} 0) (effects (font (size 1.27 1.27))))
      (property "Value" "{val}" (at 0 {-(hh+P):.3f} 0) (effects (font (size 1.27 1.27))))
      (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
      (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
      (symbol "{sn}_0_1"
        (rectangle (start {-hw:.3f} {-hh:.3f}) (end {hw:.3f} {hh:.3f})
          (stroke (width 0) (type default)) (fill (type background))))
      (symbol "{sn}_1_1"
{chr(10).join(lp)}
{chr(10).join(rp)}
      )
    )"""

def pwrsym(name, shape="vcc"):
    sn = name.replace(".", "")
    if shape == "gnd":
        body = ('        (polyline (pts (xy 0 0) (xy 0 -1.27)) (stroke (width 0) (type default)) (fill (type none)))\n'
                '        (polyline (pts (xy 1.27 -1.27) (xy -1.27 -1.27)) (stroke (width 0) (type default)) (fill (type none)))\n'
                '        (polyline (pts (xy 0.762 -1.905) (xy -0.762 -1.905)) (stroke (width 0) (type default)) (fill (type none)))\n'
                '        (polyline (pts (xy 0.254 -2.54) (xy -0.254 -2.54)) (stroke (width 0) (type default)) (fill (type none)))')
        pd, vy = 270, -3.81
    else:
        body = ('        (polyline (pts (xy 0 0) (xy 0 2.54)) (stroke (width 0) (type default)) (fill (type none)))\n'
                '        (polyline (pts (xy -0.508 2.032) (xy 0 2.54) (xy 0.508 2.032)) (stroke (width 0) (type default)) (fill (type none)))')
        pd, vy = 90, 3.81
    return f"""    (symbol "power:{name}" (power) (pin_numbers (hide yes)) (pin_names (offset 0) (hide yes)) (exclude_from_sim no) (in_bom no) (on_board no)
      (property "Reference" "#PWR" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
      (property "Value" "{name}" (at 0 {vy:.3f} 0) (effects (font (size 1.27 1.27))))
      (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
      (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
      (symbol "{sn}_0_1"
{body})
      (symbol "{sn}_1_1"
        (pin power_in line (at 0 0 {pd}) (length 0)
          (name "{name}" (effects (font (size 1.27 1.27))))
          (number "1"    (effects (font (size 1.27 1.27))))))
    )"""

def pwrflag():
    return """    (symbol "power:PWR_FLAG" (power) (pin_numbers (hide yes)) (pin_names (offset 0) (hide yes)) (exclude_from_sim no) (in_bom no) (on_board no)
      (property "Reference" "#PWR" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
      (property "Value" "PWR_FLAG" (at 0 3.81 0) (effects (font (size 1.27 1.27))))
      (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
      (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
      (symbol "PWR_FLAG_0_1"
        (polyline (pts (xy 0 0) (xy 0 1.27)) (stroke (width 0) (type default)) (fill (type none)))
        (polyline (pts (xy 0 1.27) (xy 1.27 2.032) (xy 0 2.794) (xy 0 1.27)) (stroke (width 0) (type default)) (fill (type outline))))
      (symbol "PWR_FLAG_1_1"
        (pin power_out line (at 0 0 270) (length 0)
          (name "PWR_FLAG" (effects (font (size 1.27 1.27))))
          (number "1" (effects (font (size 1.27 1.27))))))
    )"""

# ── placed component ───────────────────────────────────────────────────────────

def place(lib_id, ref, val, x, y, fp="", pins_l=None, pins_r=None, rot=0, mirror=None,
          on_board=True, in_bom=True):
    pins = list(pins_l or []) + list(pins_r or [])
    sym_uuid = _exist_sym.get(ref) or uid()
    ep = _exist_pins.get(ref, {})
    def puuid(pn): return ep.get(str(pn)) or uid()
    pe = ("\n".join(f'    (pin "{esc(str(pn))}" (uuid "{puuid(pn)}"))' for _,pn,_ in pins)
          if pins else f'    (pin "1" (uuid "{puuid(1)}"))')
    n = max(len(pins_l or []), len(pins_r or []), 1)
    x = snap(x); y = snap_y(y, n)
    hh = (n + 1) * P / 2
    mirror_str = f' (mirror {mirror})' if mirror else ''
    ob = "yes" if on_board else "no"
    ib = "yes" if in_bom  else "no"
    return f"""  (symbol (lib_id "{lib_id}") (at {x:.3f} {y:.3f} {rot}){mirror_str} (unit 1)
    (exclude_from_sim no) (in_bom {ib}) (on_board {ob}) (dnp no) (uuid "{sym_uuid}")
    (property "Reference" "{ref}" (at {x:.3f} {y - hh - P:.3f} 0) (effects (font (size 1.27 1.27))))
    (property "Value" "{val}" (at {x:.3f} {y + hh + P:.3f} 0) (effects (font (size 1.27 1.27))))
    (property "Footprint" "{fp}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
    (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
{pe}
    (instances (project "{PROJECT}" (path "/{ROOT}" (reference "{ref}") (unit 1)))))"""

# ── wiring helpers ─────────────────────────────────────────────────────────────

_pn = 0; _fn = 0
def place_pwr(name, x, y):
    global _pn; _pn += 1
    ref = f"#PWR{_pn:03d}"
    return f"""  (symbol (lib_id "power:{name}") (at {x:.3f} {y:.3f} 0) (unit 1)
    (exclude_from_sim no) (in_bom no) (on_board no) (dnp no) (uuid "{uid()}")
    (property "Reference" "{ref}" (at {x:.3f} {y+1.27:.3f} 0) (effects (font (size 1.27 1.27)) (hide yes)))
    (property "Value" "{name}" (at {x:.3f} {y+1.27:.3f} 0) (effects (font (size 1.27 1.27))))
    (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
    (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
    (pin "1" (uuid "{uid()}"))
    (instances (project "{PROJECT}" (path "/{ROOT}" (reference "{ref}") (unit 1)))))"""

def place_flag(x, y):
    global _fn; _fn += 1
    ref = f"#FLG{_fn:03d}"
    return f"""  (symbol (lib_id "power:PWR_FLAG") (at {x:.3f} {y:.3f} 0) (unit 1)
    (exclude_from_sim no) (in_bom no) (on_board no) (dnp no) (uuid "{uid()}")
    (property "Reference" "{ref}" (at {x:.3f} {y+1.27:.3f} 0) (effects (font (size 1.27 1.27)) (hide yes)))
    (property "Value" "PWR_FLAG" (at {x:.3f} {y+2.54:.3f} 0) (effects (font (size 1.27 1.27))))
    (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
    (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
    (pin "1" (uuid "{uid()}"))
    (instances (project "{PROJECT}" (path "/{ROOT}" (reference "{ref}") (unit 1)))))"""

def wseg(x1, y1, x2, y2):
    return (f'  (wire (pts (xy {x1:.3f} {y1:.3f}) (xy {x2:.3f} {y2:.3f}))\n'
            f'    (stroke (width 0) (type default))\n'
            f'    (uuid "{uid()}"))')

def nlabel(name, x, y, angle=0):
    j = "right" if angle == 180 else "left"
    return (f'  (label "{esc(name)}" (at {x:.3f} {y:.3f} {angle}) (fields_autoplaced yes)\n'
            f'    (effects (font (size 1.27 1.27)) (justify {j}))\n'
            f'    (uuid "{uid()}"))')

def nconn(x, y):
    return f'  (no_connect (at {x:.3f} {y:.3f}) (uuid "{uid()}"))'

def junction(x, y):
    return f'  (junction (at {x:.3f} {y:.3f}) (diameter 0) (color 0 0 0 0)\n    (uuid "{uid()}"))'

# ── pin coordinate helpers ─────────────────────────────────────────────────────
# cx,cy = snapped component centre; idx = 0-based pin index from top; n = total pins; bw = symbol body width arg

def lpin(cx, cy, idx, n, bw=15):
    hw = snap_bw(bw) / 2
    hh = (n + 1) * P / 2
    return (cx - hw - P, cy - hh + P + idx * P)

def rpin(cx, cy, idx, n, bw=15):
    hw = snap_bw(bw) / 2
    hh = (n + 1) * P / 2
    return (cx + hw + P, cy - hh + P + idx * P)

STUB = 3 * P
OW_BUS_X = snap(62.23)  # OneWire vertical bus x-coordinate

# Buzzer CTRL routing constants (U1 IO0 → R8 left pin)
_BZ_CORNER_X = snap(93.98)   # aligns with Q1 centre x
_BZ_CORNER_Y = snap(95.25)
_BZ_R8_LEFT_X = snap(76.2)   # R8 left pin x
_BZ_R8_LEFT_Y = snap(83.82)  # R8 left pin y

# Accumulate elements
comps = []; pwr = []; wires = []

def lbl_l(name, px, py):
    wires.append(wseg(px, py, px - STUB, py))
    wires.append(nlabel(name, px - STUB, py, 180))

def lbl_r(name, px, py):
    wires.append(wseg(px, py, px + STUB, py))
    wires.append(nlabel(name, px + STUB, py, 0))

def pwr_l(name, px, py, stub=None, with_flag=False):
    s = STUB if stub is None else snap(stub)
    wires.append(wseg(px, py, px - s, py))
    pwr.append(place_pwr(name, px - s, py))
    if with_flag:
        pwr.append(place_flag(px - s, py))

def pwr_r(name, px, py, stub=None, with_flag=False):
    s = STUB if stub is None else snap(stub)
    wires.append(wseg(px, py, px + s, py))
    pwr.append(place_pwr(name, px + s, py))
    if with_flag:
        pwr.append(place_flag(px + s, py))

def nc(px, py):
    wires.append(nconn(px, py))

# ── pin type shorthands ────────────────────────────────────────────────────────
BI="bidirectional"; INP="input"; OUT="output"
PWR_IN="power_in"; PWR_OUT="power_out"; PASS="passive"

# ── symbol pin lists ───────────────────────────────────────────────────────────
ESP32_L = [("+3V3","1",PWR_OUT),("EN","2",INP),("IO4","3",BI),("IO5","4",BI),
           ("IO6","5",BI),("IO7","6",BI),("IO0","7",BI),("IO1","8",BI),
           ("IO8","9",BI),("IO10","10",BI),("IO11","11",BI),("IO2","12",BI),
           ("IO3","13",BI),("+5V","14",PWR_IN),("GND","15",PWR_IN),("NC","16",PASS)]
ESP32_R = [("GND","32",PWR_IN),("IO16~TX","31",OUT),("IO17~RX","30",INP),
           ("IO15","29",BI),("IO23","28",BI),("IO22","27",BI),("IO21","26",BI),
           ("IO20","25",BI),("IO19","24",BI),("IO18","23",BI),("IO9","22",BI),
           ("GND","21",PWR_IN),("IO13","20",BI),("IO12","19",BI),
           ("GND","18",PWR_IN),("NC","17",PASS)]

PCF_L = [("VSS","1",PWR_IN),("VDD","2",PWR_IN),("A0","3",INP),("A1","4",INP),
         ("A2","5",INP),("~INT","6",PASS),("SCL","7",INP),("SDA","8",BI)]
PCF_R = [("P0","9",OUT),("P1","10",OUT),("P2","11",OUT),("P3","12",OUT),
         ("P4","13",OUT),("P5","14",OUT),("P6","15",OUT),("P7","16",OUT)]

LS_L = [("LV1","1",BI),("LV2","2",BI),("LV","3",PWR_IN),("GND","4",PWR_IN),("LV3","5",BI),("LV4","6",BI)]
LS_R = [("HV1","7",BI),("HV2","8",BI),("HV","9",PWR_IN),("GND","10",PWR_IN),("HV3","11",BI),("HV4","12",BI)]

MAX_L  = [("VCC","1",PWR_IN),("GND","2",PWR_IN),("SDI","3",INP),("SDO","4",PASS),("SCK","5",INP),("CS","6",INP),("DRDY","7",PASS)]
BME_L  = [("VCC","1",PWR_IN),("GND","2",PWR_IN),("SCK","3",INP),("SDI","4",INP),("SDO","5",PASS),("CSB","6",INP)]
OLED_L = [("GND","1",PWR_IN),("VCC","2",PWR_IN),("SCL","3",INP),("SDA","4",BI)]
IDS2_L = [("VCC","1",PASS),("GND","2",PWR_IN),("WHITE","3",BI),("YELLOW","4",OUT),("BLUE","5",INP)]
RELAY_L= [("VCC","1",PWR_IN),("GND","2",PWR_IN),("IN","3",INP)]
R_L    = [("~","1",PASS)];  R_R = [("~","2",PASS)]
GX12_L = [("GND","1",PWR_IN),("+3V3","2",PWR_IN),("DQ","3",BI)]
PEXT_L = [("VCC","1",PASS),("GND","2",PWR_IN)]
PJMP_L = [("IDS2","1",PASS),("5V","2",PASS),("PEXT","3",PASS)]
PSW_L  = [("SW_A","1",PASS),("SW_B","2",PASS),("LED+","3",PASS),("LED-","4",PASS)]
BTN_LED_L  = [("GND","1",PWR_IN),("BTN","2",PASS),("LED","3",PASS)]
LED_SPARE_L= [("GND","1",PWR_IN),("LED","2",PASS)]
PWR_TAP_L  = [("GND","1",PWR_IN),("+3V3","2",PWR_IN),("+5V","3",PWR_IN)]
UART_HDR_L = [("GND","1",PWR_IN),("TX_IO16","2",BI),("RX_IO17","3",BI)]
GPIO_FREE_L= [("GND","1",PWR_IN),("IO23","2",BI)]
BZ_L       = [("+","1",PASS),("-","2",PASS)]
NPN_L      = [("B","1",INP)]
NPN_R      = [("C","2",PASS),("E","3",PASS)]
SW_PUSH_L  = [("A","1",PASS)]
SW_PUSH_R  = [("B","2",PASS)]
LED_IND_L  = [("A","1",PASS)]
LED_IND_R  = [("K","2",PASS)]

# ── lib symbols ────────────────────────────────────────────────────────────────
lib_syms = "\n".join([
    pwrsym("GND", "gnd"), pwrsym("+3V3"), pwrsym("+5V"), pwrflag(),
    sym("Brew:ESP32-C6-WROOM-1U","U","ESP32-C6-WROOM-1U", ESP32_L, ESP32_R, bw=22),
    sym("Brew:PCF8574",          "U","PCF8574",           PCF_L,   PCF_R),
    sym("Brew:LevelShifter_4CH", "U","LevelShifter_4CH",  LS_L,    LS_R),
    sym("Brew:MAX31865_Module",  "U","MAX31865",           MAX_L,   [], bw=15),
    sym("Brew:BME680_Module",    "U","BME680",             BME_L,   [], bw=15),
    sym("Brew:SSD1306",          "DS","SSD1306",           OLED_L,  [], bw=15),
    sym("Brew:GGM_IDS2",         "J","GGM_IDS2",          IDS2_L,  [], bw=15),
    sym("Brew:Relay_Module",     "K","Relay",              RELAY_L, [], bw=15),
    sym("Brew:R",                "R","R",                  R_L,     R_R, bw=5),
    sym("Brew:Conn_DS18B20",     "J","DS18B20",            GX12_L,  [], bw=15),
    sym("Brew:Conn_PExt",        "J","PExt",               PEXT_L,  [], bw=15),
    sym("Brew:PSource",            "J","PSource",              PJMP_L,  [], bw=15),
    sym("Brew:Conn_PSw",         "J","PSw",                PSW_L,      [], bw=15),
    sym("Brew:Conn_BTN_LED",     "J","BTN_LED",            BTN_LED_L,  [], bw=15),
    sym("Brew:Conn_LED_Spare",   "J","LED_Spare",          LED_SPARE_L,[], bw=15),
    sym("Brew:Conn_PWR_TAP",     "J","PWR_TAP",            PWR_TAP_L,  [], bw=15),
    sym("Brew:Conn_UART_HDR",    "J","UART_HDR",           UART_HDR_L, [], bw=15),
    sym("Brew:Conn_GPIO_FREE",   "J","GPIO_FREE",          GPIO_FREE_L,[], bw=15),
    sym("Brew:Buzzer",           "BZ","Buzzer",            BZ_L,       [], bw=10),
    sym("Brew:NPN",              "Q","MMBT2222A",          NPN_L,      NPN_R, bw=10),
    sym("Brew:SW_Push",          "SW","Button",            SW_PUSH_L,  SW_PUSH_R, bw=10),
    sym("Brew:LED_Indicator",    "LD","LED",               LED_IND_L,  LED_IND_R, bw=10),
])

def phoenix(n):
    return f"TerminalBlock_Phoenix:TerminalBlock_Phoenix_MPT-0,5-{n}-2.54_1x{n:02d}_P2.54mm_Horizontal"

# ── placement + wiring ─────────────────────────────────────────────────────────
# Helper: compute snapped centre and return it (so we can reuse for pin math)
def cx(x):      return snap(x)
def cy(y, n):   return snap_y(y, n)

# ── U1  ESP32-C6 ──────────────────────────────────────────────────────────────
U1x = cx(156.21); U1y = cy(115.57, max(len(ESP32_L), len(ESP32_R)))
comps.append(place("Brew:ESP32-C6-WROOM-1U","U1","ESP32-C6-WROOM-1U", 156.21, 115.57,
                   fp="PCM_Espressif:ESP32-C6-DevKitC-1", pins_l=ESP32_L, pins_r=ESP32_R))

NL = len(ESP32_L)
# Left pin wiring
LEFT_NETS = [None, None, "CS_MAX","SPI_CLK","SPI_MISO","SPI_MOSI",
             "BUZZER_CTRL","ADC_BTN", None,"GPIO10","OneWire","CS_BME","GPIO3_ADC",
             None, None, None]
LEFT_PWR  = ["+3V3", "+3V3", None,None,None,None, None,None, None,None,None,None,None,
             "+5V","GND", None]
for i in range(NL):
    px, py = lpin(U1x, U1y, i, NL, bw=22)
    if LEFT_PWR[i]:
        pwr_l(LEFT_PWR[i], px, py)
    elif LEFT_NETS[i] == "OneWire":
        wires.append(wseg(OW_BUS_X, py, px, py))  # bus → U1 IO11
    elif LEFT_NETS[i] == "BUZZER_CTRL":
        wires.append(wseg(px, py, _BZ_CORNER_X, py))
        wires.append(wseg(_BZ_CORNER_X, py, _BZ_CORNER_X, _BZ_CORNER_Y))
        wires.append(wseg(_BZ_CORNER_X, _BZ_CORNER_Y, _BZ_R8_LEFT_X, _BZ_CORNER_Y))
        wires.append(wseg(_BZ_R8_LEFT_X, _BZ_CORNER_Y, _BZ_R8_LEFT_X, _BZ_R8_LEFT_Y))
    elif LEFT_NETS[i]:
        lbl_l(LEFT_NETS[i], px, py)
    else:
        nc(px, py)

NR = len(ESP32_R)
# Right pin wiring
RIGHT_NETS = [None, "UART_TX","UART_RX","GPIO15","I2C_SDA","I2C_SCL",
              "RELAY_CTRL","IDS2_BLU_LV","IDS2_YEL_LV","IDS2_WHT_LV",
              None, None, None, None, None, None]
RIGHT_PWR  = ["GND", None,None,None,None,None, None,None,None,None,
              None,"GND",None,None,"GND",None]
for i in range(NR):
    px, py = rpin(U1x, U1y, i, NR, bw=22)
    if RIGHT_PWR[i]:
        pwr_r(RIGHT_PWR[i], px, py)
    elif RIGHT_NETS[i]:
        lbl_r(RIGHT_NETS[i], px, py)
    else:
        nc(px, py)

# ── J7/J8/J9  DS18B20 GX12 connectors ────────────────────────────────────────
for ref, ya, n in [("J7",105.41,1),("J8",130.81,2),("J9",149.86,3)]:
    comps.append(place("Brew:Conn_DS18B20", ref, f"DS18B20_{n}", 82.55, ya,
                       fp=phoenix(3), pins_l=GX12_L))
    jx = cx(82.55); jy = cy(ya, 3)
    px0,py0 = lpin(jx,jy,0,3); px1,py1 = lpin(jx,jy,1,3)
    px2,py2 = lpin(jx,jy,2,3)
    pwr_l("GND",  px0, py0, stub=P)
    pwr_l("+3V3", px1, py1, stub=2*P)
    wires.append(wseg(px2, py2, OW_BUS_X, py2))  # DQ → OneWire bus

# ── R1  4.7k pull-up (OneWire bus) ───────────────────────────────────────────
comps.append(place("Brew:R","R1","4k7", 55.88, 121.92,
                   fp="Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
                   pins_l=R_L, pins_r=R_R))
R1x = cx(55.88); R1y = cy(121.92, 1)
lx1,ly1 = lpin(R1x,R1y,0,1,bw=5)
rx1,ry1 = rpin(R1x,R1y,0,1,bw=5)
wires.append(wseg(rx1, ry1, OW_BUS_X, ry1))  # right pin → OneWire bus
pwr_l("+3V3",    lx1, ly1)

# ── OneWire bus (split at each T-junction + explicit junctions) ───────────────
_OW_Y_J7 = snap(107.95)  # J7 DQ
_OW_Y_R1 = snap(121.92)  # R1 right + U1 IO11
_OW_Y_J8 = snap(133.35)  # J8 DQ
_OW_Y_J9 = snap(152.40)  # J9 DQ
wires.append(wseg(OW_BUS_X, _OW_Y_J7, OW_BUS_X, _OW_Y_R1))
wires.append(wseg(OW_BUS_X, _OW_Y_R1, OW_BUS_X, _OW_Y_J8))
wires.append(wseg(OW_BUS_X, _OW_Y_J8, OW_BUS_X, _OW_Y_J9))
# Junctions required at every T-point so KiCad recognises the connection
for _ow_y in (_OW_Y_J7, _OW_Y_R1, _OW_Y_J8, _OW_Y_J9):
    wires.append(junction(OW_BUS_X, _ow_y))

# ── U5  MAX31865 ──────────────────────────────────────────────────────────────
comps.append(place("Brew:MAX31865_Module","U5","MAX31865", 82.55, 30.48,
                   fp="Brew:MAX31865_Module",
                   pins_l=MAX_L))
U5x = cx(82.55); U5y = cy(30.48, 7)
pwr_l("+3V3",    *lpin(U5x,U5y,0,7), stub=2*P)
pwr_l("GND",     *lpin(U5x,U5y,1,7), stub=P)
lbl_l("SPI_MOSI",*lpin(U5x,U5y,2,7))
lbl_l("SPI_MISO",*lpin(U5x,U5y,3,7))
lbl_l("SPI_CLK", *lpin(U5x,U5y,4,7))
lbl_l("CS_MAX",  *lpin(U5x,U5y,5,7))
nc(*lpin(U5x,U5y,6,7))   # DRDY unused

# ── U6  BME680 ────────────────────────────────────────────────────────────────
comps.append(place("Brew:BME680_Module","U6","BME680", 82.55, 63.50,
                   fp=phoenix(6), pins_l=BME_L))
U6x = cx(82.55); U6y = cy(63.50, 6)
pwr_l("+3V3",    *lpin(U6x,U6y,0,6), stub=2*P)
pwr_l("GND",     *lpin(U6x,U6y,1,6), stub=P)
lbl_l("SPI_CLK", *lpin(U6x,U6y,2,6))
lbl_l("SPI_MOSI",*lpin(U6x,U6y,3,6))
lbl_l("SPI_MISO",*lpin(U6x,U6y,4,6))
lbl_l("CS_BME",  *lpin(U6x,U6y,5,6))

# ── DS1/DS2  OLEDs ────────────────────────────────────────────────────────────
for ref, ya in [("DS1",80.01),("DS2",102.87)]:
    val = "SSD1306_0x3C" if ref=="DS1" else "SSD1306_0x3D"
    comps.append(place("Brew:SSD1306",ref,val, 232.41, ya,
                       fp=phoenix(4), pins_l=OLED_L))
    dx = cx(232.41); dy = cy(ya, 4)
    pwr_l("GND",     *lpin(dx,dy,0,4), stub=P)
    pwr_l("+3V3",    *lpin(dx,dy,1,4), stub=2*P)
    lbl_l("I2C_SCL", *lpin(dx,dy,2,4))
    lbl_l("I2C_SDA", *lpin(dx,dy,3,4))

# ── U3  BSS138 level shifter ──────────────────────────────────────────────────
comps.append(place("Brew:LevelShifter_4CH","U3","LevelShifter_4CH", 142.24, 170.18,
                   fp="Brew:LevelShifter_4CH",
                   pins_l=LS_L, pins_r=LS_R))
U3x = cx(142.24); U3y = cy(170.18, max(len(LS_L),len(LS_R)))
nc(*lpin(U3x,U3y,0,6))                        # ch1 LV — nc
lbl_l("IDS2_WHT_LV", *lpin(U3x,U3y,1,6))    # ch2: WHITE — aligns with J1 pin3
pwr_l("+3V3",         *lpin(U3x,U3y,2,6), stub=2*P)
pwr_l("GND",          *lpin(U3x,U3y,3,6), stub=P)
lbl_l("IDS2_YEL_LV", *lpin(U3x,U3y,4,6))    # ch3: YELLOW — aligns with J1 pin4
lbl_l("IDS2_BLU_LV", *lpin(U3x,U3y,5,6))    # ch4: BLUE — aligns with J1 pin5
nc(*rpin(U3x,U3y,0,6))                        # ch1 HV — nc
lbl_r("IDS2_WHT_HV", *rpin(U3x,U3y,1,6))    # ch2 HV: WHITE
pwr_r("+5V",          *rpin(U3x,U3y,2,6), stub=2*P)
pwr_r("GND",          *rpin(U3x,U3y,3,6), stub=P)
lbl_r("IDS2_YEL_HV", *rpin(U3x,U3y,4,6))    # ch3 HV: YELLOW
lbl_r("IDS2_BLU_HV", *rpin(U3x,U3y,5,6))    # ch4 HV: BLUE

# ── J1  GGM IDS2 connector ────────────────────────────────────────────────────
comps.append(place("Brew:GGM_IDS2","J1","GGM_IDS2", 212.09, 171.45,
                   fp=phoenix(5),
                   pins_l=IDS2_L))
J1x = cx(212.09); J1y = cy(171.45, 5)
lbl_l("IDS2_5V",     *lpin(J1x,J1y,0,5))
pwr_l("GND",          *lpin(J1x,J1y,1,5))
lbl_l("IDS2_WHT_HV", *lpin(J1x,J1y,2,5))
lbl_l("IDS2_YEL_HV", *lpin(J1x,J1y,3,5))
lbl_l("IDS2_BLU_HV", *lpin(J1x,J1y,4,5))

# ── K1  Relay module ──────────────────────────────────────────────────────────
comps.append(place("Brew:Relay_Module","K1","Relay", 232.41, 59.69,
                   fp="Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
                   pins_l=RELAY_L))
K1x = cx(232.41); K1y = cy(59.69, 3)
pwr_l("+5V",          *lpin(K1x,K1y,0,3), stub=2*P)
pwr_l("GND",          *lpin(K1x,K1y,1,3), stub=P)
lbl_l("RELAY_CTRL",   *lpin(K1x,K1y,2,3))


# ── J3  PExt external 5V input ────────────────────────────────────────────────
comps.append(place("Brew:Conn_PExt","J3","PExt_Conn", 125.73, 27.94,
                   fp=phoenix(2), pins_l=PEXT_L))
J3x = cx(125.73); J3y = cy(27.94, 2)
lbl_l("PEXT_5V", *lpin(J3x,J3y,0,2))
pwr_l("GND",      *lpin(J3x,J3y,1,2))

# ── J4  PSource power source selector ────────────────────────────────────────
# Pin 1 (top) = IDS2_5V, Pin 2 (middle) = PSOURCE_OUT, Pin 3 (bottom) = PEXT_5V
# A jumper cap bridges one source to the common output.
comps.append(place("Brew:PSource","J4","PSource", 175.26, 33.02,
                   fp="Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
                   pins_l=PJMP_L))
J4x = cx(175.26); J4y = cy(33.02, 3)
lbl_l("IDS2_5V",   *lpin(J4x,J4y,0,3))
lbl_l("PSOURCE_OUT", *lpin(J4x,J4y,1,3))
lbl_l("PEXT_5V",   *lpin(J4x,J4y,2,3))

# ── SW1  Power switch (PSource output → +5V rail) ────────────────────────────
comps.append(place("Brew:Conn_PSw","SW1","PSw_Conn", 138.43, 46.99,
                   fp=phoenix(4), pins_l=PSW_L))
SW1x = cx(138.43); SW1y = cy(46.99, 4)
lbl_l("PSOURCE_OUT", *lpin(SW1x,SW1y,0,4))
pwr_l("+5V",         *lpin(SW1x,SW1y,1,4), with_flag=True)   # PWR_FLAG for +5V net
lbl_l("PSW_LED",     *lpin(SW1x,SW1y,2,4))                   # LED+ via R17
pwr_l("GND",         *lpin(SW1x,SW1y,3,4))                   # LED-

# ── R17  Power switch LED current limiter (PSOURCE_OUT → PSW_LED) ─────────────
comps.append(place("Brew:R","R17","470R", 102.87, 48.26,
                   fp="Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
                   pins_l=R_L, pins_r=R_R))
R17x = cx(102.87); R17y = cy(48.26, 1)
lbl_l("PSOURCE_OUT", *lpin(R17x,R17y,0,1,bw=5))
lbl_r("PSW_LED",     *rpin(R17x,R17y,0,1,bw=5))

# ── U2  PCF8574 ───────────────────────────────────────────────────────────────
comps.append(place("Brew:PCF8574","U2","PCF8574", 232.41, 132.08,
                   fp="Package_DIP:DIP-16_W7.62mm", pins_l=PCF_L, pins_r=PCF_R))
U2x = cx(232.41); U2y = cy(132.08, max(len(PCF_L),len(PCF_R)))
pwr_l("GND",      *lpin(U2x,U2y,0,8), stub=P)     # VSS
pwr_l("+3V3",     *lpin(U2x,U2y,1,8), stub=2*P)   # VDD
pwr_l("GND",      *lpin(U2x,U2y,2,8), stub=P)     # A0
pwr_l("GND",      *lpin(U2x,U2y,3,8), stub=2*P)   # A1
pwr_l("GND",      *lpin(U2x,U2y,4,8), stub=P)     # A2
nc(*lpin(U2x,U2y,5,8))                   # ~INT
lbl_l("I2C_SCL",  *lpin(U2x,U2y,6,8))
lbl_l("I2C_SDA",  *lpin(U2x,U2y,7,8))
LED_PCTS = [0,20,40,60,80,100]
# Direct wires from U2 P0-P7 to LED resistors R2-R7,R15,R16
# Staggered corner_x: min(i,7-i)*P from rpin_x; i=0/7 go straight (no horiz stub)
_base_x = rpin(U2x, U2y, 0, 8)[0]
for i in range(8):
    px, py = rpin(U2x, U2y, i, 8)
    r_ry  = cy(88.90 + i * 12.70, 1)
    r_lpx = lpin(cx(256.54), r_ry, 0, 1, bw=5)[0]
    cx_   = _base_x + min(i, 7 - i) * P
    if cx_ != px:                              # skip zero-length stub for i=0,7
        wires.append(wseg(px,   py,   cx_,   py))
    wires.append(wseg(cx_,  py,   cx_,   r_ry))
    wires.append(wseg(cx_,  r_ry, r_lpx, r_ry))

# ── R2-R7 + R15-R16  LED current limiters (PCF → LED_n net) ─────────────────
# Each right pin gets a net label — J6 removed, replaced by J15-J22.
LED_RESISTORS = (
    [(f"R{i+2}","330R",i,f"LED_{p}") for i,p in enumerate(LED_PCTS)] +
    [("R15","330R",6,"LED_P6"), ("R16","330R",7,"LED_P7")]
)

for seq, (ref, val, pcf_idx, led_net) in enumerate(LED_RESISTORS):
    rx = cx(256.54); ry = cy(88.90 + seq * 12.70, 1)
    comps.append(place("Brew:R", ref, val, 256.54, 88.90 + seq * 12.70,
                       fp="Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
                       pins_l=R_L, pins_r=R_R))
    lbl_r(led_net, *rpin(rx, ry, 0, 1, bw=5))

# ── J10  PWR tap header ───────────────────────────────────────────────────────
comps.append(place("Brew:Conn_PWR_TAP","J10","PWR_TAP", 175.26, 53.34,
                   fp="Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
                   pins_l=PWR_TAP_L))
J10x = cx(175.26); J10y = cy(53.34, 3)
pwr_l("GND",  *lpin(J10x,J10y,0,3), stub=P)
pwr_l("+3V3", *lpin(J10x,J10y,1,3), stub=2*P)
pwr_l("+5V",  *lpin(J10x,J10y,2,3), stub=3*P)

# ── J11  UART header ──────────────────────────────────────────────────────────
comps.append(place("Brew:Conn_UART_HDR","J11","UART_HDR", 232.41, 39.37,
                   fp="Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
                   pins_l=UART_HDR_L))
J11x = cx(232.41); J11y = cy(39.37, 3)
pwr_l("GND",      *lpin(J11x,J11y,0,3))
lbl_l("UART_TX",  *lpin(J11x,J11y,1,3))
lbl_l("UART_RX",  *lpin(J11x,J11y,2,3))

# ── J12  GPIO free header ─────────────────────────────────────────────────────
comps.append(place("Brew:Conn_GPIO_FREE","J12","GPIO15", 274.32, 34.29,
                   fp=phoenix(2), pins_l=GPIO_FREE_L))
J12x = cx(274.32); J12y = cy(34.29, 2)
pwr_l("GND",   *lpin(J12x,J12y,0,2))
lbl_l("GPIO15", *lpin(J12x,J12y,1,2))

# ── J13  GPIO10 connector (GND + GPIO10) ─────────────────────────────────────
comps.append(place("Brew:Conn_GPIO_FREE","J13","GPIO10", 274.32, 52.07,
                   fp=phoenix(2), pins_l=GPIO_FREE_L))
J13x = cx(274.32); J13y = cy(52.07, 2)
pwr_l("GND",   *lpin(J13x,J13y,0,2))
lbl_l("GPIO10", *lpin(J13x,J13y,1,2))

# ── J14  GPIO3_ADC connector (GND + GPIO3) ───────────────────────────────────
comps.append(place("Brew:Conn_GPIO_FREE","J14","GPIO3_ADC", 274.32, 67.31,
                   fp=phoenix(2), pins_l=GPIO_FREE_L))
J14x = cx(274.32); J14y = cy(67.31, 2)
pwr_l("GND",      *lpin(J14x,J14y,0,2))
lbl_l("GPIO3_ADC", *lpin(J14x,J14y,1,2))

# ── BZ1/Q1/R8  Buzzer circuit ─────────────────────────────────────────────────
# +5V → BZ1(+)  BZ1(-) ← Q1(C)  Q1(E) → GND
# Q1(B) = R8 right pin (coincident at 86.36, 83.82 — no wire needed)
# BUZZER_CTRL routed from U1 IO0 via 4-segment path to R8 left (see U1 loop)
comps.append(place("Brew:Buzzer","BZ1","CPM121", 124.46, 82.55,
                   fp="Buzzer_Beeper:Buzzer_12x9.5RM7.6", pins_l=BZ_L))
BZ1x = cx(124.46); BZ1y = cy(82.55, 2)
pwr_l("+5V", *lpin(BZ1x,BZ1y,0,2,bw=10))   # + pin

comps.append(place("Brew:NPN","Q1","BC547B", 93.98, 85.09,
                   fp="Package_TO_SOT_THT:TO-92_Inline",
                   pins_l=NPN_L, pins_r=NPN_R))
Q1x = cx(93.98); Q1y = cy(85.09, 2)
q1_C  = rpin(Q1x, Q1y, 0, 2, bw=10)         # Collector (101.60, 83.82)
bz1_N = lpin(BZ1x, BZ1y, 1, 2, bw=10)       # BZ1(-)   (116.84, 83.82)
wires.append(wseg(*q1_C, *bz1_N))            # C → BZ1(-) direct wire
pwr_r("GND", *rpin(Q1x,Q1y,1,2,bw=10))      # Emitter → GND

comps.append(place("Brew:R","R8","1k", 81.28, 83.82,
                   fp="Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
                   pins_l=R_L, pins_r=R_R))
R8x = cx(81.28); R8y = cy(83.82, 1)
# Left pin (76.2, 83.82) reached via BUZZER_CTRL route from U1 (see U1 loop)
# Right pin (86.36, 83.82) coincides with Q1(B) — no wire needed

# ── J15-J20  Combined BTN+LED panel connectors (3-pin, one per power level) ──
# Each connector: pin1=GND (shared switch-B + LED-), pin2=BTN_n, pin3=LED_n
# Wired to a panel-mount metal pushbutton with integrated LED ring.
_BTN_LED_CFG = [
    ("J15","BTN_0pct",  "BTN1",    "LED_0"),
    ("J16","BTN_20pct", "BTN2",    "LED_20"),
    ("J17","BTN_40pct", "BTN3",    "LED_40"),
    ("J18","BTN_60pct", "BTN4",    "LED_60"),
    ("J19","BTN_80pct", "BTN5",    "LED_80"),
    ("J20","BTN_100pct","ADC_BTN", "LED_100"),
]
_BTN_LED_Y = [83.82, 111.76, 140.97, 168.91, 196.85, 226.06]
for i, ((ref, val, btn_net, led_net), y_pos) in enumerate(zip(_BTN_LED_CFG, _BTN_LED_Y)):
    Jx = cx(346.71); Jy = cy(y_pos, 3)
    comps.append(place("Brew:Conn_BTN_LED", ref, val, 346.71, y_pos,
                       fp=phoenix(3), pins_l=BTN_LED_L))
    pwr_l("GND",   *lpin(Jx,Jy,0,3), with_flag=(i==0))  # PWR_FLAG on first only
    lbl_l(btn_net, *lpin(Jx,Jy,1,3))
    lbl_l(led_net, *lpin(Jx,Jy,2,3))

# ── J21-J22  Spare LED connectors (2-pin, no button) ─────────────────────────
for ref, val, led_net, y_pos in [
    ("J21","LED_01","LED_P6", 251.46),
    ("J22","LED_02","LED_P7", 267.97),
]:
    Jx = cx(346.71); Jy = cy(y_pos, 2)
    comps.append(place("Brew:Conn_LED_Spare", ref, val, 346.71, y_pos,
                       fp=phoenix(2), pins_l=LED_SPARE_L))
    pwr_l("GND",   *lpin(Jx,Jy,0,2))
    lbl_l(led_net, *lpin(Jx,Jy,1,2))

# ── R9  Button ladder pull-up (+3V3 → ADC_BTN) ───────────────────────────────
R9x = cx(256.54); R9y = cy(256.54, 1)
comps.append(place("Brew:R","R9","10k", 256.54, 256.54,
                   fp="Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
                   pins_l=R_L, pins_r=R_R))
lbl_l("ADC_BTN", *lpin(R9x,R9y,0,1,bw=5))
pwr_r("+3V3",    *rpin(R9x,R9y,0,1,bw=5), stub=P)

# ── R10-R14  Button ladder (BTN1-5 signal → ADC_BTN) ─────────────────────────
# Vertical column — each resistor on its own row, no shared wires.
BTN_LADDER = [("R10","39k","BTN1",193.04),("R11","33k","BTN2",205.74),
              ("R12","22k","BTN3",218.44),("R13","12k","BTN4",231.14),("R14","3k3","BTN5",243.84)]
for i, (ref, val, btn, ry_val) in enumerate(BTN_LADDER):
    rbx = cx(256.54); rby = cy(ry_val, 1)
    comps.append(place("Brew:R", ref, val, 256.54, ry_val,
                       fp="Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
                       pins_l=R_L, pins_r=R_R))
    lbl_l("ADC_BTN", *lpin(rbx,rby,0,1,bw=5))
    lbl_r(btn,       *rpin(rbx,rby,0,1,bw=5))

# ── SW2-SW7  Off-board panel buttons (not on PCB) ────────────────────────────
# Each button sets IDS2 induction cooker power. Pressed = BTN_n shorted to GND.
# SW7 (B6) shorts ADC_BTN directly to GND → ADC reads 0 → 100% power.
_BTN_INFO = [
    ("BTN1",   "IDS2 0% – off"),
    ("BTN2",   "IDS2 20%"),
    ("BTN3",   "IDS2 40%"),
    ("BTN4",   "IDS2 60%"),
    ("BTN5",   "IDS2 80%"),
    ("ADC_BTN","IDS2 100% – full"),
]
_BTN_Y = [90.17, 104.14, 133.35, 161.29, 189.23, 218.44]
for i, ((btn, desc), by_val) in enumerate(zip(_BTN_INFO, _BTN_Y)):
    ref = f"SW{i+2}"
    bx = cx(410.21); by = cy(by_val, 1)
    comps.append(place("Brew:SW_Push", ref, desc, 410.21, by_val,
                       fp="", pins_l=SW_PUSH_L, pins_r=SW_PUSH_R, on_board=False))
    lbl_l(btn,   *lpin(bx, by, 0, 1, bw=10))  # A → BTN_n / ADC_BTN net
    pwr_r("GND", *rpin(bx, by, 0, 1, bw=10))  # B → GND

# ── LD9  Off-board power switch integrated LED (not on PCB) ──────────────────
comps.append(place("Brew:LED_Indicator", "LD9", "Power switch LED", 138.43, 68.58,
                   fp="", pins_l=LED_IND_L, pins_r=LED_IND_R, on_board=False))
LD9x = cx(138.43); LD9y = cy(68.58, 1)
lbl_l("PSW_LED", *lpin(LD9x, LD9y, 0, 1, bw=10))  # A → PSW_LED (always on with power)
pwr_r("GND",     *rpin(LD9x, LD9y, 0, 1, bw=10))  # K → GND

# ── LD1-LD8  Off-board panel indicator LEDs (not on PCB) ─────────────────────
# LEDs indicate current IDS2 power state. Driven active-LOW by PCF8574 via R2-R7/R15/R16.
_LED_INFO = [
    ("LED_0",   "IDS2 0% – off"),
    ("LED_20",  "IDS2 20%"),
    ("LED_40",  "IDS2 40%"),
    ("LED_60",  "IDS2 60%"),
    ("LED_80",  "IDS2 80%"),
    ("LED_100", "IDS2 100% – full"),
    ("LED_P6",  "spare LED P6"),
    ("LED_P7",  "spare LED P7"),
]
_LED_Y = [76.20, 118.11, 147.32, 175.26, 203.20, 232.41, 251.46, 267.97]
for i, (net, desc) in enumerate(_LED_INFO):
    ref = f"LD{i+1}"
    lx = cx(410.21); ly = cy(_LED_Y[i], 1)
    comps.append(place("Brew:LED_Indicator", ref, desc, 410.21, _LED_Y[i],
                       fp="", pins_l=LED_IND_L, pins_r=LED_IND_R, on_board=False))
    lbl_l(net,   *lpin(lx, ly, 0, 1, bw=10))  # A → LED_n net (PCF8574 active-LOW)
    pwr_r("GND", *rpin(lx, ly, 0, 1, bw=10))  # K → GND

# ── assemble & write ───────────────────────────────────────────────────────────

sch = f"""(kicad_sch (version 20250610) (generator "eeschema") (generator_version "10.0")
  (uuid "{ROOT}")
  (paper "A2")
  (title_block
    (title "BrewingStation v3 — ESP32-C6")
    (date "2026-05-25")
    (rev "1.0")
    (comment 1 "Full net-label wiring — 0 ERC violations. Generated by wire_schematic.py")
    (comment 2 "IO0=BUZZER  IO1=ADC_BTN  IO2=CS_BME  IO3=GPIO3_ADC  IO4=CS_MAX  IO5=SPI_CLK  IO6=SPI_MISO  IO7=SPI_MOSI  IO10=GPIO10  IO11=OneWire  IO15=GPIO15  IO18-20=IDS2  IO21=RELAY  IO22/23=I2C")
    (comment 3 "Power: IDS2_5V or PEXT_5V → J4 PSource (middle pin=out) → SW1 → +5V rail")
  )
  (lib_symbols
{lib_syms}
  )
{chr(10).join(comps)}
{chr(10).join(pwr)}
{chr(10).join(wires)}
)
"""

out = os.path.join(_DIR, "brewingstation3.kicad_sch")
with open(out, "w", encoding="utf-8") as f:
    f.write(sch)

# Re-parse output and persist UUID map so KiCad reformatting never breaks preservation
_, _out_syms, _out_pins = load_existing_uuids(out)
with open(_MAP_PATH, "w", encoding="utf-8") as f:
    json.dump({"root": ROOT, "syms": _out_syms, "pins": _out_pins}, f, indent=2)

print(f"Wrote {out}  ({len(comps)} components, {len(pwr)} power syms, {len(wires)} wiring elements)")
