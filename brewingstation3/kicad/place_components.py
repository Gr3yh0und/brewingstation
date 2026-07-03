#!/usr/bin/env python3
"""
BrewingStation v3 — component placement + DS18B20 cluster wiring.
Generates brewingstation3.kicad_sch.
All component centres are snapped to the 2.54 mm KiCad grid automatically.
"""
import uuid, os

_n = 0
def uid():
    global _n; _n += 1
    h = f"{_n:032x}"
    return f"{h[0:8]}-{h[8:12]}-{h[12:16]}-{h[16:20]}-{h[20:32]}"

P = 2.54
PROJECT = "brewingstation3"
ROOT = uid()

def esc(s): return s.replace('"', '\\"')

# ── grid snap helpers ─────────────────────────────────────────────────────────

def snap(v):
    """Snap to nearest 2.54 mm grid point."""
    return round(v / P) * P

def snap_y(y, n):
    """Snap centre_y so all pin endpoints land on the 2.54 mm grid.

    For n even: pin y-offsets are ±P/2, ±3P/2 … → centre must be an ODD
                multiple of P/2.
    For n odd:  pin y-offsets are 0, ±P, ±2P … → centre must be a multiple
                of P (= snap to nearest 2.54 mm).
    """
    if n % 2 == 0:
        k = round(y / (P / 2))
        if k % 2 == 0:
            k += 1          # force odd
        return k * P / 2
    else:
        return snap(y)

def snap_bw(bw):
    """Round box width to the nearest EVEN multiple of P so that
    hw = bw/2 is itself a multiple of P, keeping left/right pin
    endpoints on the grid when the centre x is on the grid.
    """
    k = max(1, round(bw / (2 * P)))
    return k * 2 * P

# ── lib symbol builder ────────────────────────────────────────────────────────

