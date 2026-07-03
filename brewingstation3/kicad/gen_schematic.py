#!/usr/bin/env python3
"""
KiCad 10 schematic generator for BrewingStation v3 — ESP32-C6
Run: python gen_schematic.py
Outputs: brewingstation3.kicad_sch  brewingstation3.kicad_pro
"""
import uuid, os

# ── helpers ──────────────────────────────────────────────────────────────────

_uid_counter = 0
def uid():
    global _uid_counter
    _uid_counter += 1
    h = f"{_uid_counter:032x}"
    return f"{h[0:8]}-{h[8:12]}-{h[12:16]}-{h[16:20]}-{h[20:32]}"

P = 2.54   # pin pitch mm
PROJECT_NAME = "brewingstation3"

# Root sheet UUID — referenced by all placed-instance (instances ...) blocks
ROOT_UUID = uid()

def esc(s): return s.replace('"', '\\"')


# ── symbol definition builder ─────────────────────────────────────────────────

def sym_def(lib_sym_name, ref_pfx, default_val,
            pins_left,   # [(pin_name, pin_num, pin_type), ...]
            pins_right,  # [(pin_name, pin_num, pin_type), ...]
            body_w=15.24, extra_props=None):
    """Return an inline KiCad 7 lib_symbols entry."""
    n = max(len(pins_left), len(pins_right))
    bh = (n + 1) * P          # body height
    hw = body_w / 2
    hh = bh / 2
    sname = lib_sym_name.split(":")[-1]

    def pin_line(pname, pnum, ptype, x, y, angle):
        return (
            f'      (pin {ptype} line (at {x:.3f} {y:.3f} {angle}) (length {P:.3f})\n'
            f'        (name "{esc(pname)}" (effects (font (size 1.27 1.27))))\n'
            f'        (number "{esc(str(pnum))}" (effects (font (size 1.27 1.27)))))'
        )

    left_pins  = [pin_line(n,num,t, -(hw+P), hh-P-i*P,   0) for i,(n,num,t) in enumerate(pins_left)]
    right_pins = [pin_line(n,num,t,  (hw+P), hh-P-i*P, 180) for i,(n,num,t) in enumerate(pins_right)]

    extra = ""
    if extra_props:
        for k, v in extra_props.items():
            extra += f'\n      (property "{esc(k)}" "{esc(v)}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))'

    return f"""    (symbol "{lib_sym_name}" (pin_numbers (hide yes)) (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)
      (property "Reference" "{ref_pfx}" (at 0 {hh+P:.3f} 0) (effects (font (size 1.27 1.27))))
      (property "Value" "{default_val}" (at 0 {-(hh+P):.3f} 0) (effects (font (size 1.27 1.27))))
      (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
      (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes))){extra}
      (symbol "{sname}_0_1"
        (rectangle (start {-hw:.3f} {-hh:.3f}) (end {hw:.3f} {hh:.3f})
          (stroke (width 0) (type default)) (fill (type background))))
      (symbol "{sname}_1_1"
{chr(10).join(left_pins)}
{chr(10).join(right_pins)}
      )
    )"""


def power_sym_def(name, sym_shape="vcc"):
    """Return a power symbol definition (+3V3, +5V, or GND)."""
    sname = name.replace(".","")  # keep '+' — unit names may start with it; stripping it yields digit-leading names that KiCad rejects
    if sym_shape == "gnd":
        body = (
            '        (polyline (pts (xy 0 0) (xy 0 -1.27)) (stroke (width 0) (type default)) (fill (type none)))\n'
            '        (polyline (pts (xy 1.27 -1.27) (xy -1.27 -1.27)) (stroke (width 0) (type default)) (fill (type none)))\n'
            '        (polyline (pts (xy 0.762 -1.905) (xy -0.762 -1.905)) (stroke (width 0) (type default)) (fill (type none)))\n'
            '        (polyline (pts (xy 0.254 -2.54) (xy -0.254 -2.54)) (stroke (width 0) (type default)) (fill (type none)))'
        )
        pin_dir = 270
        val_y   = -3.81
    else:  # vcc arrow
        body = (
            '        (polyline (pts (xy 0 0) (xy 0 2.54)) (stroke (width 0) (type default)) (fill (type none)))\n'
            '        (polyline (pts (xy -0.508 2.032) (xy 0 2.54) (xy 0.508 2.032)) (stroke (width 0) (type default)) (fill (type none)))'
        )
        pin_dir = 90
        val_y   = 3.81

    return f"""    (symbol "power:{name}" (power) (pin_numbers (hide yes)) (pin_names (offset 0) (hide yes)) (exclude_from_sim no) (in_bom no) (on_board no)
      (property "Reference" "#PWR" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
      (property "Value" "{name}" (at 0 {val_y:.3f} 0) (effects (font (size 1.27 1.27))))
      (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
      (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
      (symbol "{sname}_0_1"
{body})
      (symbol "{sname}_1_1"
        (pin power_out line (at 0 0 {pin_dir}) (length 0)
          (name "{name}" (effects (font (size 1.27 1.27))))
          (number "1"    (effects (font (size 1.27 1.27))))))
    )"""


# ── placed instance builder ───────────────────────────────────────────────────

_refs = {}
def next_ref(pfx):
    _refs[pfx] = _refs.get(pfx, 0) + 1
    return f"{pfx}{_refs[pfx]}"

_pwr_n = 0
def place_pwr(name, x, y):
    global _pwr_n; _pwr_n += 1
    ref = f"#PWR{_pwr_n:03d}"
    return f"""  (symbol (lib_id "power:{name}") (at {x:.3f} {y:.3f} 0) (unit 1)
    (exclude_from_sim no) (in_bom no) (on_board no) (dnp no) (uuid "{uid()}")
    (property "Reference" "{ref}" (at {x:.3f} {y+1.27:.3f} 0) (effects (font (size 1.27 1.27)) (hide yes)))
    (property "Value" "{name}" (at {x:.3f} {y+1.27:.3f} 0) (effects (font (size 1.27 1.27))))
    (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
    (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
    (pin "1" (uuid "{uid()}"))
    (instances (project "{PROJECT_NAME}" (path "/{ROOT_UUID}" (reference "{ref}") (unit 1)))))"""


