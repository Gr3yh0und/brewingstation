# Brewing Station 3 — Bill of Materials

---

## Main boards & modules

| Ref | Component | Qty | Description | Source |
|---|---|---|---|---|
| U1 | ESP32-C6-DevKitC-1-N8 | 1 | ESP32-C6 dev board, 8MB flash, USB-CDC built-in | [**ESP32-C6DEVKI1N8**](https://www.reichelt.de/de/de/shop/produkt/entwicklungsboard_esp32-c6-wroom-1_u-380385) — Reichelt |
| U2 | PCF8574 | 1 | I2C GPIO expander, DIP-16 | [**PCF 8574 N**](https://www.reichelt.de/de/de/shop/produkt/remote_8-bit_i_o_expander_for_i2c_bus_pdip-16-216402) — Reichelt |
| U3 | BSS138 4-ch level shifter module | 1 | Bidirectional 3.3V↔5V, 2×4 pin module | AliExpress — search "BSS138 level shifter 4 channel"; Reichelt alternative: [**DEBO LOGIC 4CH**](https://www.reichelt.de/de/de/shop/produkt/entwicklerboards_-_bidirektionaler_logikpegelkonverter_4-kanal_-235503) |
| U5 | MAX31865 module | 1 | PT100/PT1000 RTD amplifier, SPI, 7-pin module | AliExpress — search "MAX31865 module" |
| U6 | BME680 breakout | 1 | Temp / humidity / pressure / gas, 6-pin SPI module. Adafruit-compatible pinout (19×17mm). Mount through case cutout. | AliExpress clone (Adafruit-similar) — search "BME680 module SPI I2C"; Reichelt alternative: [**DEBO SENS BME680**](https://www.reichelt.de/de/de/shop/produkt/entwicklerboards_-_sensor_bosch_bme680-253653) — JOY-IT, 30×14mm, verify pinout before ordering |
| K1 | Relay module | 1 | Single-channel, **3.3V-compatible** opto input, NO/NC/COM screw terminals on load side | AliExpress — search "5V relay module 3.3V trigger" |
| DS1, DS2 | SSD1306 OLED 128×64 | 2 | I2C, 4-pin (GND/VCC/SCL/SDA), 0.96". DS1 = 0x3C, DS2 = 0x3D (ADDR pin high) | AliExpress |
| — | GGM IDS2 induction cooker | 1 | Induction cooker with WHITE/YELLOW/BLUE control cable | GGM Gastro |

---

## Probes & case connectors

| Ref | Component | Qty | Description | Source |
|---|---|---|---|---|
| — | DS18B20 probe | 3 | Food-safe stainless steel, silicone cable (lebensmittelecht), OneWire | AliExpress — search "DS18B20 stainless steel silicone food safe" |
| — | PT1000 probe | 1 | Food-safe stainless steel, silicone cable (lebensmittelecht), 4-wire | AliExpress — search "PT1000 stainless steel silicone food safe 4-wire" |
| — | GX12 4-pin aviation connector | 3 | Panel-mount connectors for DS18B20 probes — not on PCB, mounted in case | [Amazon B09WY7KFB6](https://www.amazon.de/gp/product/B09WY7KFB6/) — Aiqeer, 5× 4-pin GX12, M12 thread |

---

## Discrete components (PCB)

| Ref | Component | Qty | Description | Source |
|---|---|---|---|---|
| BZ1 | Magnetic buzzer (active) | 1 | **SUMMER CPM 121**, Ø14mm, RM7.6mm, 3–16V DC, internal oscillator — on/off switching via Q1 NPN | [**SUMMER CPM 121**](https://www.reichelt.de/de/de/shop/produkt/piezosummer-35924) — Reichelt |
| Q1 | BC547B | 1 | NPN transistor, TO-92, buzzer driver | [**BC 547B**](https://www.reichelt.de/de/de/shop/produkt/bipolartransistor_npn_45v_0_1a_0_5w_to-92-5006) — Reichelt |
| R1 | 4.7kΩ 0.6W metal film | 1 | OneWire pull-up for DS18B20 bus | [**METALL 4,70K**](https://www.reichelt.de/de/de/shop/produkt/widerstand_metallschicht_4_70_kohm_0207_0_6_w_1_-11784) — Reichelt |
| R2–R7, R15, R16 | 330Ω 0.6W metal film | 8 | LED current limiters (PCF8574 P0–P5 → J15–J20 pin 3; P6/P7 → J21/J22 pin 2) | [**METALL 330**](https://www.reichelt.de/de/de/shop/produkt/widerstand_metallschicht_330_ohm_0207_0_6_w_1_-11733) — Reichelt |
| R8 | 1kΩ 0.6W metal film | 1 | Base resistor for buzzer NPN driver | [**METALL 1,00K**](https://www.reichelt.de/de/de/shop/produkt/widerstand_metallschicht_1_00_kohm_0207_0_6_w_1_-11403) — Reichelt |
| R17 | 470Ω 0.6W thin film | 1 | Power switch LED current limiter (PSOURCE_OUT → SW1 LED+) | [**DÜNNS 470**](https://www.reichelt.de/de/de/shop/produkt/duennschichtwiderstand_axial_0_6_w_470_ohm_1_-233741) — Reichelt |
| R9 | 10kΩ 0.6W metal film | 1 | Button ladder pull-up (3V3 → ADC_BTN) | [**METALL 10,0K**](https://www.reichelt.de/de/de/shop/produkt/widerstand_metallschicht_10_00_kohm_0207_0_6_w_1_-11449) — Reichelt |
| R10 | 39kΩ 0.6W metal film | 1 | Button ladder — BTN1 (0% power) | [**METALL 39,0K**](https://www.reichelt.de/de/de/shop/produkt/widerstand_metallschicht_39_0_kohm_0207_0_6_w_1_-11766) — Reichelt |
| R11 | 33kΩ 0.6W metal film | 1 | Button ladder — BTN2 (20% power) | [**METALL 33,0K**](https://www.reichelt.de/de/de/shop/produkt/widerstand_metallschicht_33_0_kohm_0207_0_6_w_1_-11730) — Reichelt |
| R12 | 22kΩ 0.6W metal film | 1 | Button ladder — BTN3 (40% power) | [**METALL 22,0K**](https://www.reichelt.de/de/de/shop/produkt/widerstand_metallschicht_22_0_kohm_0207_0_6_w_1_-11622) — Reichelt |
| R13 | 12kΩ 0.6W metal film | 1 | Button ladder — BTN4 (60% power) | [**METALL 12,0K**](https://www.reichelt.de/de/de/shop/produkt/widerstand_metallschicht_12_0_kohm_0207_0_6_w_1_-11482) — Reichelt |
| R14 | 3.3kΩ 0.6W metal film | 1 | Button ladder — BTN5 (80% power) | [**METALL 3,30K**](https://www.reichelt.de/de/de/shop/produkt/widerstand_metallschicht_3_30_kohm_0207_0_6_w_1_-11693) — Reichelt |

---

## Button + LED panel (external — not on PCB)

6 momentary NO push-buttons wired to J15–J20. All pull-up and ladder resistors (R9–R14) are on the PCB — the panel carries only bare switches.

| Component | Qty | Description | Source |
|---|---|---|---|
| Momentary NO push-button | 6 | Panel-mount, any style (SW2–SW7) | — |

---

## Connectors & terminals

Phoenix Contact MPT 0,5/X-2,54 screw terminals are available in 2/3/4/5-pin. For 6-pin U6, install two adjacent 3-pin connectors in the 6-pad footprint (MPT series is joinable at the same pitch).

**2-pin × 6 total** — [**PHC 1725656**](https://www.reichelt.de/de/de/shop/produkt/leiterplattenklemme_2-polig_rm_2_54-60874) — Reichelt

| Ref | Description |
|---|---|
| J3 | PExt external 5V power input |
| J12 | GPIO15 breakout |
| J13 | GPIO10 breakout |
| J14 | GPIO3_ADC breakout |
| J21 | Spare LED P6 |
| J22 | Spare LED P7 |

**3-pin × 11 total** — [**PHC 1725669**](https://www.reichelt.de/de/de/shop/produkt/leiterplattenklemme_3-polig_rm_2_54_mm-60875) — Reichelt

| Ref | Description |
|---|---|
| J7, J8, J9 | DS18B20 sensors ×3 |
| J15–J20 | Button + LED per power level ×6 (0/20/40/60/80/100%) |
| U6 (×2 adjacent) | BME680 module — install two 3-pin connectors side-by-side in the 6-pad footprint |

**4-pin × 3 total** — [**PHC 1725672**](https://www.reichelt.de/de/de/shop/produkt/leiterplattenklemme_4-polig_rm_2_54_mm-60876) — Reichelt

| Ref | Description |
|---|---|
| SW1 | Power switch + integrated LED (SW_A, SW_B, LED+, LED−) |
| DS1, DS2 | SSD1306 OLEDs ×2 |

**5-pin × 1 total** — [**PHC 1725685**](https://www.reichelt.de/de/de/shop/produkt/leiterplattenklemme_5-polig_rm_2_54_mm-255909) — Reichelt

| Ref | Description |
|---|---|
| J1 | GGM IDS2 induction cooker (IDS2_5V, GND, WHITE, YELLOW, BLUE) |

**Other connectors**

| Ref | Component | Qty | Description | Source |
|---|---|---|---|---|
| J4, J10, J11, K1 | Male pin header, 2.54mm | — | Break to length (1×3) | [**SL 1X36G 2,54**](https://www.reichelt.de/de/de/shop/produkt/36pol_stiftleiste_gerade_rm_2_54-19504) — Reichelt |
| U1 | Female socket 1×16, 2.54mm | 2 | DevKit socket — swappable | [**MPE 094-1-016**](https://www.reichelt.de/de/de/shop/produkt/buchsenleisten_2_54_mm_1x16_gerade-119919) — Reichelt |
| U3, U5 | Female socket 1×6/1×7, 2.54mm | 2 | Level shifter + MAX31865 — swappable | [**MPE 094-1-006**](https://www.reichelt.de/de/de/shop/produkt/buchsenleisten_2_54_mm_1x06_gerade-119915) — Reichelt |