def sym(lib_name, ref, val, pins_l, pins_r, bw=15.24):
    bw  = snap_bw(bw)
    n   = max(len(pins_l), len(pins_r))
    bh  = (n + 1) * P
    hw  = bw / 2
    hh  = bh / 2
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
        (pin power_out line (at 0 0 {pd}) (length 0)
          (name "{name}" (effects (font (size 1.27 1.27))))
          (number "1"    (effects (font (size 1.27 1.27))))))
    )"""

# ── placed instance ────────────────────────────────────────────────────────────

def place(lib_id, ref, val, x, y, fp="", pins_l=None, pins_r=None, rot=0):
    pins = list(pins_l or []) + list(pins_r or [])
    if pins:
        pe = "\n".join(f'    (pin "{esc(str(pn))}" (uuid "{uid()}"))' for _,pn,_ in pins)
    else:
        pe = f'    (pin "1" (uuid "{uid()}"))'
    n = max(len(pins_l or []), len(pins_r or []), 1)
    # Snap centre to KiCad grid
    x = snap(x)
    y = snap_y(y, n)
    hh = (n + 1) * P / 2
    return f"""  (symbol (lib_id "{lib_id}") (at {x:.3f} {y:.3f} {rot}) (unit 1)
    (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no) (uuid "{uid()}")
    (property "Reference" "{ref}" (at {x:.3f} {y - hh - P:.3f} 0) (effects (font (size 1.27 1.27))))
    (property "Value" "{val}" (at {x:.3f} {y + hh + P:.3f} 0) (effects (font (size 1.27 1.27))))
    (property "Footprint" "{fp}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
    (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
{pe}
    (instances (project "{PROJECT}" (path "/{ROOT}" (reference "{ref}") (unit 1)))))"""

# ── wiring helpers ────────────────────────────────────────────────────────────

def wire_seg(x1, y1, x2, y2):
    return (f'  (wire (pts (xy {x1:.3f} {y1:.3f}) (xy {x2:.3f} {y2:.3f}))\n'
            f'    (stroke (width 0) (type default))\n'
            f'    (uuid "{uid()}"))')

def net_label(name, x, y, angle=0):
    justify = "right" if angle == 180 else "left"
    return (f'  (label "{esc(name)}" (at {x:.3f} {y:.3f} {angle}) (fields_autoplaced yes)\n'
            f'    (effects (font (size 1.27 1.27)) (justify {justify}))\n'
            f'    (uuid "{uid()}"))')

def no_connect(x, y):
    return f'  (no_connect (at {x:.3f} {y:.3f}) (uuid "{uid()}"))'

# ── pin type shorthands ───────────────────────────────────────────────────────
BI="bidirectional"; INP="input"; OUT="output"
PWR_IN="power_in"; PWR_OUT="power_out"; PASS="passive"

# ── symbol pin lists ──────────────────────────────────────────────────────────
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

MAX_L  = [("VCC","1",PWR_IN),("GND","2",PWR_IN),("SDI","3",INP),("SDO","4",OUT),("SCK","5",INP),("CS","6",INP),("DRDY","7",OUT)]
BME_L  = [("VCC","1",PWR_IN),("GND","2",PWR_IN),("SCK","3",INP),("SDI","4",INP),("SDO","5",OUT),("CSB","6",INP)]
OLED_L = [("GND","1",PWR_IN),("VCC","2",PWR_IN),("SCL","3",INP),("SDA","4",BI)]
IDS2_L = [("VCC","1",PWR_IN),("GND","2",PWR_IN),("WHITE","3",BI),("YELLOW","4",OUT),("BLUE","5",INP)]
RELAY_L= [("VCC","1",PWR_IN),("GND","2",PWR_IN),("IN","3",INP),("NO","4",PASS),("COM","5",PASS)]
R_L    = [("~","1",PASS)];  R_R = [("~","2",PASS)]
GX12_L = [("GND","1",PWR_IN),("+3V3","2",PWR_IN),("DQ","3",BI),("NC","4",PASS)]
GPEXT_L= [("SIG","1",BI),("GND","2",PWR_IN)]
PEXT_L = [("VCC","1",PWR_IN),("GND","2",PWR_IN)]
PJMP_L = [("IDS2","1",PASS),("5V","2",PASS),("PEXT","3",PASS)]
PSW_L  = [("A","1",PASS),("B","2",PASS)]
BTN_L  = [("+3V3","1",PASS),("ADC_BTN","2",PASS),("GND","3",PASS)]
LED_L  = [("GND","1",PASS),("LED_0","2",PASS),("LED_20","3",PASS),
          ("LED_40","4",PASS),("LED_60","5",PASS),("LED_80","6",PASS),("LED_100","7",PASS)]
PWR_TAP_L  = [("GND","1",PWR_IN),("+3V3","2",PWR_IN),("+5V","3",PWR_IN)]
UART_HDR_L = [("GND","1",PWR_IN),("TX_IO16","2",BI),("RX_IO17","3",BI)]
GPIO_FREE_L= [("GND","1",PWR_IN),("IO15","2",BI),("IO23","3",BI)]
BZ_L       = [("+","1",PASS),("-","2",PASS)]
NPN_L      = [("B","1",INP)]
NPN_R      = [("C","2",PASS),("E","3",PASS)]

# ── lib_symbols section ───────────────────────────────────────────────────────
lib_syms = "\n".join([
    pwrsym("GND", "gnd"), pwrsym("+3V3"), pwrsym("+5V"),
    sym("Brew:ESP32-C6-WROOM-1U","U","ESP32-C6-WROOM-1U", ESP32_L, ESP32_R, bw=22),
    sym("Brew:PCF8574",          "U","PCF8574",           PCF_L,   PCF_R),
    sym("Brew:LevelShifter_4CH", "U","LevelShifter_4CH",  LS_L,    LS_R),
    sym("Brew:MAX31865_Module",  "U","MAX31865",           MAX_L,   [], bw=15),
    sym("Brew:BME680_Module",    "U","BME680",             BME_L,   [], bw=15),
    sym("Brew:SSD1306",          "DS","SSD1306",           OLED_L,  [], bw=15),
    sym("Brew:GGM_IDS2",         "J","GGM_IDS2",          IDS2_L,  [], bw=15),
    sym("Brew:Relay_Module",     "K","Relay",              RELAY_L, [], bw=15),
    sym("Brew:R",                "R","R",                  R_L,     R_R, bw=5),
    sym("Brew:Conn_GX12_4P",     "J","GX12_4P",           GX12_L,  [], bw=15),
    sym("Brew:Conn_GPIO_EXT",    "J","GPIO_EXT",           GPEXT_L, [], bw=15),
    sym("Brew:Conn_PExt",        "J","PExt",               PEXT_L,  [], bw=15),
    sym("Brew:PJump",            "J","PJump",              PJMP_L,  [], bw=15),
    sym("Brew:Conn_PSw",         "J","PSw",                PSW_L,   [], bw=15),
    sym("Brew:Conn_BTN",         "J","BTN_Panel",          BTN_L,   [], bw=15),
    sym("Brew:Conn_LED",         "J","LED_Panel",          LED_L,   [], bw=15),
    sym("Brew:Conn_PWR_TAP",     "J","PWR_TAP",            PWR_TAP_L,  [], bw=15),
    sym("Brew:Conn_UART_HDR",    "J","UART_HDR",           UART_HDR_L, [], bw=15),
    sym("Brew:Conn_GPIO_FREE",   "J","GPIO_FREE",          GPIO_FREE_L,[], bw=15),
    sym("Brew:Buzzer",           "BZ","Buzzer",            BZ_L,       [], bw=10),
    sym("Brew:NPN",              "Q","MMBT2222A",          NPN_L,      NPN_R, bw=10),
])

# ── footprint helpers ─────────────────────────────────────────────────────────

def phoenix(n):
    """Phoenix Contact MPT-0,5/n-2,54 — n-pole 2.54mm horizontal screw terminal."""
    return f"TerminalBlock_Phoenix:TerminalBlock_Phoenix_MPT-0,5-{n}-2.54_1x{n:02d}_P2.54mm_Horizontal"

# ── component placement (x, y in mm — auto-snapped to 2.54 mm grid) ──────────
# Layout:
#   LEFT  (x~60):   sensors — DS18B20 connectors, MAX31865, BME680, OLEDs
#   CENTER (x~130): U1 ESP32-C6
#   RIGHT (x~230):  level shifter, relay, PCF8574 + LED chain
#   FAR RIGHT (x~400-480): power connectors, expansion headers, buzzer
comps = []

# ── CENTER: ESP32-C6 ──────────────────────────────────────────────────────────
comps.append(place("Brew:ESP32-C6-WROOM-1U","U1","ESP32-C6-WROOM-1U", 130, 140,
                   fp="PCM_Espressif:ESP32-C6-DevKitC-1", pins_l=ESP32_L, pins_r=ESP32_R))

# ── LEFT: DS18B20 sensors (OneWire on IO0) ───────────────────────────────────
for (ref, y, n) in [("J7", 45, 1), ("J8", 70, 2), ("J9", 95, 3)]:
    comps.append(place("Brew:Conn_GX12_4P", ref, f"DS18B20_{n}", 60, y,
                       fp=phoenix(4),
                       pins_l=GX12_L))

comps.append(place("Brew:R","R1","4k7", 95, 70,
                   fp="Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
                   pins_l=R_L, pins_r=R_R))

# ── LEFT: SPI sensors ────────────────────────────────────────────────────────
comps.append(place("Brew:MAX31865_Module","U5","MAX31865", 60, 130,
                   fp="Connector_PinSocket_2.54mm:PinSocket_1x07_P2.54mm_Vertical",
                   pins_l=MAX_L))
comps.append(place("Brew:BME680_Module","U6","BME680", 60, 170,
                   fp=phoenix(6),
                   pins_l=BME_L))

# ── RIGHT: BSS138 level shifter + IDS2 ───────────────────────────────────────
comps.append(place("Brew:LevelShifter_4CH","U3","LevelShifter_4CH", 230, 155,
                   fp="Connector_PinSocket_2.54mm:PinSocket_2x06_P2.54mm_Vertical",
                   pins_l=LS_L, pins_r=LS_R))
comps.append(place("Brew:GGM_IDS2","J1","GGM_IDS2", 320, 130,
                   fp="Connector_JST:JST_XH_B5B-XH-A_1x05_P2.50mm_Vertical",
                   pins_l=IDS2_L))

# ── RIGHT: Relay ──────────────────────────────────────────────────────────────
comps.append(place("Brew:Relay_Module","K1","Relay_5V_3V3trig", 230, 215,
                   fp="Connector_PinSocket_2.54mm:PinSocket_1x05_P2.54mm_Vertical",
                   pins_l=RELAY_L))

# ── RIGHT: GPIO EXT ───────────────────────────────────────────────────────────
comps.append(place("Brew:Conn_GPIO_EXT","J2","GPIO_EXT_Conn", 320, 195,
                   fp=phoenix(2),
                   pins_l=GPEXT_L))

# ── RIGHT: Power connectors ───────────────────────────────────────────────────
comps.append(place("Brew:Conn_PExt","J3","PExt_Connector", 400, 100,
                   fp=phoenix(2),
                   pins_l=PEXT_L))
comps.append(place("Brew:PJump","J4","PJump", 400, 130,
                   fp="Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
                   pins_l=PJMP_L))
comps.append(place("Brew:Conn_PSw","SW1","PSw_Connector", 400, 160,
                   fp=phoenix(2),
                   pins_l=PSW_L))

# ── RIGHT: PCF8574 + LED resistors + LED panel ───────────────────────────────
comps.append(place("Brew:PCF8574","U2","PCF8574", 230, 270,
                   fp="Package_DIP:DIP-16_W7.62mm", pins_l=PCF_L, pins_r=PCF_R))
for i, ref in enumerate(["R2","R3","R4","R5","R6","R7"]):
    comps.append(place("Brew:R", ref, "330R", 280 + i*15, 270,
                       fp="Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
                       pins_l=R_L, pins_r=R_R))
comps.append(place("Brew:Conn_LED","J6","LED_Panel", 390, 270,
                   fp=phoenix(7),
                   pins_l=LED_L))

# ── LEFT/BOTTOM: OLEDs ───────────────────────────────────────────────────────
comps.append(place("Brew:SSD1306","DS1","SSD1306_0x3C", 60, 225,
                   fp=phoenix(4),
                   pins_l=OLED_L))
comps.append(place("Brew:SSD1306","DS2","SSD1306_0x3D", 60, 255,
                   fp=phoenix(4),
                   pins_l=OLED_L))

# ── FAR RIGHT: Expansion headers ─────────────────────────────────────────────
comps.append(place("Brew:Conn_PWR_TAP","J10","PWR_TAP", 480, 110,
                   fp="Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
                   pins_l=PWR_TAP_L))
comps.append(place("Brew:Conn_UART_HDR","J11","UART_HDR", 480, 150,
                   fp="Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
                   pins_l=UART_HDR_L))
comps.append(place("Brew:Conn_GPIO_FREE","J12","GPIO_FREE", 480, 190,
                   fp="Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical",
                   pins_l=GPIO_FREE_L))

# ── FAR RIGHT: Buzzer circuit ─────────────────────────────────────────────────
comps.append(place("Brew:Buzzer","BZ1","CPM121", 480, 240,
                   fp="Buzzer_Beeper:Buzzer_12x9.5RM7.6",
                   pins_l=BZ_L))
comps.append(place("Brew:NPN","Q1","BC547B", 480, 270,
                   fp="Package_TO_SOT_THT:TO-92_Inline",
                   pins_l=NPN_L, pins_r=NPN_R))
comps.append(place("Brew:R","R8","1k", 480, 300,
                   fp="Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal",
                   pins_l=R_L, pins_r=R_R))

# ── LEFT/BOTTOM: Button panel ─────────────────────────────────────────────────
comps.append(place("Brew:Conn_BTN","J5","BTN_Panel", 60, 295,
                   fp=phoenix(3),
                   pins_l=BTN_L))

# ── DS18B20 cluster wiring ────────────────────────────────────────────────────
# Wire stubs + net labels on left-side pins; no-connect on NC pin.
# Labels at angle=180 → connection point on RIGHT, text extends LEFT.
# Coordinates are derived from snap() so they always land on the 2.54 mm grid.
wires = []

GX12_bw = snap_bw(15)          # 15.24 mm  (6 × 2.54)
GX12_hw = GX12_bw / 2          #  7.62 mm
COMP_X  = snap(60)              # 60.96 mm  (component centre x)
PIN_X   = COMP_X - GX12_hw - P # 50.80 mm  (left pin endpoint x)
STUB    = 3 * P                 #  7.62 mm  stub length
LBL_X   = PIN_X - STUB         # 43.18 mm  label anchor x

# KiCad note: symbol local-Y is Y-up, but global schematic coords are Y-down.
# Placed pin global_y = comp_y - local_y_from_sym.
# For n=4 pins (GX12_4P), pin index i (0=top, 3=bottom) maps to:
#   global_y = comp_y - (n-1)*P/2 + i*P
# → pin 0 (GND):  comp_y - 3P/2  (smallest y = top in Y-down)
# → pin 3 (NC):   comp_y + 3P/2  (largest  y = bottom in Y-down)

for (_, approx_y, _) in [("J7", 45, 1), ("J8", 70, 2), ("J9", 95, 3)]:
    cy    = snap_y(approx_y, 4)
    y_gnd = cy - 3 * P / 2     # pin 1 (GND)  — top
    y_3v3 = cy -     P / 2     # pin 2 (+3V3)
    y_dq  = cy +     P / 2     # pin 3 (DQ / OneWire)
    y_nc  = cy + 3 * P / 2     # pin 4 (NC)   — bottom

    wires.append(wire_seg(PIN_X, y_gnd, LBL_X, y_gnd))
    wires.append(net_label("GND",      LBL_X, y_gnd, 180))

    wires.append(wire_seg(PIN_X, y_3v3, LBL_X, y_3v3))
    wires.append(net_label("+3V3",     LBL_X, y_3v3, 180))

    wires.append(wire_seg(PIN_X, y_dq, LBL_X, y_dq))
    wires.append(net_label("OneWire",  LBL_X, y_dq,  180))

    wires.append(no_connect(PIN_X, y_nc))

# R1 pull-up: pin1 (left) → OneWire, pin2 (right) → +3V3
# R has 1 pin per side; pin global_y = comp_y (offset = 0).
R_bw  = snap_bw(5)             # 5.08 mm (2 × 2.54)
R_hw  = R_bw / 2               # 2.54 mm
R1_cx = snap(95)               # 93.98 mm
R1_cy = snap_y(70, 1)          # 71.12 mm

R1_lx = R1_cx - R_hw - P      # left pin endpoint
R1_rx = R1_cx + R_hw + P      # right pin endpoint

wires.append(wire_seg(R1_lx, R1_cy, R1_lx - STUB, R1_cy))
wires.append(net_label("OneWire", R1_lx - STUB, R1_cy, 180))

wires.append(wire_seg(R1_rx, R1_cy, R1_rx + STUB, R1_cy))
wires.append(net_label("+3V3",    R1_rx + STUB, R1_cy, 0))

# ── assemble & write ──────────────────────────────────────────────────────────

sch = f"""(kicad_sch (version 20250610) (generator "eeschema") (generator_version "10.0")
  (uuid "{ROOT}")
  (paper "A2")
  (title_block
    (title "BrewingStation v3 — ESP32-C6")
    (date "2026-05-25")
    (rev "1.0")
    (comment 1 "DS18B20 cluster wired — remaining nets: wire manually in KiCad")
  )
  (lib_symbols
{lib_syms}
  )
{chr(10).join(comps)}
{chr(10).join(wires)}
)
"""

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "brewingstation3.kicad_sch")
with open(out, "w", encoding="utf-8") as f:
    f.write(sch)
print(f"Wrote {out}  ({len(comps)} components, {len(wires)} wiring elements)")