def place(lib_id, ref, value, x, y, rot=0, props=None, fp="", pins=None):
    """Return a placed symbol instance. Pass pins=(LEFT+RIGHT list) to emit all pin UUIDs."""
    extra = ""
    if props:
        for k,v in props.items():
            extra += f'\n    (property "{esc(k)}" "{esc(v)}" (at {x:.3f} {y+3.81:.3f} 0) (effects (font (size 1.27 1.27)) (hide yes)))'
    if pins:
        pin_entries = "\n".join(f'    (pin "{esc(str(pnum))}" (uuid "{uid()}"))' for _, pnum, _ in pins)
    else:
        pin_entries = f'    (pin "1" (uuid "{uid()}"))'
    instances_block = f'    (instances (project "{PROJECT_NAME}" (path "/{ROOT_UUID}" (reference "{ref}") (unit 1))))'
    return f"""  (symbol (lib_id "{lib_id}") (at {x:.3f} {y:.3f} {rot}) (unit 1)
    (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no) (uuid "{uid()}")
    (property "Reference" "{ref}" (at {x+2:.3f} {y-3:.3f} 0) (effects (font (size 1.27 1.27))))
    (property "Value" "{value}" (at {x-2:.3f} {y+3:.3f} 0) (effects (font (size 1.27 1.27))))
    (property "Footprint" "{fp}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
    (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes))){extra}
{pin_entries}
{instances_block})"""


# ── wire / label builders ─────────────────────────────────────────────────────

def wire(x1,y1,x2,y2):
    return f'  (wire (pts (xy {x1:.3f} {y1:.3f}) (xy {x2:.3f} {y2:.3f})) (stroke (width 0) (type default)) (uuid "{uid()}"))'

def net_label(name, x, y, angle=0):
    justify = "right" if angle == 180 else "left"
    return f"""  (label "{esc(name)}" (at {x:.3f} {y:.3f} {angle})
    (effects (font (size 1.27 1.27)) (justify {justify} bottom))
    (uuid "{uid()}"))"""

def no_connect(x, y):
    return f'  (no_connect (at {x:.3f} {y:.3f}) (uuid "{uid()}"))'


# ── symbol library definitions ────────────────────────────────────────────────

BI = "bidirectional"; INP = "input"; OUT = "output"
PWR_IN = "power_in"; PWR_OUT = "power_out"; PASS = "passive"

# Pin numbers match PCM_Espressif:ESP32-C6-DevKitC-1 footprint pads 1-32.
# Left rail: pad 1 (top) → pad 16 (bottom). Right rail: pad 32 (top) → pad 17 (bottom).
# NOTE: GPIO14 is NOT exposed on DevKitC-1 — GPIO_EXT reassigned to IO3 (pad 13).
ESP32_LEFT = [
    ("+3V3",    "1",  PWR_OUT),  # 3.3V output from DevKit LDO
    ("EN",      "2",  INP),
    ("IO4",     "3",  BI),
    ("IO5",     "4",  BI),
    ("IO6",     "5",  BI),
    ("IO7",     "6",  BI),
    ("IO0",     "7",  BI),
    ("IO1",     "8",  BI),
    ("IO8",     "9",  BI),
    ("IO10",    "10", BI),
    ("IO11",    "11", BI),
    ("IO2",     "12", BI),
    ("IO3",     "13", BI),       # GPIO_EXT (IO14 not on DevKitC-1, update firmware)
    ("+5V",     "14", PWR_IN),   # DevKit VIN — powered from +5V rail
    ("GND",     "15", PWR_IN),
    ("NC",      "16", PASS),
]
ESP32_RIGHT = [
    ("GND",      "32", PWR_IN),  # top of right rail
    ("IO16~TX",  "31", OUT),
    ("IO17~RX",  "30", INP),
    ("IO15",     "29", BI),
    ("IO23",     "28", BI),
    ("IO22",     "27", BI),
    ("IO21",     "26", BI),
    ("IO20",     "25", BI),      # IDS2_BLU_LV
    ("IO19",     "24", BI),      # IDS2_YEL_LV
    ("IO18",     "23", BI),      # IDS2_WHITE_LV
    ("IO9",      "22", BI),      # BOOT strap — NC
    ("GND",      "21", PWR_IN),
    ("IO13",     "20", BI),      # USB_D+/NC
    ("IO12",     "19", BI),      # USB_D-/NC
    ("GND",      "18", PWR_IN),
    ("NC",       "17", PASS),
]

PCF8574_LEFT = [
    ("VSS",  "1", PWR_IN),
    ("VDD",  "2", PWR_IN),
    ("A0",   "3", INP),
    ("A1",   "4", INP),
    ("A2",   "5", INP),
    ("~INT", "6", PASS),   # open-drain output
    ("SCL",  "7", INP),
    ("SDA",  "8", BI),
]
PCF8574_RIGHT = [
    ("P0",  "9",  OUT),
    ("P1",  "10", OUT),
    ("P2",  "11", OUT),
    ("P3",  "12", OUT),
    ("P4",  "13", OUT),
    ("P5",  "14", OUT),
    ("P6",  "15", OUT),
    ("P7",  "16", OUT),
]

# BSS138-based 4-channel bidirectional level shifter module
# Actual pinout — power/GND are in the MIDDLE of each 6-pin row:
#   Top row (HV):  HV1  HV2  HV(5V)  GND  HV3  HV4
#   Bottom row (LV): LV1  LV2  LV(3V3)  GND  LV3  LV4
LS_LEFT = [
    ("LV1", "1",  BI),       # 3.3V channel 1
    ("LV2", "2",  BI),       # 3.3V channel 2
    ("LV",  "3",  PWR_IN),   # 3.3V supply
    ("GND", "4",  PWR_IN),
    ("LV3", "5",  BI),       # 3.3V channel 3
    ("LV4", "6",  BI),       # 3.3V channel 4
]
LS_RIGHT = [
    ("HV1", "7",  BI),       # 5V channel 1
    ("HV2", "8",  BI),       # 5V channel 2
    ("HV",  "9",  PWR_IN),   # 5V supply
    ("GND", "10", PWR_IN),
    ("HV3", "11", BI),       # 5V channel 3
    ("HV4", "12", BI),       # 5V channel 4
]

DS18_LEFT  = [("GND","1",PWR_IN),("DQ","2",BI),("VDD","3",PWR_IN)]
DS18_RIGHT = []

MAX_LEFT   = [("VCC","1",PWR_IN),("GND","2",PWR_IN),("SDI","3",INP),("SDO","4",OUT),("SCK","5",INP),("CS","6",INP),("DRDY","7",OUT)]
MAX_RIGHT  = []

BME_LEFT   = [("VCC","1",PWR_IN),("GND","2",PWR_IN),("SCK","3",INP),("SDI","4",INP),("SDO","5",OUT),("CSB","6",INP)]
BME_RIGHT  = []

OLED_LEFT  = [("GND","1",PWR_IN),("VCC","2",PWR_IN),("SCL","3",INP),("SDA","4",BI)]
OLED_RIGHT = []

IDS2_LEFT  = [("VCC","1",PWR_IN),("GND","2",PWR_IN),("WHITE","3",BI),("YELLOW","4",OUT),("BLUE","5",INP)]
IDS2_RIGHT = []

