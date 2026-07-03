// ─────────────────────────────────────────────────────────────────────────────
// Brewstation v3.0 — 3D Printable Case
// PCB: 100 × 80 mm, 1.6 mm thick, 4× M3 corner mounting holes
//
// Two parts:
//   1. tray()  — bottom + four walls, open top
//   2. lid()   — flat top plate with two OLED windows
//
// Lid fastening: 4× M3 screws through lid corners into tray bosses.
//   Tray bosses hold M3 brass heat-set inserts (Ø4.4 mm × 4 mm).
//   Lid has 3.2 mm clearance holes + M3 flat-head countersinks.
//   Recommended screw: M3 × 10 mm flat-head (countersunk).
//
// Print orientation:
//   tray → upright, open top up  (no supports needed)
//   lid  → face up, flat         (no supports needed)
//
// All side holes (USB, connectors, buttons, sensors) are drilled by user.
// Only the two OLED rectangular windows are pre-modelled.
// ─────────────────────────────────────────────────────────────────────────────

// ── Adjustable parameters ─────────────────────────────────────────────────────
pcb_w       = 100;    // PCB width  (X axis, left ↔ right)
pcb_d       = 80;     // PCB depth  (Y axis, front ↔ back)
wall        = 3.0;    // Wall / floor thickness (mm)
lid_t       = 4.0;    // Lid thickness — needs to be ≥ countersink depth
clearance   = 10.0;   // Gap between PCB edge and inner wall (each side)
inner_h     = 84.0;   // Internal cavity height (floor surface → lid underside)
standoff_h  = 12.0;   // PCB standoff height above floor
mh_inset    = 4.0;    // PCB M3 standoff centre inset from each PCB edge

// Lid screw bosses (M3 heat-set insert, Ø4.4 × 4 mm)
boss_r      = 5.5;    // Boss outer radius (11 mm Ø)
boss_h      = 7.0;    // Boss height above tray rim
insert_r    = 2.2;    // Heat-set insert hole radius (4.4 mm Ø for M3)
insert_d    = 4.5;    // Insert hole depth (insert 4 mm + 0.5 mm margin)
screw_r     = 1.6;    // M3 clearance hole radius in lid (3.2 mm Ø)
csk_r       = 2.9;    // M3 flat-head countersink radius (5.8 mm Ø)
csk_d       = 1.8;    // Countersink depth

// OLED window dimensions (0.96" SSD1306 active area ~22 × 11 mm + margin)
oled_cut_w  = 26;
oled_cut_h  = 14;

// OLED window centre positions, measured from inner-left / inner-front corner.
// "front" = top edge of PCB as you look down at the board.
// Adjust oled1_cx / oled2_cx to match where you mount your OLED modules.
oled1_cx    = clearance + 22;   // centre X of left OLED
oled2_cx    = clearance + 57;   // centre X of right OLED
oled_cy     = clearance + 6;    // Y from inner front wall

// ── Derived (do not edit) ─────────────────────────────────────────────────────
inner_w  = pcb_w + 2 * clearance;   // 120 mm
inner_d  = pcb_d + 2 * clearance;   // 100 mm
outer_w  = inner_w + 2 * wall;      // 126 mm
outer_d  = inner_d + 2 * wall;      // 106 mm
tray_h   = wall + inner_h;          //  87 mm outer tray height

// Boss centres at the four outer corners of the box
boss_pos = [
    [ boss_r,           boss_r           ],
    [ outer_w - boss_r, boss_r           ],
    [ boss_r,           outer_d - boss_r ],
    [ outer_w - boss_r, outer_d - boss_r ],
];

// PCB corner standoff positions in inner-cavity coordinates
mh_pos = [
    [ clearance + mh_inset,         clearance + mh_inset         ],
    [ clearance + pcb_w - mh_inset, clearance + mh_inset         ],
    [ clearance + mh_inset,         clearance + pcb_d - mh_inset ],
    [ clearance + pcb_w - mh_inset, clearance + pcb_d - mh_inset ],
];

// ─────────────────────────────────────────────────────────────────────────────
// TRAY
// ─────────────────────────────────────────────────────────────────────────────
module tray() {
    difference() {
        union() {
            // Outer shell
            cube([outer_w, outer_d, tray_h]);
            // Screw bosses — protrude above tray rim to receive lid screws
            for (p = boss_pos)
                translate([p[0], p[1], tray_h])
                    cylinder(h = boss_h, r = boss_r, $fn = 48);
        }
        // Hollow interior — floor stays, top is open
        translate([wall, wall, wall])
            cube([inner_w, inner_d, tray_h]);
        // Heat-set insert holes — drilled down from top of each boss
        for (p = boss_pos)
            translate([p[0], p[1], tray_h + boss_h])
                mirror([0, 0, 1])
                    cylinder(h = insert_d, r = insert_r, $fn = 32);
    }

    // PCB corner standoffs — solid, user drills M3 pilot if needed
    for (p = mh_pos)
        translate([wall + p[0], wall + p[1], wall])
            cylinder(h = standoff_h, r = 4.0, $fn = 32);
}

// ─────────────────────────────────────────────────────────────────────────────
// LID
// ─────────────────────────────────────────────────────────────────────────────
module lid() {
    difference() {
        // Flat plate, same footprint as tray
        cube([outer_w, outer_d, lid_t]);

        // M3 clearance holes + countersinks at boss positions
        for (p = boss_pos) {
            // Clearance hole through full lid thickness
            translate([p[0], p[1], -0.1])
                cylinder(h = lid_t + 0.2, r = screw_r, $fn = 32);
            // Countersink from top face
            translate([p[0], p[1], lid_t - csk_d])
                cylinder(h = csk_d + 0.1, r1 = screw_r, r2 = csk_r, $fn = 32);
        }

        // OLED window 1 (left display)
        translate([wall + oled1_cx - oled_cut_w/2,
                   wall + oled_cy,
                   -0.1])
            cube([oled_cut_w, oled_cut_h, lid_t + 0.2]);

        // OLED window 2 (right display)
        translate([wall + oled2_cx - oled_cut_w/2,
                   wall + oled_cy,
                   -0.1])
            cube([oled_cut_w, oled_cut_h, lid_t + 0.2]);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RENDER — side by side for slicer import
// Comment out one module to export parts individually as STL.
// ─────────────────────────────────────────────────────────────────────────────
tray();
translate([outer_w + 20, 0, 0]) lid();

// ─────────────────────────────────────────────────────────────────────────────
// Key dimensions:
//   Outer footprint  : 126 × 106 mm
//   Total height     :  90 mm (tray 87 mm + lid 4 mm, bosses 7 mm above rim)
//   Internal cavity  : 120 × 100 × 84 mm
//   PCB sits at      :  15 mm from floor (3 mm floor + 12 mm standoffs)
//   Wire space below : 12 mm  |  Wire space above: ~70 mm
//   Fasteners        : 4× M3 × 10 mm flat-head + M3 brass heat-set inserts
// ─────────────────────────────────────────────────────────────────────────────