RELAY_LEFT = [("VCC","1",PWR_IN),("GND","2",PWR_IN),("IN","3",INP),("NO","4",PASS),("COM","5",PASS)]
RELAY_RIGHT= []

LED_LEFT   = [("K","1",PASS),("A","2",PASS)]
LED_RIGHT  = []

R_LEFT     = [("~","1",PASS)]
R_RIGHT    = [("~","2",PASS)]

BTN_LEFT   = [("A","1",PASS),("B","2",PASS)]
BTN_RIGHT  = []

GPIOEXT_LEFT = [("SIG","1",BI),("GND","2",PWR_IN)]
GPIOEXT_RIGHT = []

PEXT_LEFT  = [("VCC","1",PWR_IN),("GND","2",PWR_IN)]
PEXT_RIGHT = []

PJUMP_LEFT = [("IDS2","1",PASS),("5V","2",PASS),("PEXT","3",PASS)]
PJUMP_RIGHT = []

PSW_LEFT   = [("A","1",PASS),("B","2",PASS)]
PSW_RIGHT  = []

JBTN_LEFT  = [("+3V3","1",PASS),("ADC_BTN","2",PASS),("GND","3",PASS)]
JBTN_RIGHT = []

JLED_LEFT  = [("GND","1",PASS),("LED_0","2",PASS),("LED_20","3",PASS),
               ("LED_40","4",PASS),("LED_60","5",PASS),("LED_80","6",PASS),("LED_100","7",PASS)]
JLED_RIGHT = []

# GX12 4-pin panel-mount circular connector (used for external sensors)
# Pin mapping: 1=GND, 2=+3V3, 3=Data, 4=NC
GX12_LEFT  = [("GND","1",PWR_IN),("+3V3","2",PWR_IN),("DQ","3",BI),("NC","4",PASS)]
GX12_RIGHT = []


# ── build lib_symbols section ─────────────────────────────────────────────────

lib_syms = "\n".join([
    power_sym_def("GND",   "gnd"),
    power_sym_def("+3V3"),
    power_sym_def("+5V"),
    sym_def("Brew:ESP32-C6-WROOM-1U","U","ESP32-C6-WROOM-1U",  ESP32_LEFT, ESP32_RIGHT, body_w=22),
    sym_def("Brew:PCF8574",          "U","PCF8574",             PCF8574_LEFT, PCF8574_RIGHT),
    sym_def("Brew:LevelShifter_4CH", "U","LevelShifter_4CH",   LS_LEFT, LS_RIGHT),
    sym_def("Brew:MAX31865_Module",  "U","MAX31865",            MAX_LEFT, MAX_RIGHT, body_w=10),
    sym_def("Brew:BME280_Module",    "U","BME280",              BME_LEFT, BME_RIGHT, body_w=10),
    sym_def("Brew:SSD1306",          "DS","SSD1306_OLED",       OLED_LEFT, OLED_RIGHT, body_w=10),
    sym_def("Brew:GGM_IDS2",        "J","GGM_IDS2",            IDS2_LEFT, IDS2_RIGHT, body_w=10),
    sym_def("Brew:Relay_Module",    "K","Relay_5V",             RELAY_LEFT, RELAY_RIGHT, body_w=10),
    sym_def("Brew:R",               "R","R",                   R_LEFT, R_RIGHT, body_w=5),
    sym_def("Brew:Conn_GPIO_EXT",   "J","GPIO_EXT",            GPIOEXT_LEFT, GPIOEXT_RIGHT, body_w=10),
    sym_def("Brew:Conn_PExt",      "J","PExt_5V",             PEXT_LEFT,   PEXT_RIGHT,   body_w=10),
    sym_def("Brew:PJump",          "J","PJump",               PJUMP_LEFT,  PJUMP_RIGHT,  body_w=10),
    sym_def("Brew:Conn_PSw",       "J","PSw_Conn",            PSW_LEFT,    PSW_RIGHT,    body_w=10),
    sym_def("Brew:Conn_BTN",       "J","BTN_Panel",           JBTN_LEFT,   JBTN_RIGHT,   body_w=10),
    sym_def("Brew:Conn_LED",       "J","LED_Panel",           JLED_LEFT,   JLED_RIGHT,   body_w=10),
    sym_def("Brew:Conn_GX12_4P",   "J","GX12_4P",            GX12_LEFT,   GX12_RIGHT,   body_w=10),
])

# ── component placement & wiring ──────────────────────────────────────────────
# All coordinates in mm.  Net labels (NL) are the primary connection mechanism
# so routing between distant blocks can be done in KiCad GUI if desired.

# Helper: pin X/Y given symbol centre, side, pin index (0-based), body params
def lpin(cx, cy, idx, n_pins, body_w=15.24):
    hw = body_w / 2
    bh = (max(n_pins,1) + 1) * P
    hh = bh / 2
    return (cx - hw - P, cy + hh - P - idx * P)

def rpin(cx, cy, idx, n_pins, body_w=15.24):
    hw = body_w / 2
    bh = (max(n_pins,1) + 1) * P
    hh = bh / 2
    return (cx + hw + P, cy + hh - P - idx * P)

# Signal nets used (for documentation)
NETS = {
    # ESP32 IO → net name
    "IO0":  "ONE_WIRE",
    "IO1":  "ADC_BTN",
    "IO2":  "CS_BME",
    "IO3":  "NC",
    "IO4":  "SPI_MOSI",
    "IO5":  "SPI_MISO",
    "IO6":  "SPI_CLK",
    "IO7":  "CS_MAX",
    "IO8":  "NC",
    "IO9":  "NC",            # BOOT button strapping pin — avoid driving as output
    "IO10": "I2C_SDA",
    "IO11": "I2C_SCL",
    "IO12": "NC",            # USB D- — do not use (USB-CDC active)
    "IO13": "NC",            # USB D+ — do not use (USB-CDC active)
    "IO21": "RELAY_CTRL",
    "IO15": "NC",
    "IO18": "IDS2_WHITE_LV",
    "IO19": "IDS2_YEL_LV",
    "IO20": "IDS2_BLU_LV",
    "IO22": "NC",
    "IO23": "NC",
    # Power nets
    "IDS2_5V":   "5V tap from IDS2 connector VCC pin",
    "PEXT_5V":   "5V from external USB charger connector",
    "PJUMP_OUT": "PJump center pin → PSw → +5V rail",
}

instances = []
wires_out = []
labels_out = []
nc_out = []
pwr_out = []

# ── U1  ESP32-C6-WROOM-1U  ───────────────────────────────────────────────────
U1x, U1y = 85.0, 140.0
bw_esp = 22
instances.append(place("Brew:ESP32-C6-WROOM-1U","U1","ESP32-C6-WROOM-1U", U1x, U1y, fp="PCM_Espressif:ESP32-C6-DevKitC-1", pins=ESP32_LEFT+ESP32_RIGHT))

n_esp_l = len(ESP32_LEFT)
n_esp_r = len(ESP32_RIGHT)

# Left pins
LP = [lpin(U1x,U1y,i,n_esp_l,bw_esp) for i in range(n_esp_l)]
# Right pins
RP = [rpin(U1x,U1y,i,n_esp_r,bw_esp) for i in range(n_esp_r)]

# Left-side signal assignments (ordered to match ESP32_LEFT / pads 1-16)
LEFT_SIGNALS = ["+3V3","EN","IO4","IO5","IO6","IO7","IO0","IO1","IO8","IO10","IO11","IO2","IO3","+5V","GND","NC"]
LEFT_NETS =    [None,  None,"SPI_MOSI","SPI_MISO","SPI_CLK","CS_MAX","ONE_WIRE","ADC_BTN",None,"I2C_SDA","I2C_SCL","CS_BME","GPIO_EXT_LV",None,None,None]

for i, (sig, net) in enumerate(zip(LEFT_SIGNALS, LEFT_NETS)):
    px, py = LP[i]
    if sig == "GND":
        pwr_out.append(place_pwr("GND", px-P, py))
        wires_out.append(wire(px-P, py, px, py))
    elif sig == "+3V3":
        pwr_out.append(place_pwr("+3V3", px-P, py))
        wires_out.append(wire(px-P, py, px, py))
    elif sig == "+5V":
        pwr_out.append(place_pwr("+5V", px-P, py))
        wires_out.append(wire(px-P, py, px, py))
    elif sig == "NC":
        nc_out.append(no_connect(px, py))
    elif sig == "EN":
        pwr_out.append(place_pwr("+3V3", px-P*2, py))
        wires_out.append(wire(px-P*2, py, px, py))
    elif net:
        labels_out.append(net_label(net, px-1.27, py, 180))
        wires_out.append(wire(px, py, px-1.27, py))
    else:
        nc_out.append(no_connect(px, py))

# Right-side signal assignments (ordered to match ESP32_RIGHT / pads 32→17)
RIGHT_SIGNALS = ["GND","IO16~TX","IO17~RX","IO15","IO23","IO22","IO21","IO20","IO19","IO18","IO9","GND","IO13","IO12","GND","NC"]
RIGHT_NETS =    [None, "UART_TX","UART_RX",None,  None,  None, "RELAY_CTRL","IDS2_BLU_LV","IDS2_YEL_LV","IDS2_WHITE_LV",None,None,None,None,None,None]

for i, (sig, net) in enumerate(zip(RIGHT_SIGNALS, RIGHT_NETS)):
    px, py = RP[i]
    if sig == "GND":
        pwr_out.append(place_pwr("GND", px+P, py))
        wires_out.append(wire(px+P, py, px, py))
    elif sig == "NC":
        nc_out.append(no_connect(px, py))
    elif net:
        labels_out.append(net_label(net, px+1.27, py))
        wires_out.append(wire(px, py, px+1.27, py))
    else:
        nc_out.append(no_connect(px, py))


# ── U2  PCF8574  ─────────────────────────────────────────────────────────────
U2x, U2y = 195.0, 80.0
instances.append(place("Brew:PCF8574","U2","PCF8574", U2x, U2y, fp="Package_DIP:DIP-16_W7.62mm", pins=PCF8574_LEFT+PCF8574_RIGHT))

n2l = len(PCF8574_LEFT); n2r = len(PCF8574_RIGHT)
LP2 = [lpin(U2x,U2y,i,n2l) for i in range(n2l)]
RP2 = [rpin(U2x,U2y,i,n2r) for i in range(n2r)]

# VSS GND
pwr_out.append(place_pwr("GND",  LP2[0][0]-P, LP2[0][1]))
wires_out.append(wire(LP2[0][0]-P, LP2[0][1], LP2[0][0], LP2[0][1]))
# VDD +3V3
pwr_out.append(place_pwr("+3V3", LP2[1][0]-P, LP2[1][1]))
wires_out.append(wire(LP2[1][0]-P, LP2[1][1], LP2[1][0], LP2[1][1]))
# A0,A1,A2 → GND (address 0x20)
for i in [2,3,4]:
    pwr_out.append(place_pwr("GND", LP2[i][0]-P, LP2[i][1]))
    wires_out.append(wire(LP2[i][0]-P, LP2[i][1], LP2[i][0], LP2[i][1]))
# ~INT (left pin 5) — no connect
nc_out.append(no_connect(LP2[5][0], LP2[5][1]))
# SCL, SDA net labels (left pins 6,7)
labels_out.append(net_label("I2C_SCL", LP2[6][0]-1.27, LP2[6][1], 180))
wires_out.append(wire(LP2[6][0]-1.27, LP2[6][1], LP2[6][0], LP2[6][1]))
labels_out.append(net_label("I2C_SDA", LP2[7][0]-1.27, LP2[7][1], 180))
wires_out.append(wire(LP2[7][0]-1.27, LP2[7][1], LP2[7][0], LP2[7][1]))

# P0-P5 → intermediate PCF_PX nets (resistors sit between PCF and J6 LED_XPCT nets)
LED_NETS = ["PCF_P0","PCF_P1","PCF_P2","PCF_P3","PCF_P4","PCF_P5"]
for i, lnet in enumerate(LED_NETS):
    labels_out.append(net_label(lnet, RP2[i][0]+1.27, RP2[i][1]))
    wires_out.append(wire(RP2[i][0], RP2[i][1], RP2[i][0]+1.27, RP2[i][1]))
# P6, P7 NC
nc_out.append(no_connect(RP2[6][0], RP2[6][1]))
nc_out.append(no_connect(RP2[7][0], RP2[7][1]))


# ── U3  BSS138 4-channel bidirectional level shifter module  ─────────────────
U3x, U3y = 195.0, 175.0
instances.append(place("Brew:LevelShifter_4CH","U3","LevelShifter_4CH", U3x, U3y,
                       fp="Connector_PinHeader_2.54mm:PinHeader_2x06_P2.54mm_Vertical",
                       pins=LS_LEFT+LS_RIGHT))

n3l = len(LS_LEFT); n3r = len(LS_RIGHT)
LP3 = [lpin(U3x,U3y,i,n3l) for i in range(n3l)]
RP3 = [rpin(U3x,U3y,i,n3r) for i in range(n3r)]

# LV side (left): LV1(0) LV2(1) LV-supply(2) GND(3) LV3(4) LV4(5)
# Channels 1-4 map: LV1=IDS2_WHITE_LV, LV2=IDS2_YEL_LV, LV3=IDS2_BLU_LV, LV4=GPIO_EXT_LV
labels_out.append(net_label("IDS2_WHITE_LV", LP3[0][0]-1.27, LP3[0][1], 180))
wires_out.append(wire(LP3[0][0]-1.27, LP3[0][1], LP3[0][0], LP3[0][1]))
labels_out.append(net_label("IDS2_YEL_LV",  LP3[1][0]-1.27, LP3[1][1], 180))
wires_out.append(wire(LP3[1][0]-1.27, LP3[1][1], LP3[1][0], LP3[1][1]))
pwr_out.append(place_pwr("+3V3", LP3[2][0]-P, LP3[2][1]))   # LV supply
wires_out.append(wire(LP3[2][0]-P, LP3[2][1], LP3[2][0], LP3[2][1]))
pwr_out.append(place_pwr("GND",  LP3[3][0]-P, LP3[3][1]))
wires_out.append(wire(LP3[3][0]-P, LP3[3][1], LP3[3][0], LP3[3][1]))
labels_out.append(net_label("IDS2_BLU_LV",  LP3[4][0]-1.27, LP3[4][1], 180))
wires_out.append(wire(LP3[4][0]-1.27, LP3[4][1], LP3[4][0], LP3[4][1]))
labels_out.append(net_label("GPIO_EXT_LV",  LP3[5][0]-1.27, LP3[5][1], 180))
wires_out.append(wire(LP3[5][0]-1.27, LP3[5][1], LP3[5][0], LP3[5][1]))

# HV side (right): HV1(0) HV2(1) HV-supply(2) GND(3) HV3(4) HV4(5)
labels_out.append(net_label("IDS2_WHITE",  RP3[0][0]+1.27, RP3[0][1]))
wires_out.append(wire(RP3[0][0], RP3[0][1], RP3[0][0]+1.27, RP3[0][1]))
labels_out.append(net_label("IDS2_YELLOW", RP3[1][0]+1.27, RP3[1][1]))
wires_out.append(wire(RP3[1][0], RP3[1][1], RP3[1][0]+1.27, RP3[1][1]))
pwr_out.append(place_pwr("+5V", RP3[2][0]+P, RP3[2][1]))    # HV supply
wires_out.append(wire(RP3[2][0]+P, RP3[2][1], RP3[2][0], RP3[2][1]))
pwr_out.append(place_pwr("GND", RP3[3][0]+P, RP3[3][1]))
wires_out.append(wire(RP3[3][0]+P, RP3[3][1], RP3[3][0], RP3[3][1]))
labels_out.append(net_label("IDS2_BLUE",   RP3[4][0]+1.27, RP3[4][1]))
wires_out.append(wire(RP3[4][0], RP3[4][1], RP3[4][0]+1.27, RP3[4][1]))
labels_out.append(net_label("GPIO_EXT",    RP3[5][0]+1.27, RP3[5][1]))
wires_out.append(wire(RP3[5][0], RP3[5][1], RP3[5][0]+1.27, RP3[5][1]))


# ── J7,J8,J9  DS18B20 sensors — GX12 4-pin panel-mount connectors  ──────────
# Sensors are external, wired via GX12 4-pin aviation connectors.
# GX12 pin map: 1=GND, 2=+3V3, 3=DQ (OneWire), 4=NC
for _ds_idx, (_DSx, _DSy, _DSref) in enumerate([
        (305.0, 35.0, "J7"),
        (305.0, 65.0, "J8"),
        (305.0, 95.0, "J9"),
]):
    instances.append(place("Brew:Conn_GX12_4P", _DSref, "DS18B20_Sensor", _DSx, _DSy,
                           fp="Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
                           pins=GX12_LEFT+GX12_RIGHT))
    _LP = [lpin(_DSx,_DSy,i,len(GX12_LEFT),10) for i in range(len(GX12_LEFT))]
    pwr_out.append(place_pwr("GND",  _LP[0][0]-P, _LP[0][1]))
    wires_out.append(wire(_LP[0][0]-P, _LP[0][1], _LP[0][0], _LP[0][1]))
    pwr_out.append(place_pwr("+3V3", _LP[1][0]-P, _LP[1][1]))
    wires_out.append(wire(_LP[1][0]-P, _LP[1][1], _LP[1][0], _LP[1][1]))
    labels_out.append(net_label("ONE_WIRE", _LP[2][0]-1.27, _LP[2][1], 180))
    wires_out.append(wire(_LP[2][0]-1.27, _LP[2][1], _LP[2][0], _LP[2][1]))
    nc_out.append(no_connect(_LP[3][0], _LP[3][1]))
# 4.7kΩ pull-up on ONE_WIRE bus (on PCB, between +3V3 and DQ line)
R1x, R1y = 305.0+40, 35.0-5
instances.append(place("Brew:R", next_ref("R"), "4k7", R1x, R1y,
                        fp="Resistor_SMD:R_0402_1005Metric", pins=R_LEFT+R_RIGHT))
_pu_lp = lpin(R1x, R1y, 0, 1, 5)
_pu_rp = rpin(R1x, R1y, 0, 1, 5)
labels_out.append(net_label("ONE_WIRE", _pu_lp[0]-1.27, _pu_lp[1], 180))
wires_out.append(wire(_pu_lp[0], _pu_lp[1], _pu_lp[0]-1.27, _pu_lp[1]))
pwr_out.append(place_pwr("+3V3", _pu_rp[0]+P, _pu_rp[1]))
wires_out.append(wire(_pu_rp[0], _pu_rp[1], _pu_rp[0]+P, _pu_rp[1]))


# ── U5  MAX31865  ─────────────────────────────────────────────────────────────
U5x, U5y = 305.0, 125.0
instances.append(place("Brew:MAX31865_Module","U5","MAX31865", U5x, U5y,
                       fp="Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Vertical",
                       pins=MAX_LEFT+MAX_RIGHT))
LP5 = [lpin(U5x,U5y,i,len(MAX_LEFT),10) for i in range(len(MAX_LEFT))]
pwr_out.append(place_pwr("+3V3", LP5[0][0]-P, LP5[0][1]))
wires_out.append(wire(LP5[0][0]-P, LP5[0][1], LP5[0][0], LP5[0][1]))
pwr_out.append(place_pwr("GND",  LP5[1][0]-P, LP5[1][1]))
wires_out.append(wire(LP5[1][0]-P, LP5[1][1], LP5[1][0], LP5[1][1]))
for i,(sig,net) in enumerate([("SDI","SPI_MOSI"),("SDO","SPI_MISO"),("SCK","SPI_CLK"),("CS","CS_MAX"),("DRDY","NC")]):
    if net == "NC":
        nc_out.append(no_connect(LP5[2+i][0], LP5[2+i][1]))
    else:
        labels_out.append(net_label(net, LP5[2+i][0]-1.27, LP5[2+i][1], 180))
        wires_out.append(wire(LP5[2+i][0]-1.27, LP5[2+i][1], LP5[2+i][0], LP5[2+i][1]))


# ── U6  BME280  ──────────────────────────────────────────────────────────────
U6x, U6y = 305.0, 168.0
instances.append(place("Brew:BME280_Module","U6","BME280", U6x, U6y,
                       fp="Connector_PinHeader_2.54mm:PinHeader_1x06_P2.54mm_Vertical",
                       pins=BME_LEFT+BME_RIGHT))
LP6 = [lpin(U6x,U6y,i,len(BME_LEFT),10) for i in range(len(BME_LEFT))]
pwr_out.append(place_pwr("+3V3", LP6[0][0]-P, LP6[0][1]))
wires_out.append(wire(LP6[0][0]-P, LP6[0][1], LP6[0][0], LP6[0][1]))
pwr_out.append(place_pwr("GND",  LP6[1][0]-P, LP6[1][1]))
wires_out.append(wire(LP6[1][0]-P, LP6[1][1], LP6[1][0], LP6[1][1]))
for i,(sig,net) in enumerate([("SCK","SPI_CLK"),("SDI","SPI_MOSI"),("SDO","SPI_MISO"),("CSB","CS_BME")]):
    labels_out.append(net_label(net, LP6[2+i][0]-1.27, LP6[2+i][1], 180))
    wires_out.append(wire(LP6[2+i][0]-1.27, LP6[2+i][1], LP6[2+i][0], LP6[2+i][1]))


# ── DS1, DS2  OLEDs  ──────────────────────────────────────────────────────────
for di, (DSx, DSy, ref) in enumerate([(305.0, 212.0, "DS1"), (305.0, 235.0, "DS2")]):
    addr = "0x3C" if di==0 else "0x3D"
    instances.append(place("Brew:SSD1306",ref,f"SSD1306_{addr}", DSx, DSy,
                           fp="Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
                           pins=OLED_LEFT+OLED_RIGHT))
    LP_d = [lpin(DSx,DSy,i,len(OLED_LEFT),10) for i in range(len(OLED_LEFT))]
    pwr_out.append(place_pwr("GND",  LP_d[0][0]-P, LP_d[0][1]))
    wires_out.append(wire(LP_d[0][0]-P, LP_d[0][1], LP_d[0][0], LP_d[0][1]))
    pwr_out.append(place_pwr("+3V3", LP_d[1][0]-P, LP_d[1][1]))
    wires_out.append(wire(LP_d[1][0]-P, LP_d[1][1], LP_d[1][0], LP_d[1][1]))
    labels_out.append(net_label("I2C_SCL", LP_d[2][0]-1.27, LP_d[2][1], 180))
    wires_out.append(wire(LP_d[2][0]-1.27, LP_d[2][1], LP_d[2][0], LP_d[2][1]))
    labels_out.append(net_label("I2C_SDA", LP_d[3][0]-1.27, LP_d[3][1], 180))
    wires_out.append(wire(LP_d[3][0]-1.27, LP_d[3][1], LP_d[3][0], LP_d[3][1]))


# ── J1  GGM IDS2  (5-pin: VCC, GND, WHITE, YELLOW, BLUE)  ───────────────────
J1x, J1y = 305.0, 270.0
instances.append(place("Brew:GGM_IDS2","J1","GGM_IDS2", J1x, J1y,
                       fp="Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical",
                       pins=IDS2_LEFT+IDS2_RIGHT))
LP_j = [lpin(J1x,J1y,i,len(IDS2_LEFT),10) for i in range(len(IDS2_LEFT))]
# VCC tap → IDS2_5V net (feeds PJump)
labels_out.append(net_label("IDS2_5V", LP_j[0][0]-1.27, LP_j[0][1], 180))
wires_out.append(wire(LP_j[0][0]-1.27, LP_j[0][1], LP_j[0][0], LP_j[0][1]))
# GND
pwr_out.append(place_pwr("GND", LP_j[1][0]-P, LP_j[1][1]))
wires_out.append(wire(LP_j[1][0]-P, LP_j[1][1], LP_j[1][0], LP_j[1][1]))
# Signal wires (indices 2,3,4)
for i, net in enumerate(["IDS2_WHITE","IDS2_YELLOW","IDS2_BLUE"]):
    labels_out.append(net_label(net, LP_j[2+i][0]-1.27, LP_j[2+i][1], 180))
    wires_out.append(wire(LP_j[2+i][0]-1.27, LP_j[2+i][1], LP_j[2+i][0], LP_j[2+i][1]))


# ── K1  Relay module  ─────────────────────────────────────────────────────────
K1x, K1y = 195.0, 255.0
instances.append(place("Brew:Relay_Module","K1","G5V-1-5V", K1x, K1y,
                       fp="Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical",
                       pins=RELAY_LEFT+RELAY_RIGHT))
LP_k = [lpin(K1x,K1y,i,len(RELAY_LEFT),10) for i in range(len(RELAY_LEFT))]
pwr_out.append(place_pwr("+5V", LP_k[0][0]-P, LP_k[0][1]))
wires_out.append(wire(LP_k[0][0]-P, LP_k[0][1], LP_k[0][0], LP_k[0][1]))
pwr_out.append(place_pwr("GND", LP_k[1][0]-P, LP_k[1][1]))
wires_out.append(wire(LP_k[1][0]-P, LP_k[1][1], LP_k[1][0], LP_k[1][1]))
labels_out.append(net_label("RELAY_CTRL", LP_k[2][0]-1.27, LP_k[2][1], 180))
wires_out.append(wire(LP_k[2][0]-1.27, LP_k[2][1], LP_k[2][0], LP_k[2][1]))
# NO and COM are output terminals — no connect for now (user wires load)
nc_out.append(no_connect(LP_k[3][0], LP_k[3][1]))
nc_out.append(no_connect(LP_k[4][0], LP_k[4][1]))


# ── J2  GPIO_EXT  ─────────────────────────────────────────────────────────────
J2x, J2y = 305.0, 310.0
instances.append(place("Brew:Conn_GPIO_EXT","J2","GPIO_EXT_Conn", J2x, J2y,
                       fp="Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical",
                       pins=GPIOEXT_LEFT+GPIOEXT_RIGHT))
LP_j2 = [lpin(J2x,J2y,i,len(GPIOEXT_LEFT),10) for i in range(len(GPIOEXT_LEFT))]
labels_out.append(net_label("GPIO_EXT", LP_j2[0][0]-1.27, LP_j2[0][1], 180))
wires_out.append(wire(LP_j2[0][0]-1.27, LP_j2[0][1], LP_j2[0][0], LP_j2[0][1]))
pwr_out.append(place_pwr("GND", LP_j2[1][0]-P, LP_j2[1][1]))
wires_out.append(wire(LP_j2[1][0]-P, LP_j2[1][1], LP_j2[1][0], LP_j2[1][1]))


# ── J3  PExt — external 5V power input (PCB screw terminal / Leiterplattenklemme)  ──────
J3x, J3y = 400.0, 245.0
instances.append(place("Brew:Conn_PExt","J3","PExt_Connector", J3x, J3y,
                       fp="TerminalBlock_Phoenix:PhoenixContact_MKDS-1,5-2_1x02_P5.00mm_Horizontal",
                       pins=PEXT_LEFT+PEXT_RIGHT))
LP_j3 = [lpin(J3x,J3y,i,len(PEXT_LEFT),10) for i in range(len(PEXT_LEFT))]
# VCC → PEXT_5V net (feeds PJump)
labels_out.append(net_label("PEXT_5V", LP_j3[0][0]-1.27, LP_j3[0][1], 180))
wires_out.append(wire(LP_j3[0][0]-1.27, LP_j3[0][1], LP_j3[0][0], LP_j3[0][1]))
# GND
pwr_out.append(place_pwr("GND", LP_j3[1][0]-P, LP_j3[1][1]))
wires_out.append(wire(LP_j3[1][0]-P, LP_j3[1][1], LP_j3[1][0], LP_j3[1][1]))


# ── J4  PJump — power source selector (IDS2 5V vs PExt 5V)  ─────────────────
# Pin 1=IDS2, Pin 2=output to PSw, Pin 3=PExt
# Jumper cap selects: 1-2 → IDS2 powers board; 2-3 → PExt powers board
J4x, J4y = 400.0, 280.0
instances.append(place("Brew:PJump","J4","PJump", J4x, J4y,
                       fp="Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
                       pins=PJUMP_LEFT+PJUMP_RIGHT))
LP_j4 = [lpin(J4x,J4y,i,len(PJUMP_LEFT),10) for i in range(len(PJUMP_LEFT))]
labels_out.append(net_label("IDS2_5V",   LP_j4[0][0]-1.27, LP_j4[0][1], 180))
wires_out.append(wire(LP_j4[0][0]-1.27, LP_j4[0][1], LP_j4[0][0], LP_j4[0][1]))
labels_out.append(net_label("PJUMP_OUT", LP_j4[1][0]-1.27, LP_j4[1][1], 180))
wires_out.append(wire(LP_j4[1][0]-1.27, LP_j4[1][1], LP_j4[1][0], LP_j4[1][1]))
labels_out.append(net_label("PEXT_5V",   LP_j4[2][0]-1.27, LP_j4[2][1], 180))
wires_out.append(wire(LP_j4[2][0]-1.27, LP_j4[2][1], LP_j4[2][0], LP_j4[2][1]))


# ── SW1  PSw — main power switch (PJump output → +5V rail)  ──────────────────
SW7x, SW7y = 400.0, 315.0
instances.append(place("Brew:Conn_PSw","SW1","PSw_Connector", SW7x, SW7y,
                       fp="Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical",
                       pins=PSW_LEFT+PSW_RIGHT))
LP_sw7 = [lpin(SW7x,SW7y,i,len(PSW_LEFT),10) for i in range(len(PSW_LEFT))]
# A side: from PJump output
labels_out.append(net_label("PJUMP_OUT", LP_sw7[0][0]-1.27, LP_sw7[0][1], 180))
wires_out.append(wire(LP_sw7[0][0]-1.27, LP_sw7[0][1], LP_sw7[0][0], LP_sw7[0][1]))
# B side: to +5V rail
pwr_out.append(place_pwr("+5V", LP_sw7[1][0]-P, LP_sw7[1][1]))
wires_out.append(wire(LP_sw7[1][0]-P, LP_sw7[1][1], LP_sw7[1][0], LP_sw7[1][1]))


# ── LEDs PL0-PL5: 330Ω resistors on PCB, LEDs external via J6  ──────────────
# PCF8574 P-pin → 330Ω (PCB) → J6 LED panel connector → external LED → GND
led_start_x = 155.0
led_y_r = 28.0   # resistor row
led_pitch = 20.0  # 20mm gives ~10mm gap between 10mm-wide resistor bodies

for li in range(6):
    lx = led_start_x + li * led_pitch
    pct = [0, 20, 40, 60, 80, 100][li]
    lnet_pcf = f"PCF_P{li}"      # left pin → PCF8574 P-pin (intermediate net)
    lnet_led = f"LED_{pct}PCT"   # right pin → J6 LED panel connector
    rref = next_ref("R")

    # 330Ω current-limiting resistor: PCF8574 P-pin → R → LED panel connector
    instances.append(place("Brew:R", rref, "330R", lx, led_y_r,
                           fp="Resistor_SMD:R_0402_1005Metric", pins=R_LEFT+R_RIGHT))
    _r_lp = lpin(lx, led_y_r, 0, 1, 5)   # pin 1: PCF side
    _r_rp = rpin(lx, led_y_r, 0, 1, 5)   # pin 2: J6 side
    labels_out.append(net_label(lnet_pcf, _r_lp[0]-1.27, _r_lp[1], 180))
    wires_out.append(wire(_r_lp[0], _r_lp[1], _r_lp[0]-1.27, _r_lp[1]))
    labels_out.append(net_label(lnet_led, _r_rp[0]+1.27, _r_rp[1], 0))
    wires_out.append(wire(_r_rp[0], _r_rp[1], _r_rp[0]+1.27, _r_rp[1]))

# ── J6  LED panel connector (7-pin: GND + 6 LED signals)  ────────────────────
J6x, J6y = led_start_x + 5 * led_pitch + 25.0, 28.0  # right of last resistor
instances.append(place("Brew:Conn_LED","J6","LED_Panel", J6x, J6y,
                       fp="Connector_PinHeader_2.54mm:PinHeader_1x07_P2.54mm_Vertical",
                       pins=JLED_LEFT+JLED_RIGHT))
LP_j6 = [lpin(J6x,J6y,i,len(JLED_LEFT),10) for i in range(len(JLED_LEFT))]
pwr_out.append(place_pwr("GND", LP_j6[0][0]-P, LP_j6[0][1]))
wires_out.append(wire(LP_j6[0][0]-P, LP_j6[0][1], LP_j6[0][0], LP_j6[0][1]))
for i, lnet in enumerate(["LED_0PCT","LED_20PCT","LED_40PCT","LED_60PCT","LED_80PCT","LED_100PCT"]):
    labels_out.append(net_label(lnet, LP_j6[1+i][0]-1.27, LP_j6[1+i][1], 180))
    wires_out.append(wire(LP_j6[1+i][0]-1.27, LP_j6[1+i][1], LP_j6[1+i][0], LP_j6[1+i][1]))


# ── J5  Button panel connector  ───────────────────────────────────────────────
# Buttons and resistor ladder are fully external (on the button panel).
# PCB exposes: +3V3, ADC_BTN signal, GND.
# External panel: 6 buttons (0/20/40/60/80/100%) + resistor voltage divider.
J5x, J5y = 50.0, 250.0
instances.append(place("Brew:Conn_BTN","J5","BTN_Panel", J5x, J5y,
                       fp="Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
                       pins=JBTN_LEFT+JBTN_RIGHT))
LP_j5 = [lpin(J5x,J5y,i,len(JBTN_LEFT),10) for i in range(len(JBTN_LEFT))]
pwr_out.append(place_pwr("+3V3", LP_j5[0][0]-P, LP_j5[0][1]))
wires_out.append(wire(LP_j5[0][0]-P, LP_j5[0][1], LP_j5[0][0], LP_j5[0][1]))
labels_out.append(net_label("ADC_BTN", LP_j5[1][0]-1.27, LP_j5[1][1], 180))
wires_out.append(wire(LP_j5[1][0]-1.27, LP_j5[1][1], LP_j5[1][0], LP_j5[1][1]))
pwr_out.append(place_pwr("GND", LP_j5[2][0]-P, LP_j5[2][1]))
wires_out.append(wire(LP_j5[2][0]-P, LP_j5[2][1], LP_j5[2][0], LP_j5[2][1]))


# ── assemble schematic ────────────────────────────────────────────────────────

def schematic():
    all_blocks = (
        [f"  (lib_symbols\n{lib_syms}\n  )"]
        + pwr_out
        + instances
        + wires_out
        + labels_out
        + nc_out
    )
    body = "\n".join(all_blocks)
    return f"""(kicad_sch (version 20250610) (generator "eeschema") (generator_version "10.0")
  (uuid "{ROOT_UUID}")
  (paper "A2")
  (title_block
    (title "BrewingStation v3 — ESP32-C6")
    (date "2026-05-24")
    (rev "1.0")
    (comment 1 "ESP32-C6-WROOM-1U · PCF8574 LED expander · TXS0104E level shifter · DS18B20 ×3")
    (comment 2 "IO0=DS18B20  IO1=ADC_BTN  IO2=CS_BME  IO3=GPIO_EXT_LV  IO4-7=SPI  IO21=RELAY  IO18-20=IDS2  [IO9=BOOT strap, IO14 not on DevKitC-1]")
    (comment 3 "Power: IDS2-5V-tap or PExt → PJump selector → PSw on/off → +5V rail → DevKit VIN")
  )
{body}
)
"""

# ── kicad_pro ────────────────────────────────────────────────────────────────

def project():
    return """{
  "board": {
    "3dviewports": [],
    "design_settings": {
      "defaults": {"board_outline_line_width": 0.05, "copper_line_width": 0.2, "copper_text_size_h": 1.5, "copper_text_size_v": 1.5, "copper_text_thickness": 0.3},
      "diff_pair_dimensions": [],
      "drc_exclusions": [],
      "meta": {"version": 2},
      "rule_severities": {},
      "rules": {"min_clearance": 0.2, "min_copper_edge_clearance": 0.5, "min_hole_clearance": 0.25, "min_track_width": 0.2, "min_via_annular_width": 0.1, "min_via_diameter": 0.5},
      "track_widths": [],
      "via_dimensions": []
    },
    "ipc2581": {"dist": "", "distpn": "", "internal_id": "", "mfr": "", "mpn": ""},
    "layer_presets": [],
    "viewports": []
  },
  "boards": [],
  "cvpcb": {"equivalence_files": []},
  "erc": {"erc_exclusions": [], "meta": {"version": 0}, "pin_map": [], "rule_severities": {}},
  "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []},
  "meta": {"filename": "brewingstation3.kicad_pro", "version": 1},
  "net_settings": {"classes": [{"bus_width": 12, "clearance": 0.2, "diff_pair_gap": 0.25, "diff_pair_via_gap": 0.25, "diff_pair_width": 0.2, "line_style": 0, "microvia_diameter": 0.3, "microvia_drill": 0.1, "name": "Default", "pcb_color": "rgba(0, 0, 0, 0.000)", "schematic_color": "rgba(0, 0, 0, 0.000)", "track_width": 0.25, "via_diameter": 0.8, "via_drill": 0.4, "wire_width": 6}], "meta": {"version": 3}, "net_colors": null, "netclass_assignments": null, "netclass_patterns": []},
  "pcbnew": {"last_paths": {"gencad": "", "idf": "", "netlist": "", "plot": "", "pos_files": "", "specctra_dsn": "", "step": "", "svg": "", "vrml": ""},
    "page_layout_descr_file": ""},
  "schematic": {
    "annotate_start_num": 0,
    "bom_export_filename": "",
    "bom_fmt_preset": "",
    "bom_fmt_settings": {},
    "bus_alias_definitions": [],
    "connection_grid_size": 50,
    "default_bus_thickness": 12,
    "default_junction_size": 40,
    "default_line_thickness": 6,
    "default_net_thickness": 6,
    "default_text_size": 50,
    "drawing_sheet_file": "",
    "field_names": [],
    "intersheets_ref_own_page": false,
    "intersheets_ref_prefix": "",
    "intersheets_ref_short": false,
    "intersheets_ref_show": false,
    "intersheets_ref_suffix": "",
    "junction_size_choice": 3,
    "label_size_ratio": 0.375,
    "meta": {"version": 1},
    "net_format_name": "",
    "ngspice_settings": null,
    "op_point_scale": 1.0,
    "page_layout_descr_file": "",
    "plot_directory": "",
    "spice_adjust_passive_values": false,
    "spice_current_sheet_as_root": false,
    "spice_external_command": "spice -a %I",
    "spice_model_current_sheet_as_root": true,
    "spice_save_all_currents": false,
    "spice_save_all_dissipations": false,
    "spice_save_all_voltages": false,
    "subpart_first_id": 65,
    "subpart_id_separator": 0
  },
  "sheets": [["ROOT",""]],
  "text_variables": {}
}
"""

# ── write output files ────────────────────────────────────────────────────────

out_dir = os.path.dirname(os.path.abspath(__file__))

sch_path = os.path.join(out_dir, "brewingstation3.kicad_sch")
pro_path = os.path.join(out_dir, "brewingstation3.kicad_pro")

with open(sch_path, "w", encoding="utf-8") as f:
    f.write(schematic())
print(f"Wrote {sch_path}")

with open(pro_path, "w", encoding="utf-8") as f:
    f.write(project())
print(f"Wrote {pro_path}")
