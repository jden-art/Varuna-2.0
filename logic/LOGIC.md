```
You are being given the complete technical specification, physics model, sensor
architecture, failure modes, and algorithmic logic for a RIVER WATER LEVEL
MONITORING SYSTEM based on a tethered capsule buoy. You must internalize every
detail below before answering any questions or generating any code, hardware
designs, or analyses related to this system.

Read this document in its entirety. Do not skim. Every section matters.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SECTION 1: SYSTEM OVERVIEW AND CORE PRINCIPLE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1.1 WHAT THIS SYSTEM IS

This is a river water level monitoring and flood early-warning system. It
consists of a capsule-shaped buoy tethered by a fixed-length rope to an anchor
on the riverbed. By measuring the buoy's tilt angle, acceleration profile, and
ambient pressure, the system infers the water level, current speed, and flood
conditions in real time.

1.2 CORE MEASUREMENT PRINCIPLE

The buoy is attached to a riverbed anchor by a tether of fixed length L. The
tether length L is chosen to equal the MAXIMUM expected safe water level — the
height at which flooding begins. When river current pushes the buoy, the
tether pulls taut, and the buoy sits at the water surface at some horizontal
offset from the anchor. This forms a RIGHT TRIANGLE:

    Hypotenuse = L (tether length, fixed and known)
    Vertical leg = H (water level above riverbed — the quantity we measure)
    Horizontal leg = d (horizontal displacement of buoy from anchor)

    The fundamental equation:

    ┌─────────────────────────────────────────────┐
    │                                             │
    │          H = L × cos(θ)                     │
    │                                             │
    │   where θ = angle of tether from vertical   │
    │   L = tether length (known constant)        │
    │   H = water level (what we want)            │
    │                                             │
    └─────────────────────────────────────────────┘

    Equivalently:
        d = L × sin(θ)          (horizontal displacement)
        θ = arccos(H / L)       (tether angle)
        H² + d² = L²            (Pythagorean relation)

    The two complementary angles in the triangle:
        θ₁ = angle at the buoy (between vertical and tether)
        θ₂ = angle at the anchor (between horizontal and tether)
        θ₁ + θ₂ + 90° = 180°
        θ₁ + θ₂ = 90°

        H = L × cos(θ₁) = L × sin(θ₂)
        d = L × sin(θ₁) = L × cos(θ₂)

1.3 MOVEMENT MODEL

The buoy moves TANGENTIALLY relative to the anchor. It is constrained to a
SEMICIRCULAR ARC of radius L centered on the anchor point. This means:

    - The buoy can swing from directly left of anchor (θ = -90°) to directly
      right (θ = +90°), sweeping through the vertical (θ = 0°, directly above)
    - At any point on this arc, the tether is taut and the buoy's distance
      from the anchor is exactly L
    - The motion is analogous to a pendulum, but inverted (the "weight" is
      buoyant and floats upward, and the tether pulls DOWN toward the anchor)
    - Current pushes the buoy downstream, displacing it horizontally, which
      increases θ and reduces the measured H

    Visualization of the arc:

                     ● Buoy (θ=0, directly above)
                    /|\
                   / | \
                  /  |  \
              ● /   |H  \ ●  (θ = ±some angle)
               /    |    \
              / θ   |     \
             /      |      \
    ────────●───────┼───────●──── Riverbed
          (max     Anchor   (max
           left)             right)

    The semicircular constraint means:
    - Buoy ALWAYS stays at distance L from anchor (when tether is taut)
    - Water level directly determines the vertical position
    - Current speed determines the horizontal displacement
    - Both are encoded in angle θ

1.4 WHY TETHER LENGTH = FLOOD THRESHOLD

The tether length L is deliberately set equal to the maximum safe water level.
This creates a natural flood detection mechanism:

    H < L:  Buoy is at surface, tether is taut (if current exists) or slack
            (if calm). θ > 0. Normal operation.

    H = L:  Buoy is directly above anchor. θ = 0. Tether is vertical.
            This is the FLOOD THRESHOLD. Water has reached maximum safe level.

    H > L:  Water level exceeds tether length. The buoy CANNOT rise above L
            because the tether holds it. The buoy becomes SUBMERGED.
            This is a FLOOD CONDITION. The pressure sensor detects submersion.

            Extended measurement:
            H_flood = L + ΔP / (ρ × g)
            where ΔP = pressure above atmospheric

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SECTION 2: PHYSICAL HARDWARE — THE BUOY
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

2.1 CAPSULE SHAPE

The buoy is a CAPSULE (also called a stadiumoid or spherocylinder): a cylinder
with hemispherical end caps. It is characterized by two parameters:

    - Height (H_cap): The total length from tip to tip. Typical: 0.10 — 0.80 m
    - Diameter (D_cap): The diameter of the cylindrical section. Typical: 0.03 — 0.25 m

    The capsule radius: r = D_cap / 2

    Cylindrical section length: cyl = max(0, H_cap - 2r)

    Total volume:
        V_cap = π × r² × cyl + (4/3) × π × r³

    This can be decomposed as:
        V_cylinder = π × r² × cyl       (middle section)
        V_spheres  = (4/3) × π × r³     (two hemispherical end caps = one sphere)

    The capsule shape is chosen because:
        ✅ Hydrodynamically smooth — low drag when aligned with current
        ✅ Rotationally stable — self-rights when ballasted correctly
        ✅ Strong structurally — no sharp edges or stress concentrations
        ✅ Easy to manufacture (pipe + end caps)

2.2 MASS AND DENSITY

    Mass (m): Configurable. Typical: 0.02 — 2.0 kg
    Capsule density: ρ_cap = m / V_cap

    CRITICAL DESIGN RULE:
        ρ_cap < ρ_water  →  Buoy FLOATS (required for operation)
        ρ_cap > ρ_water  →  Buoy SINKS (system failure)

    The fraction submerged when floating:
        f_sub = ρ_cap / ρ_water

    Example: ρ_cap = 600 kg/m³, ρ_water = 1000 kg/m³
        → 60% submerged, 40% above water

2.3 CENTER OF MASS (CoM) AND BALLASTING

The center of mass is NOT at the geometric center. It is deliberately
offset downward through ballasting (heavy material at the bottom).

    Ballast position parameter: B ∈ [0, 1]
        B = 0   → CoM at very bottom of capsule
        B = 0.5 → CoM at geometric center
        B = 1   → CoM at very top

    CoM offset from geometric center (along capsule axis):
        offset_CoM = (B - 0.5) × H_cap × 0.4

    DESIGN REQUIREMENT: B < 0.5 (CoM below geometric center)
    OPTIMAL RANGE: B ≈ 0.2 — 0.35

    Why low CoM matters:
        ✅ The buoy self-rights if tipped (like a Weeble)
        ✅ In current, the buoy aligns with the tether (bottom faces anchor)
        ✅ Reduces oscillation amplitude
        ✅ Ensures the MPU6050 has a consistent reference orientation

    The ballast is achieved by placing heavy components (batteries,
    electronics, lead weights) at the bottom of the capsule.

2.4 MOMENT OF INERTIA

The moment of inertia about the center of mass determines how the buoy
resists rotational acceleration:

    I = (1/12) × m × (3r² + H_cap²) + m × offset_CoM²

    The first term is for a uniform cylinder; the second is the parallel
    axis theorem correction for the offset CoM.

    Higher I → slower rotational response → more stable but less responsive
    Lower I → faster rotational response → more sensitive to waves

2.5 TETHER ATTACHMENT POINT

The tether attaches to the buoy at a configurable point along its axis:

    Attachment parameter: A ∈ [0, 0.5]
        A = 0   → Tether attaches at the VERY BOTTOM of the buoy
        A = 0.5 → Tether attaches at the CENTER of the buoy

    Attachment position (distance from bottom):
        d_attach = A × H_cap

    Position along capsule axis from center:
        pos = -H_cap/2 + A × H_cap

    DESIGN REQUIREMENT: A should be 0 or very close to 0.
    The tether MUST attach at the VERY BOTTOM of the buoy.

    WHY BOTTOM ATTACHMENT IS CRITICAL:

    When current flows, it exerts drag on the buoy. The drag acts at the
    CENTER OF PRESSURE (CoP), which is roughly at the geometric center
    of the submerged portion. The tether tension acts at the attachment
    point.

    The ALIGNMENT TORQUE is:
        τ_alignment = T × d_lever

    where d_lever is the distance from the attachment point to the CoP.

    If attachment is at BOTTOM: d_lever ≈ H_cap/2 (MAXIMUM lever arm)
        → Strong alignment torque → buoy points toward anchor ✅

    If attachment is at CENTER: d_lever ≈ 0 (NO lever arm)
        → No alignment torque → buoy flops around randomly ❌

    The bottom attachment also ensures:
        ✅ Tether tension pulls the heavy bottom toward the anchor
        ✅ Buoyancy pushes the light top away from the anchor
        ✅ These two effects COOPERATE to align the buoy along the tether
        ✅ The capsule axis approximately equals the tether direction

2.6 REQUIRED STRUCTURAL PROPERTIES SUMMARY

    ✅ Tether attaches at the VERY BOTTOM of buoy
    ✅ Center of mass LOW (heavy bottom — ballast, batteries, electronics)
    ✅ Buoy is elongated (capsule shape — large lever arm for tension)
    ✅ Buoy is compact (small displaced volume — weak buoyancy torque)
    ✅ Strong current → large tension → strong alignment torque

    ❌ Tether attaches at middle → weak tension torque (BAD)
    ❌ Light buoy with high CoM → strong buoyancy restoring torque (BAD)
    ❌ Wide flat buoy → strong buoyancy torque (BAD)
    ❌ Very calm water → weak tension, buoyancy dominates (PROBLEMATIC)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SECTION 3: SENSORS ON THE BUOY
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

3.1 MPU6050 — 3-AXIS GYROSCOPE AND 3-AXIS ACCELEROMETER

The MPU6050 is an inertial measurement unit (IMU) that provides:

    Gyroscope: Measures angular velocity (ω) in three axes (°/s)
        ω_x, ω_y, ω_z

    Accelerometer: Measures acceleration (including gravity) in three axes (m/s²)
        a_x, a_y, a_z

    The MPU6050 is mounted inside the buoy with a known orientation relative
    to the capsule axis. Typically:
        z-axis aligned with capsule long axis (pointing from bottom to top)
        x-axis and y-axis perpendicular to capsule axis

3.1.1 MEASURING ANGLE θ WITH THE GYROSCOPE

    The gyroscope measures angular velocity ω(t).
    Integrating gives angle:

        θ(t) = θ₀ + ∫ ω(t) dt

    PROBLEM: Gyroscope drift.
        The MPU6050 has a drift rate of 20-80°/hour.
        After 1 hour, θ could be off by up to 80°.
        After 1 day, the reading is completely meaningless.

    CONCLUSION: Gyroscope alone is INSUFFICIENT for absolute angle
    measurement over long periods. It is excellent for SHORT-TERM
    (sub-second) angular rate measurement and tracking rapid changes.

3.1.2 MEASURING ANGLE θ WITH THE ACCELEROMETER

    When the buoy is stationary or in steady-state:

        θ = atan2(a_x, a_z)

    This gives the tilt angle relative to true vertical (gravity).

    PROBLEM: On a buoy in a river, the accelerometer also sees:
        - Wave-induced accelerations (±2-10° noise)
        - Turbulence vibrations
        - Tether snapping and jerking
        - Vortex-induced oscillations

    CONCLUSION: Accelerometer alone gives noisy but DRIFT-FREE absolute
    angle. Good for long-term average, bad for instantaneous reading.

3.1.3 SENSOR FUSION — COMBINING GYRO AND ACCELEROMETER

    The key insight: gyro is good short-term (no noise) but drifts long-term.
    Accelerometer is good long-term (no drift) but noisy short-term.
    Fusing them gives the best of both.

    METHOD 1: Complementary Filter (simple, effective)

        θ_fused = α × (θ_prev + ω × dt) + (1 - α) × atan2(a_x, a_z)

        α ≈ 0.96 — 0.98
        Trust gyro for fast changes, accelerometer for slow drift correction.
        Result: ±0.5 — 2° accuracy in moderate river conditions.

    METHOD 2: Kalman Filter (optimal, more complex)

        State: [θ, ω_bias]
        Prediction: θ_pred = θ_prev + (ω_measured - ω_bias) × dt
        Update: correct using accelerometer reading
        Adapts noise estimates automatically.
        Result: ±0.3 — 1° accuracy.

    METHOD 3: Madgwick / Mahony AHRS Filter

        Quaternion-based orientation estimation.
        Very efficient, works well on microcontrollers.
        Result: ±0.3 — 1° accuracy.

    METHOD 4: MPU6050 DMP (Digital Motion Processor) ⭐ RECOMMENDED

        The MPU6050 has a BUILT-IN digital motion processor that performs
        sensor fusion IN HARDWARE. It outputs quaternions or Euler angles
        directly. This is:
            ✅ More accurate than software filters
            ✅ Lower CPU load on the microcontroller
            ✅ Handles calibration internally
            ✅ Outputs at up to 200 Hz

        Use the MPU6050 DMP. It is the best option for this application.

3.1.4 ANGLE ACCURACY AND ITS IMPACT ON WATER LEVEL

    The water level error from angle error:

        H = L × cos(θ)
        dH/dθ = -L × sin(θ)
        ΔH = L × sin(θ) × Δθ

    For L = 3m and Δθ = 1° (0.0175 rad):

        θ = 10° → ΔH = 3 × 0.174 × 0.0175 = 0.009 m ≈ 1 cm
        θ = 30° → ΔH = 3 × 0.500 × 0.0175 = 0.026 m ≈ 3 cm
        θ = 60° → ΔH = 3 × 0.866 × 0.0175 = 0.045 m ≈ 5 cm
        θ = 80° → ΔH = 3 × 0.985 × 0.0175 = 0.052 m ≈ 5 cm

    With good sensor fusion (±1°), water level accuracy is ±1-5 cm
    depending on the tether angle. This is ADEQUATE for flood monitoring.

    Note: accuracy is WORST at large θ (low water level relative to L)
    and BEST at small θ (water level near flood threshold). This is
    fortunate because accuracy matters most near the flood threshold.

3.2 PRESSURE SENSOR

    A waterproof pressure sensor (e.g., MS5803, BMP280 in sealed housing)
    measures absolute pressure at the sensor location on the buoy.

    Reading:
        P_measured = P_atmospheric + ρ × g × depth_below_surface

3.2.1 THREE-STATE DETECTION

    State 1: BUOY IS DRY or AT SURFACE
        P_measured ≈ P_atmospheric (±100 Pa)
        The buoy is floating at the surface, or the sensor is above water.
        depth ≈ 0 or slightly positive (sensor might be just below waterline)

    State 2: BUOY IS FLOATING NORMALLY
        P_measured = P_atm + small offset
        The sensor (mounted low on the buoy) is slightly below the waterline.
        P_measured = P_atm + ρ × g × draft
        For a typical buoy, draft ≈ 0.02 — 0.10 m
        Pressure offset: 200 — 1000 Pa above atmospheric

    State 3: BUOY IS SUBMERGED (FLOOD!)
        P_measured >> P_atm
        The entire buoy is underwater because H > L.
        The tether prevents the buoy from rising.
        P_measured = P_atm + ρ × g × (H - L + buoy_depth_offset)

    Transition from State 2 → State 3:
        ╔═══════════════════════════════════════════════╗
        ║         🚨 FLOOD ALERT TRIGGERED 🚨           ║
        ║   Water level has exceeded tether length L    ║
        ║   Submersion depth is measurable via pressure ║
        ╚═══════════════════════════════════════════════╝

3.2.2 FLOOD-LEVEL MEASUREMENT EXTENSION

    Without pressure sensor: maximum measurable water level = L
    With pressure sensor: maximum measurable water level = L + pressure_range/(ρg)

    For a sensor with 1 bar gauge range:
        max_extra = 100000 / (1000 × 9.81) ≈ 10.2 m above L

    Extended flood measurement:
        H_flood = L + (P_measured - P_atm) / (ρ × g)

    This is extremely valuable: when flooding occurs, you not only DETECT it
    but can MEASURE how deep the flood is.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SECTION 4: THE SLACK TETHER PROBLEM
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

This is the MOST CRITICAL failure mode of the system. Understanding and
solving it is essential.

4.1 WHAT HAPPENS WHEN THE TETHER IS SLACK

    When there is insufficient horizontal force to pull the buoy sideways,
    the buoy floats DIRECTLY ABOVE the anchor. The tether hangs loosely
    below the buoy.

                TAUT                              SLACK

    Surface ~~~~~~●~~~~~~~~~        Surface ~~~~~●~~~~~
                 /                               |
                / θ                              |  (hanging loose)
               /                                 ⌢
          L   /    H < L                     L   |   H < L
             /     BUT current                   |   AND no current
            /      pushes buoy out               |
           /                                     |
    ──────●────── Riverbed           ────────────●──── Riverbed
        Anchor                                 Anchor

    When taut: cos(θ) = H/L ✓ WORKS — gives correct water level
    When slack: θ ≈ 0, cos(0) = 1, H_measured = L ≠ H  ✗ FAILS

    The system reports the water level as L (flood threshold) when in
    reality it could be much lower. This is a FALSE FLOOD ALARM or simply
    a garbage reading.

4.2 WHEN DOES SLACK OCCUR?

    The tether is slack when:
        distance(buoy, anchor) < L

    The buoy floats at the surface directly above the anchor when:
        distance = H (water level height)

    Therefore: tether is slack when H < L AND the horizontal displacement
    of the buoy is less than sqrt(L² - H²).

    Horizontal displacement is caused by:
        - River current (drag force on buoy)
        - Wind on the exposed portion of the buoy
        - Wave orbital motion

    The MINIMUM current speed to make the tether taut:

        F_drag = T × sin(θ)

        where T ≈ net buoyancy force = (ρ_water - ρ_cap) × V_cap × g

        ½ × ρ_water × v² × Cd × A = T × sin(arccos(H/L))

        v_min = sqrt(2 × T × sin(arccos(H/L)) / (ρ_water × Cd × A))

    Numerical estimates for a typical small buoy (T_net ≈ 2N, Cd×A ≈ 0.005 m²):

        H/L = 0.9 → θ = 26° → v_min ≈ 0.9 m/s (moderate river)
        H/L = 0.7 → θ = 46° → v_min ≈ 1.2 m/s (fast river)
        H/L = 0.5 → θ = 60° → v_min ≈ 1.3 m/s (fast river)
        H/L = 0.3 → θ = 73° → v_min ≈ 1.4 m/s (very fast)

    In CALM conditions (v < 0.3 m/s), the tether is almost always slack
    unless H is very close to L.

4.3 DETECTING TAUT vs SLACK WITH THE ACCELEROMETER

    TAUT TETHER:
        The buoy is constrained on a circular arc. It cannot move freely.
        The accelerometer reads acceleration ≠ pure gravity.
        There is a CENTRIPETAL component (buoy on circular arc).
        There is a steady offset from tether tension.
        Specifically: the measured gravity direction in the buoy frame
        does NOT point straight down relative to the buoy axis.
        The lateral acceleration (a_x) has a persistent nonzero component
        proportional to T × sin(α) / m.

    SLACK TETHER:
        The buoy floats freely. Only forces are buoyancy + gravity.
        These are perfectly vertical and balanced.
        Accelerometer reads: pure gravity (0, 0, g) in buoy frame.
        θ_gyro ≈ 0° (buoy is upright, buoyancy-dominated).
        No lateral acceleration component.

    DETECTION ALGORITHM:

        lateral_accel = sqrt(a_x² + a_y²)    // horizontal plane

        if (lateral_accel > threshold) → TAUT
        else → SLACK

        threshold ≈ 0.1 — 0.3 m/s² (calibrate in lab)

4.4 SOLUTION: DUAL-MODE SENSING (PRIMARY SOLUTION)

    The system operates in THREE modes with automatic switching:

    ┌─────────────────────────────────────────────────────┐
    │              DECISION LOGIC                         │
    │                                                     │
    │   Read: accelerometer, gyro, pressure               │
    │                                                     │
    │   ┌─────────────────────────┐                       │
    │   │ Is pressure >> P_atm?   │──YES──► FLOOD MODE    │
    │   └────────┬────────────────┘        H = L + ΔP/ρg  │
    │            NO                                       │
    │   ┌────────▼────────────────┐                       │
    │   │ Is tether taut?         │──YES──► TRIG MODE     │
    │   │ (detect via accel)      │        H = L × cos(θ) │
    │   └────────┬────────────────┘                       │
    │            NO                                       │
    │   ┌────────▼────────────────┐                       │
    │   │ SLACK MODE              │──────► ESTIMATE MODE  │
    │   │ H < L, low current      │        H < L (safe)   │
    │   └─────────────────────────┘        Use pressure   │
    │                                      delta or report│
    │                                      "below flood   │
    │                                      threshold"     │
    └─────────────────────────────────────────────────────┘

    Pseudocode:

        enum Mode { TAUT, SLACK, FLOOD };

        Mode detectMode(float ax, float ay, float az, float pressure) {
            float P_gauge = pressure - P_ATM;

            // Flood: pressure indicates submersion
            if (P_gauge > SUBMERSION_THRESHOLD)    // e.g., 500 Pa ≈ 5cm
                return FLOOD;

            // Check lateral acceleration for tether tension
            float lateral = sqrt(ax*ax + ay*ay);
            float tilt = atan2(sqrt(ax*ax + ay*ay), az);

            if (lateral > 0.15 && tilt > 3.0 * DEG_TO_RAD)
                return TAUT;

            return SLACK;
        }

        float getWaterLevel(Mode mode, float theta, float pressure) {
            switch(mode) {
                case TAUT:
                    return L * cos(theta);             // Primary measurement

                case FLOOD:
                    float depth = (pressure - P_ATM) / (RHO * G);
                    return L + depth;                  // Extended range

                case SLACK:
                    return -1;  // Report "below flood threshold"
                                // OR use supplementary measurement
            }
        }

4.5 SUPPLEMENTARY SOLUTION: SUBSURFACE DRAG ELEMENT

    Attach a small drogue/fin/vane to the buoy or tether that catches
    current. This ensures that even minimal current (0.05 m/s) produces
    enough drag to keep the tether taut.

    Design: Flat plate (10cm × 15cm) attached to bottom of buoy,
    perpendicular to current. Catches current effectively. Low cost.

    This reduces the minimum current needed for a taut tether dramatically,
    but does NOT eliminate the slack problem in truly still water (ponds,
    reservoirs, dead-calm conditions).

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SECTION 5: MEASURING WATER CURRENT SPEED
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

5.1 CAN THE ACCELEROMETER MEASURE CURRENT SPEED?

    Yes, BUT with significant caveats.

    WHAT THE ACCELEROMETER READS:

        a_measured = a_gravity + a_dynamic

    At TRUE equilibrium (perfectly still buoy in steady current):
        a_dynamic = 0
        a_measured = g (just gravity)
        → You learn NOTHING about current speed. Just tilt angle.

    During TRANSIENT motion (buoy accelerating after perturbation):
        a_dynamic = F_net / m_eff
        a_measured = g + (F_drag + F_buoyancy + F_tension) / m_eff
        → Current information IS encoded here, but mixed with everything

    CRITICAL INSIGHT: At steady state, the tether angle θ is determined by
    the RATIO of horizontal drag force to vertical net buoyancy force. But
    since θ also encodes water level (H = L×cos(θ)), you CANNOT separate
    current speed from water level using angle alone.

    In other words: different combinations of (H, v_current) can produce
    the SAME angle θ. The angle is NOT a unique function of current speed.

5.2 THE LOG DECREMENT METHOD (BEST APPROACH)

    Your buoy on a taut tether IS a damped pendulum. The damping is
    provided by water drag, which depends on current speed.

    Equation of motion along the arc:

        (m + m_added) × L × θ̈ = RESTORING + DAMPING

        Restoring: -(W_net) × sin(θ - θ₀)     ← gravity/buoyancy
        Damping:   -½ρ|v_rel|² × Cd × A       ← WATER DRAG

    When the buoy is perturbed from equilibrium:

        θ(t) = θ₀ + A × e^(-γt) × cos(ωt + φ)

        γ = decay rate       ← DEPENDS ON CURRENT SPEED
        ω = natural frequency ← depends on geometry and net buoyancy

    The key relationship:

        In flowing water, the relative velocity:
            v_rel = v_current - v_buoy

        Drag force linearized around steady state:
            F_drag ≈ ρ × v_current × Cd × A × v_buoy_perturbation

        Damping coefficient:
            b = ρ × v_current × Cd × A × L

        Decay rate:
            γ = b / (2 × (m + m_added) × L)
            γ = (ρ × v_current × Cd × A) / (2 × (m + m_added))

        SOLVING FOR CURRENT SPEED:
        ┌─────────────────────────────────────────────────┐
        │                                                 │
        │   v_current = 2γ × (m + m_added) / (ρ × CdA)  │
        │                                                 │
        │   Everything on the right is either:            │
        │   • MEASURED (γ from accelerometer data)        │
        │   • KNOWN BY DESIGN (m, CdA)                   │
        │   • ESTIMABLE (m_added ≈ 0.5 × displaced mass) │
        │                                                 │
        └─────────────────────────────────────────────────┘

5.3 EXTRACTING γ: THE LOG DECREMENT PROCEDURE

    After a perturbation (natural wave, debris, wind gust):

        accel
          │  A₁
          │  ╱╲
          │ ╱  ╲    A₂
          │╱    ╲  ╱╲
          ┼──────╲╱──╲──A₃──── time
          │       ╲  ╱╲ ╱╲
          │        ╲╱  ╲╱

        Find successive PEAKS: A₁, A₂, A₃, ...

        Log decrement:
            δ = ln(A₁ / A₂)

        More accurate (average over N peaks):
            δ = (1/N) × ln(A₁ / A_{N+1})

        Decay rate:
            γ = δ × f_oscillation

        Damping ratio:
            ζ = δ / sqrt(4π² + δ²)

5.4 NATURAL PERTURBATION SOURCES

    You do NOT need to artificially disturb the buoy:

        Source                  Frequency        Amplitude
        ──────                  ─────────        ─────────
        Passing waves           Every 1-5 sec    Good
        Turbulent eddies        Continuous       Moderate
        Wind gusts              Every 5-30 sec   Good
        Debris/branches         Random           Large
        Boat wake               Occasional       Excellent
        Fish bumping tether     Random           Small
        Water level changes     Slow             Large

    Algorithm:
        1. Continuously monitor accelerometer
        2. WAIT for acceleration spike > threshold
        3. Start recording peaks
        4. When oscillation decays below noise floor:
           compute log decrement → γ → v_current
        5. Go back to step 1

        Typical wait time: 5-60 seconds
        Analysis window: 3-10 seconds
        Update rate: every 10-60 seconds

5.5 HYBRID CURRENT SPEED ESTIMATION

    For best accuracy, combine multiple approaches:

    CONTINUOUS (always running, trivial computation):
    │
    ├── RMS Acceleration
    │   rms_accel = sqrt(running_mean(accel²))
    │   v_rough = K × pow(rms_accel, 0.6)      // empirical calibration
    │   Accuracy alone: ±40-60%
    │
    EVENT-TRIGGERED (when perturbation detected):
    │
    ├── Log Decrement Analysis
    │   Wait for natural perturbation
    │   Measure decay rate → current speed
    │   Accuracy: ±15-25% with good calibration
    │
    PERIODIC (every 5 minutes):
    │
    ├── Short FFT Burst
    │   Record 5 seconds at 100Hz = 500 samples
    │   Compute FFT, look for vortex shedding peak: f = 0.2 × v / D
    │   Accuracy: ±30-50% but independent estimate
    │
    FUSION:
    │
    └── Weighted Average
        v_final = w₁×v_rms + w₂×v_logdec + w₃×v_fft
        Combined accuracy: ±10-20%

    Practical categories easily distinguished:
        Calm:       0 — 0.3 m/s     ✅
        Gentle:     0.3 — 1.0 m/s   ✅
        Moderate:   1.0 — 2.0 m/s   ✅
        Fast:       2.0 — 3.5 m/s   ✅
        Dangerous:  > 3.5 m/s       ✅

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SECTION 6: COMPLETE PHYSICS MODEL (FROM THE SIMULATION)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

A detailed physics simulation has been built to validate this system. Here is
the complete physics model it implements. Any AI working on this system should
understand these equations as they represent the ground truth of expected
behavior.

6.1 STATE VARIABLES

    The buoy has 6 degrees of freedom (2D simulation):

        S.x   — horizontal position (m)
        S.y   — vertical position (m, 0 = riverbed)
        S.a   — tilt angle (rad, 0 = upright)
        S.vx  — horizontal velocity (m/s)
        S.vy  — vertical velocity (m/s)
        S.w   — angular velocity (rad/s)

    The anchor is fixed at (anchorX, anchorY) = (0, 0) on the riverbed.

6.2 FORCES

    The simulation computes 5 primary forces and their torques each timestep:

    A. GRAVITY
        F_grav = (0, -m × g)
        Applied at: Center of Mass
        Torque: τ_g = (CoM.x - S.x) × F_grav.y - (CoM.y - S.y) × F_grav.x

    B. BUOYANCY
        F_buoy = (0, +ρ_water × g × V_submerged)
        Applied at: Center of Buoyancy (centroid of submerged volume)
        Torque: τ_b = (CoB.x - S.x) × F_buoy.y - (CoB.y - S.y) × F_buoy.x

        V_submerged is computed numerically by dividing the capsule into 40
        horizontal slices and checking which slices are below the water surface.

    C. WATER DRAG
        Relative velocity: v_rel = v_flow - v_buoy
        Drag magnitude: F_d = ½ × ρ_water × |v_rel|² × Cd × A_proj × roughness × viscosity_mult
        Direction: along v_rel
        Applied at: Center of Pressure (centroid of submerged projected area)

        Cd varies with angle of attack:
            Cd = 0.4 × |dot(axis, flow)| + 1.1 × (1 - |dot(axis, flow)|)
            → 0.4 when flow is along the capsule axis (streamlined)
            → 1.1 when flow is perpendicular (bluff body)

        Projected area also varies with angle:
            A_proj = A_end × |dot| + A_side × (1 - |dot|)
            → Small when flow hits the end cap
            → Large when flow hits the side

    D. AIR DRAG
        Acts on the portion above water. Same form as water drag but with
        air density (1.225 kg/m³) and reduced projected area.

    E. VISCOUS DAMPING
        Linear velocity damping: F_visc = -damping × viscosity × f_sub × v
        Angular damping: τ_visc = -ang_damp × viscosity × f_sub × ω

    F. TETHER TENSION
        Two modes depending on tether elasticity:

        INELASTIC TETHER (elasticity < 0.005):
            If distance(buoy_attach, anchor) > L:
                Status: TAUT
                Tether acts as a rigid constraint.
                Compute the constraint force needed to prevent extension:
                    Position correction: spring-like term (k=800)
                    Velocity correction: damping term (k=80)
                    Force floor: max(0, ...) — tether can only pull, not push

                After integration, enforce constraint by:
                    1. Project buoy back to distance L from anchor
                    2. Remove outward velocity component

            If distance ≤ L:
                Status: SLACK
                Tension = 0

        ELASTIC TETHER (elasticity > 0.005):
            If distance > L:
                Status: ELASTIC
                Extension: ext = distance - L
                Force: F = k × ext  (k = elasticity × 2000)
                Direction: toward anchor

    G. TETHER DRAG
        When tether is taut and submerged, the tether line itself has drag:
            F_tether_drag = ½ × ρ × v² × 1.2 × (0.005 × tether_length)

6.3 EFFECTIVE MASS (ADDED MASS)

    When a body accelerates through fluid, it must also accelerate the
    fluid around it. This is modeled as "added mass":

        Displaced mass: m_disp = ρ_water × V_submerged
        Added mass: m_add = added_mass_coeff × m_disp
        Effective mass: m_eff = m + m_add

    Similarly for rotation:
        Effective I: I_eff = I + added_mass_coeff × m_disp × (H_cap²/12)

    The added mass coefficient is typically 0.5 — 1.0 for a cylinder.

6.4 WATER SURFACE

    The water surface varies with position and time:

        y_water(x, t) = waterLevel + waveAmp × sin(2πx/2 - 2πt/wavePer)

    This models a traveling wave with configurable amplitude and period.

6.5 FLOW VELOCITY

    The water current is uniform with configurable speed and angle:

        v_flow.x = currentSpeed × cos(streamAngle)
        v_flow.y = -currentSpeed × sin(streamAngle)

    streamAngle > 0 means the flow has a downward component (river flowing
    downhill). This is important for steep channels and rapids.

6.6 STABILITY ANALYSIS

    The simulation computes metacentric height (GM):

        I_wp = π × r⁴ / 4                     (second moment of waterplane area)
        BM = I_wp / V_submerged                (distance from CoB to metacenter)
        GM = BM - (KG - KB)                    (metacentric height)

    where KG = height of CoM, KB = height of CoB.

        GM > 0 → STABLE (buoy self-rights when tilted)
        GM < 0 → UNSTABLE (buoy capsizes)

    Righting moment: M_right = ρ_water × g × V_sub × GM × sin(θ)

6.7 INTEGRATION

    The simulation uses Euler integration with small timesteps:

        dt_sim = dt_frame × simSpeed
        steps = min(ceil(dt_sim / 0.001), 60)
        dt_step = dt_sim / steps

    Each step:
        S.vx += (F_net.x / m_eff) × dt_step
        S.vy += (F_net.y / m_eff) × dt_step
        S.w  += (τ_net / I_eff) × dt_step
        S.x  += S.vx × dt_step
        S.y  += S.vy × dt_step
        S.a  += S.w × dt_step

    Then constraint enforcement:
        - If inelastic tether and distance > L: project back
        - If buoy bottom touches riverbed: bounce
        - Wrap angle to [-π, π]

6.8 INITIAL CONDITIONS

    On reset:
        Anchor at (0, 0) (riverbed)
        Capsule density computed
        If ρ_cap < ρ_water (floats):
            Submersion fraction: f = ρ_cap / ρ_water
            Initial y = waterLevel - (1 - f) × H_cap / 2
        If ρ_cap ≥ ρ_water (sinks):
            Initial y = waterLevel
        Initial angle = configurable
        All velocities = 0

6.9 SIMULATION ENVIRONMENT PRESETS

    The simulation includes presets modeling real-world conditions:

    Still Pond:        v=0, no waves, H=1.5m, ρ=1000
    Gentle Stream:     v=0.3, small waves (2cm, 3s period)
    Moderate River:    v=1.0, θ_stream=2°, waves (5cm, 2s)
    Fast River:        v=2.0, θ_stream=5°, waves (7cm, 1.5s)
    Steep Rapids:      v=3.0, θ_stream=15°, waves (12cm, 1s)
    Waterfall Approach: v=4.0, θ_stream=30°, waves (15cm, 0.8s)
    Drainage Channel:  v=2.0, θ_stream=20°, no waves
    Flood High Water:  v=3.0, H=2.5m, waves (12cm, 1.2s)
    Flood Low Fast:    v=4.0, θ_stream=10°, H=0.5m
    Dead Calm:         v=0, no waves, H=1.0m
    Salt Water Harbor: v=0.5, ρ=1025, waves (3cm, 3s)
    Muddy Flood:       v=2.5, ρ=1080, viscosity×2, waves (8cm, 1.5s)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SECTION 7: SYSTEM OPERATING MODES — COMPLETE DECISION TREE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

7.1 MODE 1: TAUT TETHER (TRIGONOMETRIC MODE) — PRIMARY

    Conditions: H < L, current speed sufficient to make tether taut
    Detection: lateral acceleration > threshold, tilt angle > 3°
    Measurement: H = L × cos(θ)
    Accuracy: ±1-5 cm (depends on θ and sensor fusion quality)
    Additional data: current speed via log decrement

7.2 MODE 2: FLOOD (PRESSURE MODE)

    Conditions: H > L, buoy submerged
    Detection: pressure significantly above atmospheric
    Measurement: H = L + (P - P_atm) / (ρ × g)
    Accuracy: ±1-2 cm (pressure sensors are very accurate)
    Alert: FLOOD WARNING triggered immediately

7.3 MODE 3: SLACK TETHER (ESTIMATION MODE)

    Conditions: H < L, current too weak to tension tether
    Detection: lateral acceleration ≈ 0, tilt ≈ 0
    Measurement: Cannot compute exact H.
        Report: "Water level is below flood threshold (H < L)"
        If pressure sensor has sufficient resolution:
            Can estimate H from small pressure differences (low accuracy)
        Overall: system reports SAFE but with reduced precision

7.4 MODE TRANSITION LOGIC

    SLACK → TAUT:  Current increases, lateral accel crosses threshold
    TAUT → SLACK:  Current decreases, lateral accel drops below threshold
    TAUT → FLOOD:  Water rises above L, pressure increases, buoy submerges
    FLOOD → TAUT:  Water drops below L, pressure returns to near atmospheric
    SLACK → FLOOD: Water rises rapidly, pressure jumps (possible in flash flood)
    FLOOD → SLACK: Water drops rapidly and current dies (rare)

    Hysteresis should be applied to prevent mode chattering:
        SLACK→TAUT threshold: 0.20 m/s² lateral accel
        TAUT→SLACK threshold: 0.10 m/s² lateral accel

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SECTION 8: KEY EQUATIONS SUMMARY
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    WATER LEVEL (taut mode):
        H = L × cos(θ)

    WATER LEVEL (flood mode):
        H = L + (P_measured - P_atm) / (ρ_water × g)

    TILT ANGLE (sensor fusion):
        θ = α × (θ_prev + ω × dt) + (1 - α) × atan2(a_x, a_z)

    CAPSULE VOLUME:
        V = π × r² × (H_cap - 2r) + (4/3) × π × r³

    CAPSULE DENSITY:
        ρ_cap = m / V

    BUOYANCY FORCE:
        F_b = ρ_water × g × V_submerged

    GRAVITY FORCE:
        F_g = m × g

    NET BUOYANCY:
        F_net = (ρ_water × V_submerged - m) × g

    SUBMERSION FRACTION (static equilibrium):
        f_sub = ρ_cap / ρ_water (if floating)

    DRAG FORCE:
        F_d = ½ × ρ_water × |v_rel|² × Cd × A_proj

    CURRENT SPEED (log decrement):
        v = 2γ × (m + m_added) / (ρ_water × Cd × A)

    LOG DECREMENT:
        δ = (1/N) × ln(A₁ / A_{N+1})
        γ = δ × f_oscillation

    METACENTRIC HEIGHT:
        GM = (π × r⁴) / (4 × V_sub) - (y_CoM - y_CoB)

    MOMENT OF INERTIA:
        I = (1/12) × m × (3r² + H_cap²) + m × offset²

    WATER LEVEL ERROR FROM ANGLE ERROR:
        ΔH = L × sin(θ) × Δθ

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SECTION 9: VISUALIZATION AND MONITORING (FROM SIMULATION)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

The simulation renders and tracks the following in real time:

    VISUAL ELEMENTS:
        - River cross-section with water surface (wave animation)
        - Riverbed with ground line
        - Anchor point on riverbed (cross marker)
        - Tether line (dashed when slack, solid when taut, colored when elastic)
        - Capsule buoy (orange body with wet/dry shading)
        - Center of Mass (white dot)
        - Center of Buoyancy (green dot)
        - Tether attachment point (yellow dot)
        - Force arrows: Gravity (red), Buoyancy (green), Drag (blue), Tension (yellow)
        - Tilt angle arc indicator
        - Flow direction arrow with speed label
        - Flow visualization particles

    THREE TIME-SERIES GRAPHS:
        Graph 1: Tilt Angle (°) vs Time — range -90° to +90°
        Graph 2: Position (m) vs Time — capsule Y and water surface Y
        Graph 3: Forces (N) vs Time — Buoyancy, Drag, Tension, Gravity

    NUMERICAL READOUTS (updated 6.67 Hz):
        Capsule: position, velocity, speed, tilt, angular velocity, % submerged,
                 density, floating status, volume
        Forces: gravity, buoyancy, drag (with components), tension, net force,
                net torque
        Tether: status (TAUT/SLACK/ELASTIC), force, angle
        Water: flow Vx, flow Vy, water type (Fresh/Salt/Muddy)
        Stability: metacentric height, righting moment, stable (YES/NO)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SECTION 10: KNOWN LIMITATIONS AND EDGE CASES
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

10.1 SLACK TETHER IN CALM WATER
    Problem: Cannot measure H when tether is slack (see Section 4)
    Mitigation: Dual-mode sensing, drag vane, report "safe" status

10.2 ANGLE ACCURACY AT LOW WATER
    Problem: When H << L, θ is large, and ΔH = L×sin(θ)×Δθ is maximized.
    Also, cos(θ) is small and changes slowly — low sensitivity.
    Mitigation: Accept reduced accuracy at low water (not critical for flood warning)

10.3 DEBRIS ENTANGLEMENT
    Problem: Floating debris can wrap around the tether or buoy.
    Effect: Changes effective drag area, can pull buoy underwater.
    Detection: Sudden change in pressure, anomalous angle readings.
    Mitigation: Smooth capsule shape, debris guard, anomaly detection in software.

10.4 SENSOR DRIFT AND CALIBRATION
    Problem: MPU6050 gyro drifts, accelerometer offset changes with temperature.
    Mitigation: DMP sensor fusion, periodic recalibration, temperature compensation.

10.5 WAVE-INDUCED NOISE
    Problem: Waves cause rapid oscillations in angle and acceleration.
    Mitigation: Low-pass filtering, averaging over wave periods, use wave
    period as additional data rather than noise.

10.6 RIVERBED EROSION/SEDIMENTATION
    Problem: Anchor position may change if riverbed erodes or sediment buries it.
    Effect: Changes effective L (distance from anchor to surface).
    Mitigation: Periodic manual verification, pressure baseline monitoring.

10.7 EXTREME CURRENTS
    Problem: At very high current (> 3-4 m/s), drag forces may exceed
    buoyancy and pull the buoy under even when H < L.
    Detection: Pressure sensor shows submersion, but angle doesn't indicate H > L.
    Mitigation: Distinguish "current submersion" from "flood submersion" using
    the relationship between angle, pressure, and expected H.

10.8 TEMPERATURE EFFECTS ON WATER DENSITY
    Problem: Water density varies with temperature (996-1000 kg/m³ for fresh).
    Effect: Small change in buoyancy, negligible for flood monitoring.
    Mitigation: Use temperature sensor to compensate if needed.

10.9 ICE FORMATION
    Problem: Ice can freeze the buoy in place, encase the tether.
    Effect: System becomes non-functional.
    Mitigation: Heater element, or accept seasonal downtime in cold climates.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SECTION 11: HARDWARE COMPONENT LIST
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    SENSORS:
        - MPU6050: 3-axis gyroscope + 3-axis accelerometer (I2C, built-in DMP)
        - Pressure sensor: MS5803-14BA or similar (waterproof, I2C)

    MICROCONTROLLER:
        - ESP32 or Arduino-compatible (for computation, communication, power management)

    COMMUNICATION:
        - LoRa module (for long-range wireless data transmission to base station)
        - OR cellular modem (for remote areas with cell coverage)

    POWER:
        - Rechargeable LiPo battery (placed at bottom for ballast)
        - Optional: small solar panel on top of buoy

    STRUCTURE:
        - Waterproof capsule housing (PVC pipe + end caps, sealed)
        - Tether: stainless steel cable or high-strength synthetic rope
        - Anchor: heavy weight or screw-in riverbed anchor
        - Optional: drag vane (flat plate at bottom)

    BALLAST:
        - Lead weights or steel shot at the bottom of the capsule
        - Batteries positioned at the bottom

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SECTION 12: INSTRUCTIONS FOR THE AI
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

When answering questions about this system:

1. ALWAYS consider which MODE the system is in (TAUT / SLACK / FLOOD) before
   applying any equation.

2. NEVER use H = L × cos(θ) when the tether is slack. This equation is ONLY
   valid when the tether is taut.

3. ALWAYS remember that the tether length L is a DESIGN CONSTANT equal to the
   flood threshold height. It does not change during operation.

4. When discussing angle measurement, ALWAYS mention sensor fusion. Raw gyro
   or raw accelerometer alone are insufficient.

5. When discussing current speed measurement, the LOG DECREMENT METHOD is the
   primary recommended approach. It exploits the damped pendulum dynamics of
   the tethered buoy.

6. The pressure sensor serves THREE purposes:
   a. Flood detection (submersion)
   b. Extended flood-level measurement (H > L)
   c. Mode detection (floating vs. submerged vs. dry)

7. The SLACK TETHER is the biggest weakness. When discussing system reliability,
   always address how slack conditions are handled.

8. The buoy structure MUST have:
   - Bottom tether attachment
   - Low center of mass
   - Elongated shape
   - Positive buoyancy (ρ_cap < ρ_water)

9. The simulation uses a 2D rigid-body physics model with Euler integration.
   Forces include gravity, buoyancy, water drag, air drag, viscous damping,
   and tether tension (both rigid and elastic). The simulation validates the
   concept and allows parameter exploration.

10. When generating code for the actual embedded system, use the MPU6050 DMP
    library for angle estimation, implement the three-mode decision tree, and
    include proper filtering (complementary or Kalman filter as backup).

<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>River Capsule Physics Simulation</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0a0a1a;color:#e0e0e0;font-family:'Segoe UI',Tahoma,sans-serif;font-size:12px;overflow:hidden;height:100vh}
#mainLayout{display:flex;height:100vh}
#simArea{flex:1;display:flex;flex-direction:column;min-width:0}
#simCanvas{flex:1;display:block}
#graphArea{display:flex;height:130px;border-top:2px solid #333}
#graphArea canvas{flex:1;display:block}
#graphArea canvas+canvas{border-left:1px solid #333}
#cp{width:310px;background:#16183a;overflow-y:auto;border-left:2px solid #444;padding:0}
#cp::-webkit-scrollbar{width:6px}
#cp::-webkit-scrollbar-thumb{background:#555;border-radius:3px}
.sec{border-bottom:1px solid #333}
.sec-h{background:#1e2050;padding:7px 10px;cursor:pointer;font-weight:bold;font-size:12px;user-select:none;display:flex;justify-content:space-between}
.sec-h:hover{background:#262870}
.sec-b{padding:6px 10px;display:none;background:#121430}
.sec-b.open{display:block}
.cr{display:flex;align-items:center;margin-bottom:5px;gap:4px}
.cr label{flex:0 0 95px;font-size:11px;color:#aab}
.cr input[type=range]{flex:1;height:16px;-webkit-appearance:none;background:transparent;cursor:pointer}
.cr input[type=range]::-webkit-slider-runnable-track{height:5px;background:#334;border-radius:3px}
.cr input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:14px;height:14px;background:#5ae;border-radius:50%;margin-top:-5px}
.cr .vl{flex:0 0 60px;text-align:right;font-size:11px;color:#8cf;font-family:monospace}
.cr select{flex:1;background:#223;color:#ddd;border:1px solid #445;padding:3px;border-radius:3px;font-size:11px}
.btn-row{display:flex;gap:5px;margin:5px 0;flex-wrap:wrap}
.btn{padding:5px 10px;background:#1e2050;color:#ddd;border:1px solid #445;border-radius:4px;cursor:pointer;font-size:11px}
.btn:hover{background:#2a3070}
.rdout{font-family:monospace;font-size:10.5px;line-height:1.6;padding:2px 0}
.rdout .lb{color:#789;display:inline-block;width:95px}
.rdout .vr{color:#8cf}.rdout .vg{color:#8f8}.rdout .vy{color:#ff8}.rdout .vn{color:#f88}
.rg{margin-bottom:3px;padding-bottom:3px;border-bottom:1px solid #222}
.rg-t{color:#9ab;font-weight:bold;font-size:11px;margin-bottom:2px}
</style>
</head>
<body>
<div id="mainLayout">
<div id="simArea">
<canvas id="simCanvas"></canvas>
<div id="graphArea">
<canvas id="g1"></canvas>
<canvas id="g2"></canvas>
<canvas id="g3"></canvas>
</div>
</div>
<div id="cp"></div>
</div>
<script>
"use strict";

// ===================== PARAMETERS =====================
const P = {
  currentSpeed:0.5, streamAngle:0, waterLevel:1.0, waterDensity:1000,
  viscMult:1.0, waveOn:false, waveAmp:0.05, wavePer:2.0,
  capH:0.30, capD:0.08, capM:0.150,
  roughness:1.0, ballast:0.5, initAngle:0,
  tethLen:1.0, tethElast:0, tethAttach:0,
  damping:1.0, angDamp:0.02, addedMass:1.0, simSpeed:1.0,
  showForces:true, showFlow:true
};

// ===================== STATE =====================
let S={x:0,y:0,a:0,vx:0,vy:0,w:0};
let simT=0, paused=false;
const G=9.81, AIR_RHO=1.225;
let anchorX=0, anchorY=0;

// Computed per-frame
let F_grav={x:0,y:0},F_buoy={x:0,y:0},F_drag={x:0,y:0},F_tens={x:0,y:0},F_net={x:0,y:0};
let netTorque=0, pctSub=0, tethStat='SLACK', tethForce=0, tethAngV=0, tethExt=0;
let cobW={x:0,y:0}, copW={x:0,y:0}, comW={x:0,y:0};
let metaH=0, rightM=0, stab=true;

// History for graphs
const hist={t:[],ang:[],py:[],ws:[],fb:[],fd:[],ft:[],fg:[]};
const HIST_DUR=30;

// Flow particles
let particles=[];

// FPS
let fps=60, lastTS=0, fpsC=0, fpsT=0;

// ===================== HELPERS =====================
function capR(){return P.capD/2}
function capVol(){
  const r=capR(),h=P.capH,cyl=Math.max(0,h-2*r);
  return Math.PI*r*r*cyl+(4/3)*Math.PI*r*r*r;
}
function capDensity(){return P.capM/capVol()}
function comOff(){return(P.ballast-0.5)*P.capH*0.4}
function moi(){
  const r=capR(),h=P.capH,m=P.capM,off=comOff();
  return(1/12)*m*(3*r*r+h*h)+m*off*off;
}
function ptOnAxis(d){
  return{x:S.x+d*Math.sin(S.a),y:S.y+d*Math.cos(S.a)};
}
function tethAttPt(){
  const d=-P.capH/2+P.tethAttach*P.capH;
  return ptOnAxis(d);
}
function comPt(){return ptOnAxis(comOff())}
function waterY(x,t){
  let lv=P.waterLevel;
  if(P.waveOn&&P.waveAmp>0){
    lv+=P.waveAmp*Math.sin(2*Math.PI*x/2-2*Math.PI*t/P.wavePer);
  }
  return lv;
}
function flowVel(){
  const a=P.streamAngle*Math.PI/180;
  return{x:P.currentSpeed*Math.cos(a),y:-P.currentSpeed*Math.sin(a)};
}

// ===================== SUBMERGED VOLUME =====================
function subProps(){
  const h=P.capH, r=capR(), N=40;
  let totV=0,subV=0,scx=0,scy=0,pcx=0,pcy=0,pw=0,subA=0,totA=0;
  for(let i=0;i<N;i++){
    const f=(i+0.5)/N;
    const d=-h/2+f*h;
    const pt=ptOnAxis(d);
    const wy=waterY(pt.x,simT);
    const de=Math.min(f*h,(1-f)*h);
    let sr=de<r?Math.sqrt(Math.max(0,r*r-(r-de)*(r-de))):r;
    const sv=Math.PI*sr*sr*(h/N);
    const sw=2*sr*(h/N);
    totV+=sv; totA+=sw;
    if(pt.y<=wy){
      const depth=wy-pt.y;
      subV+=sv; scx+=pt.x*sv; scy+=pt.y*sv;
      subA+=sw;
      pcx+=pt.x*sw; pcy+=pt.y*sw; pw+=sw;
    }
  }
  if(subV>1e-12){scx/=subV;scy/=subV;}else{const c=ptOnAxis(0);scx=c.x;scy=c.y;}
  if(pw>1e-12){pcx/=pw;pcy/=pw;}else{pcx=scx;pcy=scy;}
  return{subV,totV,frac:totV>1e-12?subV/totV:0,cobX:scx,cobY:scy,copX:pcx,copY:pcy,subA,totA};
}

// ===================== DRAG COEFFICIENT =====================
function dragCd(flowAng){
  const ax=Math.sin(S.a),ay=Math.cos(S.a);
  const fx=Math.cos(flowAng),fy=-Math.sin(flowAng);
  const dot=Math.abs(ax*fx+ay*fy);
  return 0.4*dot+1.1*(1-dot);
}
function projArea(sp,flowAng){
  const ax=Math.sin(S.a),ay=Math.cos(S.a);
  const fx=Math.cos(flowAng),fy=-Math.sin(flowAng);
  const dot=Math.abs(ax*fx+ay*fy);
  const cross=1-dot;
  const r=capR();
  const alignA=Math.PI*r*r*sp.frac;
  return alignA*dot+sp.subA*cross;
}

// ===================== PHYSICS STEP =====================
function physStep(dt){
  const h=P.capH, r=capR(), m=P.capM;
  const cOff=comOff(), I=moi();
  const sp=subProps();
  pctSub=sp.frac*100;
  cobW={x:sp.cobX,y:sp.cobY};
  copW={x:sp.copX,y:sp.copY};
  comW=comPt();

  const dispM=P.waterDensity*sp.subV;
  const addM=P.addedMass*dispM;
  const effM=m+addM;
  const effI=I+P.addedMass*dispM*(h*h/12);

  let fx=0,fy=0,torq=0;

  // A. Gravity
  const Fg={x:0,y:-m*G};
  fx+=Fg.x; fy+=Fg.y;
  const cw=comPt();
  torq+=(cw.x-S.x)*Fg.y-(cw.y-S.y)*Fg.x;
  F_grav={x:Fg.x,y:Fg.y};

  // B. Buoyancy
  const Fb={x:0,y:P.waterDensity*G*sp.subV};
  fx+=Fb.x; fy+=Fb.y;
  torq+=(sp.cobX-S.x)*Fb.y-(sp.cobY-S.y)*Fb.x;
  F_buoy={x:Fb.x,y:Fb.y};

  // C. Water drag
  const fl=flowVel();
  let Fd={x:0,y:0};
  if(sp.frac>0.001){
    const vrx=fl.x-S.vx, vry=fl.y-S.vy;
    const vrm=Math.sqrt(vrx*vrx+vry*vry);
    if(vrm>1e-7){
      const fAng=Math.atan2(-vry,vrx);
      const Cd=dragCd(fAng);
      const Ap=projArea(sp,fAng);
      const Fm=0.5*P.waterDensity*vrm*vrm*Cd*Ap*P.roughness*P.viscMult;
      Fd.x=Fm*vrx/vrm; Fd.y=Fm*vry/vrm;
      fx+=Fd.x; fy+=Fd.y;
      torq+=(sp.copX-S.x)*Fd.y-(sp.copY-S.y)*Fd.x;
    }
  }
  F_drag={x:Fd.x,y:Fd.y};

  // D. Air drag
  const airFr=1-sp.frac;
  if(airFr>0.01){
    const vax=-S.vx, vay=-S.vy;
    const vam=Math.sqrt(vax*vax+vay*vay);
    if(vam>1e-6){
      const Ap2=sp.totA*airFr*0.3;
      const Fda=0.5*AIR_RHO*vam*vam*1.0*Ap2;
      fx+=Fda*vax/vam; fy+=Fda*vay/vam;
    }
  }

  // E. Viscous damping
  const df=P.damping*P.viscMult*sp.frac*5;
  fx-=df*S.vx; fy-=df*S.vy;
  torq-=P.angDamp*P.viscMult*sp.frac*100*S.w;

  // F. Tether
  const ap=tethAttPt();
  const tdx=ap.x-anchorX, tdy=ap.y-anchorY;
  const dist=Math.sqrt(tdx*tdx+tdy*tdy);
  let Ft={x:0,y:0};
  tethExt=0;

  if(P.tethElast<0.005){
    if(dist>P.tethLen){
      tethStat='TAUT';
      const nx=-tdx/dist, ny=-tdy/dist;
      const rax=ap.x-S.x,ray=ap.y-S.y;
      const vax2=S.vx+S.w*(-ray), vay2=S.vy+S.w*rax;
      const vOut=-(vax2*nx+vay2*ny);
      const fOut=-(fx*nx+fy*ny);
      const posCorr=(dist-P.tethLen)*800;
      const velCorr=vOut>0?vOut*80:0;
      const tmag=Math.max(0,fOut+posCorr+velCorr);
      Ft.x=tmag*nx; Ft.y=tmag*ny;
      tethForce=tmag;
      fx+=Ft.x; fy+=Ft.y;
      torq+=(ap.x-S.x)*Ft.y-(ap.y-S.y)*Ft.x;
    } else { tethStat='SLACK'; tethForce=0; }
  } else {
    if(dist>P.tethLen){
      tethStat='ELASTIC'; tethExt=dist-P.tethLen;
      const k=P.tethElast*2000;
      const nx=-tdx/dist,ny=-tdy/dist;
      const tmag=k*tethExt;
      Ft.x=tmag*nx; Ft.y=tmag*ny;
      tethForce=tmag;
      fx+=Ft.x; fy+=Ft.y;
      torq+=(ap.x-S.x)*Ft.y-(ap.y-S.y)*Ft.x;
    } else { tethStat='SLACK'; tethForce=0; }
  }
  F_tens={x:Ft.x,y:Ft.y};

  // G. Tether drag
  if(tethStat!=='SLACK'&&sp.frac>0){
    const mx=(anchorX+ap.x)/2, my=(anchorY+ap.y)/2;
    if(my<waterY(mx,simT)){
      const vtx=fl.x-S.vx*0.5, vty=fl.y-S.vy*0.5;
      const vtm=Math.sqrt(vtx*vtx+vty*vty);
      if(vtm>1e-6){
        const ta=0.005*dist;
        const ftd=0.5*P.waterDensity*vtm*vtm*1.2*ta*P.viscMult;
        fx+=ftd*vtx/vtm*0.5; fy+=ftd*vty/vtm*0.5;
      }
    }
  }

  F_net={x:fx,y:fy}; netTorque=torq;
  if(dist>0.01) tethAngV=Math.atan2(tdx,tdy)*180/Math.PI; else tethAngV=0;

  // Stability
  if(sp.frac>0.01&&sp.frac<0.99){
    const Iwp=Math.PI*r*r*r*r/4;
    const BM=sp.subV>1e-12?Iwp/sp.subV:0;
    const KB=sp.cobY;
    const KG=S.y+cOff;
    metaH=BM-(KG-KB);
    rightM=P.waterDensity*G*sp.subV*metaH*Math.sin(S.a);
    stab=metaH>0;
  } else { metaH=0;rightM=0;stab=sp.frac>0.5; }

  // Integrate
  S.vx+=fx/effM*dt;
  S.vy+=fy/effM*dt;
  S.w+=torq/effI*dt;
  S.x+=S.vx*dt;
  S.y+=S.vy*dt;
  S.a+=S.w*dt;

  // Constraint enforcement
  if(P.tethElast<0.005){
    const ap2=tethAttPt();
    const d2x=ap2.x-anchorX,d2y=ap2.y-anchorY;
    const d2=Math.sqrt(d2x*d2x+d2y*d2y);
    if(d2>P.tethLen+0.001){
      const ex=d2-P.tethLen;
      const nx2=d2x/d2,ny2=d2y/d2;
      S.x-=nx2*ex*0.9;
      S.y-=ny2*ex*0.9;
      const vout2=S.vx*nx2+S.vy*ny2;
      if(vout2>0){S.vx-=vout2*nx2;S.vy-=vout2*ny2;}
    }
  }

  // River bed
  const bot=ptOnAxis(-h/2);
  if(bot.y<0.001){S.y+=(0.001-bot.y);if(S.vy<0)S.vy*=-0.2;}
  const top2=ptOnAxis(h/2);
  if(top2.y<0.001){S.y+=(0.001-top2.y);if(S.vy<0)S.vy*=-0.2;}

  while(S.a>Math.PI)S.a-=2*Math.PI;
  while(S.a<-Math.PI)S.a+=2*Math.PI;
  simT+=dt;
}

// ===================== PARTICLES =====================
function initParts(){
  particles=[];
  for(let i=0;i<250;i++){
    particles.push({x:(Math.random()-0.5)*8,y:Math.random()*3,sz:1+Math.random()*2,al:0.15+Math.random()*0.35});
  }
}
function updateParts(dt){
  const fl=flowVel();
  for(let p of particles){
    const spd=0.5+0.5*Math.min(1,p.y/Math.max(0.1,P.waterLevel));
    p.x+=fl.x*dt*spd;
    p.y+=fl.y*dt*0.3;
    const wy=waterY(p.x,simT);
    if(p.y>wy)p.y=Math.random()*wy*0.95;
    if(p.y<0.01)p.y=Math.random()*P.waterLevel*0.9;
    if(p.x>4.5)p.x-=9;
    if(p.x<-4.5)p.x+=9;
  }
}

// ===================== RESET =====================
function resetSim(){
  anchorX=0; anchorY=0;
  const cd=capDensity(), wy=P.waterLevel;
  let sy;
  if(cd<P.waterDensity){
    const sf=cd/P.waterDensity;
    sy=wy-(1-sf)*P.capH/2;
  } else { sy=wy; }
  S={x:anchorX,y:sy,a:P.initAngle*Math.PI/180,vx:0,vy:0,w:0};
  simT=0;
  for(let k in hist)hist[k]=[];
  initParts();
}

// ===================== RENDER =====================
const simC=document.getElementById('simCanvas');
const ctx=simC.getContext('2d');
const g1=document.getElementById('g1'),g2=document.getElementById('g2'),g3=document.getElementById('g3');
const c1=g1.getContext('2d'),c2=g2.getContext('2d'),c3=g3.getContext('2d');

function resize(){
  const sa=document.getElementById('simArea');
  const ga=document.getElementById('graphArea');
  simC.width=sa.clientWidth;
  simC.height=sa.clientHeight-ga.offsetHeight;
  const gw=Math.floor(ga.clientWidth/3);
  const gh=ga.offsetHeight;
  g1.width=gw;g1.height=gh;
  g2.width=gw;g2.height=gh;
  g3.width=ga.clientWidth-2*gw;g3.height=gh;
}
window.addEventListener('resize',resize);

function render(){
  const W=simC.width,H=simC.height;
  if(W<10||H<10)return;
  ctx.clearRect(0,0,W,H);

  // View: 5m wide, anchor near bottom
  const vw=5, sc=W/vw, vh=H/sc;
  const camX=anchorX, camY=vh*0.3;
  function toS(wx,wy){return{x:W/2+(wx-camX)*sc,y:H-(wy-(camY-vh/2))*sc}}
  function toSY(wy){return H-(wy-(camY-vh/2))*sc}
  function toSX(wx){return W/2+(wx-camX)*sc}

  // Sky
  const skyG=ctx.createLinearGradient(0,0,0,H);
  skyG.addColorStop(0,'#0c1028');skyG.addColorStop(0.5,'#1a2848');skyG.addColorStop(1,'#2a4868');
  ctx.fillStyle=skyG;ctx.fillRect(0,0,W,H);

  // River bed
  const bedY=toSY(0);
  const bedG=ctx.createLinearGradient(0,bedY,0,H);
  bedG.addColorStop(0,'#5a4a2a');bedG.addColorStop(1,'#2a1a0a');
  ctx.fillStyle=bedG;ctx.fillRect(0,Math.min(bedY,H),W,H);

  // Bed line
  ctx.strokeStyle='#7a6a3a';ctx.lineWidth=3;
  ctx.beginPath();ctx.moveTo(0,bedY);ctx.lineTo(W,bedY);ctx.stroke();

  // Water body
  ctx.beginPath();ctx.moveTo(0,H);
  for(let sx=0;sx<=W;sx+=4){
    const wx=(sx-W/2)/sc+camX;
    const wy=waterY(wx,simT);
    ctx.lineTo(sx,toSY(wy));
  }
  ctx.lineTo(W,H);ctx.closePath();
  const wG=ctx.createLinearGradient(0,toSY(P.waterLevel),0,bedY);
  wG.addColorStop(0,'rgba(20,60,140,0.55)');wG.addColorStop(1,'rgba(10,30,70,0.7)');
  ctx.fillStyle=wG;ctx.fill();

  // Water surface
  ctx.strokeStyle='rgba(80,170,255,0.9)';ctx.lineWidth=2.5;
  ctx.beginPath();
  for(let sx=0;sx<=W;sx+=3){
    const wx=(sx-W/2)/sc+camX;
    const wy=waterY(wx,simT);
    const sy=toSY(wy);
    sx===0?ctx.moveTo(sx,sy):ctx.lineTo(sx,sy);
  }
  ctx.stroke();

  // Flow particles
  if(P.showFlow&&P.currentSpeed>0.01){
    for(let p of particles){
      const wy2=waterY(p.x,simT);
      if(p.y>wy2+0.02)continue;
      const sp=toS(p.x,p.y);
      if(sp.x<-5||sp.x>W+5||sp.y<0)continue;
      ctx.fillStyle=`rgba(120,190,255,${p.al})`;
      ctx.beginPath();ctx.arc(sp.x,sp.y,p.sz,0,6.283);ctx.fill();
    }
  }

  // Flow arrow indicator
  if(P.currentSpeed>0.05){
    ctx.save();ctx.translate(W-80,28);
    ctx.rotate(P.streamAngle*Math.PI/180);
    ctx.strokeStyle='#4af';ctx.lineWidth=3;
    ctx.beginPath();ctx.moveTo(-30,0);ctx.lineTo(30,0);
    ctx.lineTo(20,-7);ctx.moveTo(30,0);ctx.lineTo(20,7);ctx.stroke();
    ctx.restore();
    ctx.fillStyle='#5af';ctx.font='11px monospace';
    ctx.fillText(`${P.currentSpeed.toFixed(1)}m/s @ ${P.streamAngle}°`,W-135,52);
  }

  // Anchor
  const anS=toS(anchorX,anchorY);
  ctx.fillStyle='#999';ctx.strokeStyle='#bbb';ctx.lineWidth=2;
  ctx.beginPath();ctx.arc(anS.x,anS.y,7,0,6.283);ctx.fill();ctx.stroke();
  ctx.strokeStyle='#ccc';ctx.lineWidth=2;
  ctx.beginPath();ctx.moveTo(anS.x-5,anS.y);ctx.lineTo(anS.x+5,anS.y);
  ctx.moveTo(anS.x,anS.y-5);ctx.lineTo(anS.x,anS.y+5);ctx.stroke();

  // Tether
  const ap=tethAttPt();
  const apS=toS(ap.x,ap.y);
  if(tethStat==='SLACK'){
    ctx.strokeStyle='#a86';ctx.lineWidth=2;ctx.setLineDash([5,4]);
    ctx.beginPath();
    const mx=(anS.x+apS.x)/2, my=Math.max(anS.y,apS.y)+25;
    ctx.moveTo(anS.x,anS.y);ctx.quadraticCurveTo(mx,my,apS.x,apS.y);ctx.stroke();
    ctx.setLineDash([]);
  } else {
    ctx.strokeStyle=tethStat==='ELASTIC'?'#da4':'#a86';ctx.lineWidth=2.5;
    ctx.beginPath();ctx.moveTo(anS.x,anS.y);ctx.lineTo(apS.x,apS.y);ctx.stroke();
  }

  // Capsule
  const capS=toS(S.x,S.y);
  const cH=P.capH*sc, cR=capR()*sc;
  const cylH2=Math.max(0,cH-2*cR);
  ctx.save();
  ctx.translate(capS.x,capS.y);
  ctx.rotate(-S.a);

  // Capsule path
  function capsulePath(){
    ctx.beginPath();
    ctx.arc(0,-cylH2/2,cR,Math.PI,0);
    ctx.lineTo(cR,cylH2/2);
    ctx.arc(0,cylH2/2,cR,0,Math.PI);
    ctx.closePath();
  }

  // Dry fill
  capsulePath();
  ctx.fillStyle='#e08030';ctx.fill();

  // Wet overlay using clip
  ctx.save();
  capsulePath();ctx.clip();
  // Water line in local frame
  const wlY=waterY(S.x,simT);
  const wlScreenY=toSY(wlY);
  const wlLocal=wlScreenY-capS.y;
  // The waterline in rotated local coords is tilted
  const sa2=Math.sin(S.a),ca2=Math.cos(S.a);
  ctx.fillStyle='rgba(10,40,80,0.8)';
  ctx.beginPath();
  // Waterline as a tilted line in local coords
  // lx, ly -> screen: sx = capS.x + lx*cos(-a) - ly*sin(-a)
  // screen y of waterline = wlScreenY
  // capS.y + lx*sin(-a) + ly*cos(-a) = wlScreenY
  // ly = (wlLocal + lx*sin(S.a)) / cos(S.a)   if cos(S.a) != 0
  if(Math.abs(Math.cos(S.a))>0.01){
    const wfn=lx=>(wlLocal+lx*Math.sin(S.a))/Math.cos(S.a);
    const ex=cR+10;
    ctx.moveTo(-ex,wfn(-ex));ctx.lineTo(ex,wfn(ex));
    ctx.lineTo(ex,cH/2+cR+10);ctx.lineTo(-ex,cH/2+cR+10);
    ctx.closePath();ctx.fill();
  } else {
    ctx.fillRect(-cR-10,-cH/2-cR-10,(cR+10)*2,cH+2*cR+20);
  }
  ctx.restore();

  // Outline
  capsulePath();
  ctx.strokeStyle='#222';ctx.lineWidth=2;ctx.stroke();

  // CoM dot
  const cmD=comOff()*sc;
  ctx.fillStyle='#fff';ctx.beginPath();ctx.arc(0,-cmD,4,0,6.283);ctx.fill();
  ctx.strokeStyle='#000';ctx.lineWidth=1;ctx.stroke();

  // Attach point
  const atD=(-P.capH/2+P.tethAttach*P.capH)*sc;
  ctx.fillStyle='#ff0';ctx.beginPath();ctx.arc(0,-atD,3,0,6.283);ctx.fill();

  ctx.restore();

  // CoB dot (world)
  if(pctSub>0.5){
    const cbS=toS(cobW.x,cobW.y);
    ctx.fillStyle='#0f0';ctx.beginPath();ctx.arc(cbS.x,cbS.y,4,0,6.283);ctx.fill();
    ctx.strokeStyle='#080';ctx.lineWidth=1;ctx.stroke();
  }

  // Force arrows
  if(P.showForces){
    const fsc=sc*0.03;
    const cmS=toS(comW.x,comW.y);
    function arrow(ox,oy,fx2,fy2,col,lbl){
      const m=Math.sqrt(fx2*fx2+fy2*fy2);if(m<0.002)return;
      const ln=Math.min(m*fsc,130);
      const dx2=fx2/m,dy2=fy2/m;
      const tx=ox+dx2*ln, ty=oy-dy2*ln;
      ctx.strokeStyle=col;ctx.lineWidth=2.5;
      ctx.beginPath();ctx.moveTo(ox,oy);ctx.lineTo(tx,ty);
      const an=Math.atan2(-(ty-oy),tx-ox);
      ctx.lineTo(tx-9*Math.cos(an-0.4),ty+9*Math.sin(an-0.4));
      ctx.moveTo(tx,ty);ctx.lineTo(tx-9*Math.cos(an+0.4),ty+9*Math.sin(an+0.4));
      ctx.stroke();
      ctx.fillStyle=col;ctx.font='10px monospace';
      ctx.fillText(lbl+':'+m.toFixed(2)+'N',tx+6,ty-5);
    }
    arrow(cmS.x,cmS.y,F_grav.x,F_grav.y,'#f55','G');
    if(pctSub>0.5){
      const cbS2=toS(cobW.x,cobW.y);
      arrow(cbS2.x,cbS2.y,F_buoy.x,F_buoy.y,'#4f4','B');
    }
    const dm=Math.sqrt(F_drag.x**2+F_drag.y**2);
    if(dm>0.005){const cpS=toS(copW.x,copW.y);arrow(cpS.x,cpS.y,F_drag.x,F_drag.y,'#48f','D');}
    if(tethForce>0.005){arrow(apS.x,apS.y,F_tens.x,F_tens.y,'#ff0','T');}
  }

  // Tilt angle arc
  if(Math.abs(S.a)>0.02){
    ctx.strokeStyle='rgba(255,200,50,0.6)';ctx.lineWidth=1.5;
    ctx.beginPath();
    const sa3=-Math.PI/2, ea3=-Math.PI/2+S.a;
    ctx.arc(capS.x,capS.y,28,Math.min(sa3,ea3),Math.max(sa3,ea3));ctx.stroke();
    ctx.fillStyle='#ffa';ctx.font='11px monospace';
    ctx.fillText((S.a*180/Math.PI).toFixed(1)+'°',capS.x+32,capS.y-18);
  }

  // Info overlay
  ctx.fillStyle='rgba(0,0,0,0.6)';ctx.fillRect(5,5,240,70);
  ctx.fillStyle='#ddd';ctx.font='11px monospace';
  ctx.fillText('Time: '+simT.toFixed(1)+'s  FPS: '+fps.toFixed(0),10,20);
  const cdv=capDensity();
  ctx.fillStyle=cdv<P.waterDensity?'#5f5':'#f55';
  ctx.fillText('ρcap:'+cdv.toFixed(0)+' vs ρw:'+P.waterDensity+(cdv<P.waterDensity?' FLOATS':' SINKS'),10,36);
  ctx.fillStyle='#8cf';
  ctx.fillText('Sub:'+pctSub.toFixed(1)+'%  Tilt:'+(S.a*180/Math.PI).toFixed(1)+'°',10,52);
  ctx.fillText('Tether:'+tethStat+' '+tethForce.toFixed(2)+'N',10,67);

  // Legend
  ctx.fillStyle='rgba(0,0,0,0.5)';ctx.fillRect(5,H-48,160,44);
  ctx.font='10px monospace';
  ctx.fillStyle='#fff';ctx.fillText('● CoM',10,H-35);
  ctx.fillStyle='#0f0';ctx.fillText('● CoB',10,H-22);
  ctx.fillStyle='#ff0';ctx.fillText('● Attach',10,H-9);
  ctx.fillStyle='#48f';ctx.fillText('● CoP',85,H-35);
}

// ===================== GRAPHS =====================
function recHist(){
  if(hist.t.length===0||simT-hist.t[hist.t.length-1]>0.06){
    hist.t.push(simT);
    hist.ang.push(S.a*180/Math.PI);
    hist.py.push(S.y);
    hist.ws.push(waterY(S.x,simT));
    hist.fb.push(Math.sqrt(F_buoy.x**2+F_buoy.y**2));
    hist.fd.push(Math.sqrt(F_drag.x**2+F_drag.y**2));
    hist.ft.push(tethForce);
    hist.fg.push(Math.sqrt(F_grav.x**2+F_grav.y**2));
    while(hist.t.length>0&&simT-hist.t[0]>HIST_DUR){for(let k in hist)hist[k].shift();}
  }
}
function drawG(cx,cv,title,series,yMin,yMax,zeroLine){
  const W=cv.width,H=cv.height;if(W<5||H<5)return;
  cx.fillStyle='#080818';cx.fillRect(0,0,W,H);
  const p={l:38,r:8,t:18,b:16};
  const gw=W-p.l-p.r, gh=H-p.t-p.b;
  cx.fillStyle='#889';cx.font='10px sans-serif';cx.fillText(title,p.l,13);
  cx.strokeStyle='#1a1a2a';cx.lineWidth=1;
  for(let i=0;i<=4;i++){
    const y=p.t+i/4*gh;
    cx.beginPath();cx.moveTo(p.l,y);cx.lineTo(W-p.r,y);cx.stroke();
    const v=yMax-(i/4)*(yMax-yMin);
    cx.fillStyle='#556';cx.font='9px monospace';cx.fillText(v.toFixed(1),1,y+3);
  }
  if(zeroLine&&yMin<0&&yMax>0){
    const zy=p.t+(yMax/(yMax-yMin))*gh;
    cx.strokeStyle='#444';cx.setLineDash([3,3]);
    cx.beginPath();cx.moveTo(p.l,zy);cx.lineTo(W-p.r,zy);cx.stroke();cx.setLineDash([]);
  }
  if(hist.t.length<2)return;
  const tE=hist.t[hist.t.length-1], tS=tE-HIST_DUR;
  series.forEach((s,si)=>{
    cx.strokeStyle=s.c;cx.lineWidth=1.5;
    if(s.d)cx.setLineDash(s.d);
    cx.beginPath();let st=false;
    for(let i=0;i<hist.t.length;i++){
      const x2=p.l+((hist.t[i]-tS)/HIST_DUR)*gw;
      const v=s.data[i];
      const y2=p.t+((yMax-v)/(yMax-yMin))*gh;
      st?cx.lineTo(x2,y2):cx.moveTo(x2,y2);st=true;
    }
    cx.stroke();if(s.d)cx.setLineDash([]);
    if(s.data.length>0){
      cx.fillStyle=s.c;cx.font='9px monospace';
      cx.fillText(s.l+':'+s.data[s.data.length-1].toFixed(2),W-p.r-85,p.t+11+si*11);
    }
  });
}
function renderGraphs(){
  recHist();
  drawG(c1,g1,'Tilt Angle (°)',[{data:hist.ang,c:'#f55',l:'Ang'}],-90,90,true);
  const pm=Math.max(P.waterLevel+0.5,2);
  drawG(c2,g2,'Position (m)',[{data:hist.py,c:'#fa4',l:'Cap'},{data:hist.ws,c:'#48f',l:'Wtr'}],0,pm,false);
  let fm=2;hist.fb.forEach(v=>{if(v>fm)fm=v});hist.fg.forEach(v=>{if(v>fm)fm=v});
  hist.fd.forEach(v=>{if(v>fm)fm=v});hist.ft.forEach(v=>{if(v>fm)fm=v});fm*=1.15;
  drawG(c3,g3,'Forces (N)',[
    {data:hist.fb,c:'#4f4',l:'Buoy'},{data:hist.fd,c:'#48f',l:'Drag'},
    {data:hist.ft,c:'#ff0',l:'Tens'},{data:hist.fg,c:'#f55',l:'Grav',d:[4,3]}
  ],0,fm,false);
}

// ===================== READOUTS =====================
function updRead(){
  const div=document.getElementById('rdiv');if(!div)return;
  const fl=flowVel();
  const spd=Math.sqrt(S.vx*S.vx+S.vy*S.vy);
  const cd=capDensity(), isF=cd<P.waterDensity;
  const gm=Math.sqrt(F_grav.x**2+F_grav.y**2);
  const bm=Math.sqrt(F_buoy.x**2+F_buoy.y**2);
  const dm=Math.sqrt(F_drag.x**2+F_drag.y**2);
  const nm=Math.sqrt(F_net.x**2+F_net.y**2);
  const dl=P.waterDensity<1010?'Fresh':P.waterDensity<1030?'Salt':'Muddy';
  div.innerHTML=`<div class="rg"><div class="rg-t">CAPSULE</div>
<span class="lb">Pos:</span><span class="vr">(${S.x.toFixed(3)}, ${S.y.toFixed(3)})m</span><br>
<span class="lb">Vel:</span><span class="vr">(${S.vx.toFixed(3)}, ${S.vy.toFixed(3)})m/s</span><br>
<span class="lb">Speed:</span><span class="vr">${spd.toFixed(3)}m/s</span><br>
<span class="lb">Tilt:</span><span class="vr">${(S.a*180/Math.PI).toFixed(1)}°</span><br>
<span class="lb">ω:</span><span class="vr">${(S.w*180/Math.PI).toFixed(1)}°/s</span><br>
<span class="lb">Submerged:</span><span class="vr">${pctSub.toFixed(1)}%</span><br>
<span class="lb">ρ capsule:</span><span class="${isF?'vg':'vn'}">${cd.toFixed(0)}kg/m³</span><br>
<span class="lb">Floating:</span><span class="${isF?'vg':'vn'}">${isF?'YES':'NO'}</span><br>
<span class="lb">Volume:</span><span class="vr">${(capVol()*1e6).toFixed(1)}cm³</span></div>
<div class="rg"><div class="rg-t">FORCES (N)</div>
<span class="lb">Gravity:</span><span class="vn">${gm.toFixed(3)}↓</span><br>
<span class="lb">Buoyancy:</span><span class="vg">${bm.toFixed(3)}↑</span><br>
<span class="lb">Drag:</span><span class="vr">${dm.toFixed(3)} (${F_drag.x.toFixed(3)},${F_drag.y.toFixed(3)})</span><br>
<span class="lb">Tension:</span><span class="vy">${tethForce.toFixed(3)}</span><br>
<span class="lb">Net:</span><span class="vr">${nm.toFixed(3)} (${F_net.x.toFixed(3)},${F_net.y.toFixed(3)})</span><br>
<span class="lb">Torque:</span><span class="vr">${netTorque.toFixed(4)}N·m</span></div>
<div class="rg"><div class="rg-t">TETHER</div>
<span class="lb">Status:</span><span class="${tethStat==='TAUT'?'vy':tethStat==='ELASTIC'?'vn':'vg'}">${tethStat}</span><br>
<span class="lb">Force:</span><span class="vy">${tethForce.toFixed(3)}N</span><br>
<span class="lb">Angle:</span><span class="vr">${tethAngV.toFixed(1)}°</span></div>
<div class="rg"><div class="rg-t">WATER</div>
<span class="lb">Flow Vx:</span><span class="vr">${fl.x.toFixed(2)}m/s</span><br>
<span class="lb">Flow Vy:</span><span class="vr">${fl.y.toFixed(2)}m/s</span><br>
<span class="lb">Type:</span><span class="vr">${dl} (${P.waterDensity})</span></div>
<div class="rg"><div class="rg-t">STABILITY</div>
<span class="lb">Metacentric:</span><span class="${metaH>0?'vg':'vn'}">${metaH.toFixed(4)}m</span><br>
<span class="lb">Righting M:</span><span class="vr">${rightM.toFixed(4)}N·m</span><br>
<span class="lb">Stable:</span><span class="${stab?'vg':'vn'}">${stab?'YES':'NO'}</span></div>`;
}

// ===================== BUILD UI =====================
function buildUI(){
  const cp=document.getElementById('cp');
  const defs=[
    {t:'🌊 WATER & ENVIRONMENT',open:true,ctrls:[
      {p:'currentSpeed',l:'Current Speed',mn:0,mx:5,st:0.1,u:'m/s'},
      {p:'streamAngle',l:'Stream Angle',mn:-45,mx:45,st:1,u:'°'},
      {p:'waterLevel',l:'Water Level',mn:0.2,mx:3,st:0.05,u:'m'},
      {p:'waterDensity',l:'Water Density',mn:900,mx:1100,st:5,u:'kg/m³'},
      {p:'viscMult',l:'Viscosity ×',mn:0.5,mx:3,st:0.1,u:'×'},
      {p:'waveOn',l:'Waves',type:'cb'},
      {p:'waveAmp',l:'Wave Amp',mn:0,mx:0.15,st:0.01,u:'m'},
      {p:'wavePer',l:'Wave Period',mn:0.3,mx:5,st:0.1,u:'s'},
    ]},
    {t:'💊 CAPSULE',open:true,ctrls:[
      {p:'capH',l:'Height',mn:0.10,mx:0.80,st:0.01,u:'m'},
      {p:'capD',l:'Diameter',mn:0.03,mx:0.25,st:0.01,u:'m'},
      {p:'capM',l:'Mass',mn:0.02,mx:2,st:0.01,u:'kg'},
      {p:'roughness',l:'Roughness ×',mn:0.5,mx:2,st:0.1,u:'×'},
      {p:'ballast',l:'Ballast Pos',mn:0,mx:1,st:0.01,u:''},
      {p:'initAngle',l:'Init Angle',mn:-180,mx:180,st:1,u:'°'},
    ]},
    {t:'🔗 TETHER',open:true,ctrls:[
      {p:'tethLen',l:'Length',mn:0.2,mx:3,st:0.05,u:'m'},
      {p:'tethElast',l:'Elasticity',mn:0,mx:0.5,st:0.01,u:''},
      {p:'tethAttach',l:'Attach Pt',mn:0,mx:0.5,st:0.01,u:''},
    ]},
    {t:'⚙️ PHYSICS',open:false,ctrls:[
      {p:'damping',l:'Damping',mn:0.1,mx:5,st:0.1,u:''},
      {p:'angDamp',l:'Ang Damping',mn:0.001,mx:0.1,st:0.001,u:''},
      {p:'addedMass',l:'Added Mass',mn:0,mx:2,st:0.1,u:''},
      {p:'simSpeed',l:'Sim Speed',mn:0.1,mx:3,st:0.1,u:'×'},
    ]},
  ];

  let html='';
  defs.forEach(sec=>{
    html+=`<div class="sec"><div class="sec-h" onclick="this.nextElementSibling.classList.toggle('open')">${sec.t} <span>▼</span></div><div class="sec-b${sec.open?' open':''}">`;
    sec.ctrls.forEach(c=>{
      if(c.type==='cb'){
        html+=`<div class="cr"><label>${c.l}</label><input type="checkbox" id="cb_${c.p}" ${P[c.p]?'checked':''}></div>`;
      } else {
        const prec=c.st<0.01?3:c.st<1?2:0;
        html+=`<div class="cr"><label>${c.l}</label><input type="range" id="sl_${c.p}" min="${c.mn}" max="${c.mx}" step="${c.st}" value="${P[c.p]}"><span class="vl" id="vl_${c.p}">${Number(P[c.p]).toFixed(prec)}${c.u}</span></div>`;
      }
    });
    html+=`</div></div>`;
  });

  // Sim controls
  html+=`<div class="sec"><div class="sec-h" onclick="this.nextElementSibling.classList.toggle('open')">▶ CONTROLS <span>▼</span></div><div class="sec-b open">
  <div class="btn-row"><button class="btn" id="bPause">⏸ Pause</button><button class="btn" id="bReset">🔄 Reset</button></div>
  <div class="btn-row"><button class="btn" id="bForce">🔀 Forces ON</button><button class="btn" id="bFlow">💧 Flow ON</button></div>
  <div class="cr"><label>Preset</label><select id="presetSel">${Object.keys(presets).map(k=>`<option>${k}</option>`).join('')}</select></div>
  <div class="btn-row"><button class="btn" id="bPreset">Apply Preset</button></div>
  </div></div>`;

  // Readouts
  html+=`<div class="sec"><div class="sec-h" onclick="this.nextElementSibling.classList.toggle('open')">📊 READOUTS <span>▼</span></div><div class="sec-b open"><div class="rdout" id="rdiv"></div></div></div>`;

  cp.innerHTML=html;

  // Bind sliders
  defs.forEach(sec=>sec.ctrls.forEach(c=>{
    if(c.type==='cb'){
      const el=document.getElementById('cb_'+c.p);
      el.addEventListener('change',()=>{P[c.p]=el.checked;});
    } else {
      const sl=document.getElementById('sl_'+c.p);
      const vl=document.getElementById('vl_'+c.p);
      const prec=c.st<0.01?3:c.st<1?2:0;
      sl.addEventListener('input',()=>{
        P[c.p]=parseFloat(sl.value);
        vl.textContent=Number(sl.value).toFixed(prec)+c.u;
      });
    }
  }));

  document.getElementById('bPause').addEventListener('click',function(){
    paused=!paused;this.textContent=paused?'▶ Play':'⏸ Pause';
  });
  document.getElementById('bReset').addEventListener('click',resetSim);
  document.getElementById('bForce').addEventListener('click',function(){
    P.showForces=!P.showForces;this.textContent='🔀 Forces '+(P.showForces?'ON':'OFF');
  });
  document.getElementById('bFlow').addEventListener('click',function(){
    P.showFlow=!P.showFlow;this.textContent='💧 Flow '+(P.showFlow?'ON':'OFF');
  });
  document.getElementById('bPreset').addEventListener('click',()=>{
    const nm=document.getElementById('presetSel').value;
    const pr=presets[nm];if(!pr)return;
    Object.keys(pr).forEach(k=>{if(P.hasOwnProperty(k)){
      P[k]=pr[k];
      const sl=document.getElementById('sl_'+k);
      if(sl){sl.value=pr[k];sl.dispatchEvent(new Event('input'));}
      const cb=document.getElementById('cb_'+k);
      if(cb)cb.checked=pr[k];
    }});
    resetSim();
  });
}

const presets={
  "Still Pond":{currentSpeed:0,streamAngle:0,waveOn:false,waterLevel:1.5,waterDensity:1000,viscMult:1},
  "Gentle Stream":{currentSpeed:0.3,streamAngle:0,waveOn:true,waterLevel:1.0,waveAmp:0.02,wavePer:3},
  "Moderate River":{currentSpeed:1.0,streamAngle:2,waveOn:true,waveAmp:0.05,wavePer:2},
  "Fast River":{currentSpeed:2.0,streamAngle:5,waveOn:true,waveAmp:0.07,wavePer:1.5},
  "Steep Rapids":{currentSpeed:3.0,streamAngle:15,waveOn:true,waveAmp:0.12,wavePer:1.0},
  "Waterfall Approach":{currentSpeed:4.0,streamAngle:30,waveOn:true,waveAmp:0.15,wavePer:0.8},
  "Drainage Channel":{currentSpeed:2.0,streamAngle:20,waveOn:false,waveAmp:0},
  "Flood High Water":{currentSpeed:3.0,streamAngle:5,waterLevel:2.5,waveOn:true,waveAmp:0.12,wavePer:1.2},
  "Flood Low Fast":{currentSpeed:4.0,streamAngle:10,waterLevel:0.5},
  "Dead Calm":{currentSpeed:0,streamAngle:0,waveOn:false,waterLevel:1.0,waveAmp:0},
  "Salt Water Harbor":{currentSpeed:0.5,streamAngle:0,waterDensity:1025,waveOn:true,waveAmp:0.03,wavePer:3},
  "Muddy Flood":{currentSpeed:2.5,streamAngle:8,waterDensity:1080,viscMult:2.0,waveOn:true,waveAmp:0.08,wavePer:1.5}
};

// ===================== MAIN LOOP =====================
let readoutTimer=0;
function loop(ts){
  if(!lastTS)lastTS=ts;
  let dt=(ts-lastTS)/1000;
  lastTS=ts;
  if(dt>0.05)dt=0.05;
  fpsC++;fpsT+=dt;
  if(fpsT>0.5){fps=fpsC/fpsT;fpsC=0;fpsT=0;}

  if(!paused){
    const simDt=dt*P.simSpeed;
    const steps=Math.min(Math.ceil(simDt/0.001),60);
    const sDt=simDt/steps;
    for(let i=0;i<steps;i++)physStep(sDt);
    updateParts(simDt);
  }

  render();
  renderGraphs();

  readoutTimer+=dt;
  if(readoutTimer>0.15){readoutTimer=0;updRead();}

  requestAnimationFrame(loop);
}

// ===================== INIT =====================
buildUI();
resize();
initParts();
resetSim();
requestAnimationFrame(loop);
</script>
</body>
</html>

</🎯 What This System Is
This is a browser-based physics simulation of a capsule-shaped buoy tethered to an anchor on the riverbed. It models how the capsule responds to changing water levels, currents, and waves — forming the conceptual foundation for a water level monitoring system.

Core monitoring principle: By measuring the capsule's vertical position, tilt angle, and tether tension, you can infer the water level, flow speed, and flood conditions in real time.



<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Cylinder Pendulum - Semicircular Swing</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            background: #050515;
            overflow: hidden;
        }

        canvas {
            border-radius: 10px;
        }
    </style>
</head>
<body>
    <canvas id="canvas"></canvas>

    <script>
        const canvas = document.getElementById('canvas');
        const ctx = canvas.getContext('2d');

        function resize() {
            canvas.width = window.innerWidth;
            canvas.height = window.innerHeight;
        }
        resize();
        window.addEventListener('resize', resize);

        const ropeLen = 320;
        let theta = -Math.PI / 2;
        let direction = 1;
        const speed = 1;

        const maxTrail = 60;
        const trail = new Array(maxTrail);
        let trailIndex = 0;
        let trailCount = 0;

        function getAnchor() {
            return { x: canvas.width / 2, y: canvas.height * 0.72 };
        }

        function getGroundY() {
            return canvas.height * 0.72;
        }

        function getCylPos(anchor) {
            return {
                x: anchor.x + Math.sin(theta) * ropeLen,
                y: anchor.y - Math.cos(theta) * ropeLen
            };
        }

        function drawBackground() {
            ctx.fillStyle = '#0a0a25';
            ctx.fillRect(0, 0, canvas.width, canvas.height);
        }

        function drawGround() {
            const gy = getGroundY();
            ctx.fillStyle = '#1a1a1a';
            ctx.fillRect(0, gy, canvas.width, canvas.height - gy);
            ctx.strokeStyle = '#444';
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.moveTo(0, gy);
            ctx.lineTo(canvas.width, gy);
            ctx.stroke();
        }

        function drawPath(anchor) {
            ctx.strokeStyle = 'rgba(0, 180, 255, 0.15)';
            ctx.lineWidth = 1;
            ctx.setLineDash([5, 10]);
            ctx.beginPath();
            ctx.arc(anchor.x, anchor.y, ropeLen, -Math.PI, 0, false);
            ctx.stroke();
            ctx.setLineDash([]);
        }

        function drawGroundMarkers(anchor) {
            const gy = getGroundY();
            ctx.fillStyle = 'rgba(255, 100, 100, 0.5)';
            ctx.beginPath();
            ctx.arc(anchor.x - ropeLen, gy, 4, 0, Math.PI * 2);
            ctx.fill();
            ctx.beginPath();
            ctx.arc(anchor.x + ropeLen, gy, 4, 0, Math.PI * 2);
            ctx.fill();
        }

        function addTrailPoint(x, y, t) {
            trail[trailIndex] = { x, y, theta: t };
            trailIndex = (trailIndex + 1) % maxTrail;
            if (trailCount < maxTrail) trailCount++;
        }

        function drawTrail() {
            if (trailCount < 2) return;
            ctx.lineWidth = 2;
            for (let i = 1; i < trailCount; i++) {
                const idx = (trailIndex - trailCount + i + maxTrail) % maxTrail;
                const prevIdx = (idx - 1 + maxTrail) % maxTrail;
                const alpha = i / trailCount;
                const hue = (trail[idx].theta / Math.PI * 180 + 270) % 360;
                ctx.strokeStyle = `hsla(${hue}, 100%, 55%, ${alpha * 0.4})`;
                ctx.beginPath();
                ctx.moveTo(trail[prevIdx].x, trail[prevIdx].y);
                ctx.lineTo(trail[idx].x, trail[idx].y);
                ctx.stroke();
            }
        }

        function drawCylShadow(pos) {
            const gy = getGroundY();
            const dist = gy - pos.y;
            const stretch = 1 + dist / 500;
            const alpha = Math.max(0.05, 0.3 - dist / 700);
            ctx.fillStyle = `rgba(0,0,0,${alpha})`;
            ctx.beginPath();
            ctx.ellipse(pos.x, gy + 3, 40 * stretch, 6, 0, 0, Math.PI * 2);
            ctx.fill();
        }

        function drawRope(anchor, pos) {
            ctx.strokeStyle = '#7a6545';
            ctx.lineWidth = 3;
            ctx.beginPath();
            ctx.moveTo(anchor.x, anchor.y - 10);
            ctx.lineTo(pos.x, pos.y);
            ctx.stroke();
        }

        function drawAnchor(anchor) {
            const ax = anchor.x, ay = anchor.y;
            ctx.fillStyle = '#666';
            ctx.fillRect(ax - 25, ay - 5, 50, 12);
            ctx.fillStyle = '#888';
            ctx.beginPath(); ctx.arc(ax - 15, ay, 3, 0, Math.PI * 2); ctx.fill();
            ctx.beginPath(); ctx.arc(ax + 15, ay, 3, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = '#777';
            ctx.fillRect(ax - 4, ay - 18, 8, 15);
            ctx.strokeStyle = '#aaa';
            ctx.lineWidth = 3;
            ctx.beginPath(); ctx.arc(ax, ay - 12, 7, 0, Math.PI * 2); ctx.stroke();
        }

        function drawCylinder(pos) {
            ctx.save();
            ctx.translate(pos.x, pos.y);
            ctx.rotate(theta);
            const w = 50, h = 80, hw = 25, hh = 40;

            ctx.shadowColor = 'rgba(0,0,0,0.4)';
            ctx.shadowBlur = 15;
            ctx.fillStyle = '#c0392b';
            ctx.fillRect(-hw, -hh, w, h);
            ctx.shadowColor = 'transparent';
            ctx.shadowBlur = 0;

            ctx.fillStyle = '#962d22';
            ctx.fillRect(-hw, -hh, 10, h);
            ctx.fillRect(hw - 10, -hh, 10, h);
            ctx.fillStyle = 'rgba(255,255,255,0.15)';
            ctx.fillRect(-hw + 12, -hh, 8, h);
            ctx.fillStyle = '#999';
            ctx.fillRect(-hw, -hh + 8, w, 4);
            ctx.fillRect(-hw, hh - 12, w, 4);

            ctx.fillStyle = '#7a2118';
            ctx.beginPath(); ctx.ellipse(0, hh, hw, 10, 0, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = '#e74c3c';
            ctx.beginPath(); ctx.ellipse(0, -hh, hw, 10, 0, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = 'rgba(255,255,255,0.2)';
            ctx.beginPath(); ctx.ellipse(-5, -hh - 2, 8, 4, 0, 0, Math.PI * 2); ctx.fill();
            ctx.fillStyle = '#666';
            ctx.beginPath(); ctx.arc(0, hh + 5, 4, 0, Math.PI * 2); ctx.fill();
            ctx.restore();
        }

        function drawTrigonometryLines(pos, anchor) {
            const gy = getGroundY();
            const verticalHeight = gy - pos.y;
            const horizontalDist = pos.x - anchor.x;

            if (Math.abs(verticalHeight) < 12 || Math.abs(horizontalDist) < 12) return;

            // Triangle vertices
            // A = anchor on ground, B = cylinder, C = foot of perpendicular
            const A = { x: anchor.x, y: gy };
            const B = { x: pos.x, y: pos.y };
            const C = { x: pos.x, y: gy };
            const isRight = horizontalDist > 0;

            // Subtle triangle fill
            ctx.fillStyle = 'rgba(255, 255, 255, 0.035)';
            ctx.beginPath();
            ctx.moveTo(A.x, A.y);
            ctx.lineTo(B.x, B.y);
            ctx.lineTo(C.x, C.y);
            ctx.closePath();
            ctx.fill();

            // Green perpendicular line (B → C)
            ctx.strokeStyle = '#00ff88';
            ctx.lineWidth = 2;
            ctx.setLineDash([6, 4]);
            ctx.beginPath();
            ctx.moveTo(B.x, B.y);
            ctx.lineTo(C.x, C.y);
            ctx.stroke();
            ctx.setLineDash([]);

            ctx.fillStyle = '#00ff88';
            ctx.beginPath();
            ctx.arc(C.x, C.y, 4, 0, Math.PI * 2);
            ctx.fill();

            // Orange horizontal line (A → C)
            ctx.strokeStyle = '#ffaa00';
            ctx.lineWidth = 2;
            ctx.setLineDash([6, 4]);
            ctx.beginPath();
            ctx.moveTo(A.x, A.y);
            ctx.lineTo(C.x, C.y);
            ctx.stroke();
            ctx.setLineDash([]);

            ctx.fillStyle = '#ffaa00';
            ctx.beginPath();
            ctx.arc(A.x, A.y, 4, 0, Math.PI * 2);
            ctx.fill();

            // Side length labels
            ctx.font = 'bold 13px monospace';
            ctx.textAlign = 'center';

            // h label
            ctx.fillStyle = '#00ff88';
            ctx.fillText(`h = ${Math.abs(verticalHeight).toFixed(0)}`,
                B.x + (isRight ? 38 : -38), B.y + verticalHeight / 2);

            // d label
            ctx.fillStyle = '#ffaa00';
            ctx.fillText(`d = ${Math.abs(horizontalDist).toFixed(0)}`,
                A.x + horizontalDist / 2, gy + 24);

            // L label on hypotenuse
            const mx = (A.x + B.x) / 2;
            const my = (A.y + B.y) / 2;
            ctx.fillStyle = '#ccc';
            ctx.fillText(`L = ${ropeLen}`, mx + (isRight ? -28 : 28), my - 10);

            // Right angle square at C (INSIDE triangle)
            const sq = 13;
            ctx.strokeStyle = 'rgba(255, 255, 255, 0.6)';
            ctx.lineWidth = 1.5;
            ctx.setLineDash([]);
            ctx.beginPath();
            if (isRight) {
                ctx.moveTo(C.x - sq, C.y);
                ctx.lineTo(C.x - sq, C.y - sq);
                ctx.lineTo(C.x, C.y - sq);
            } else {
                ctx.moveTo(C.x + sq, C.y);
                ctx.lineTo(C.x + sq, C.y - sq);
                ctx.lineTo(C.x, C.y - sq);
            }
            ctx.stroke();

            // === ANGLE VALUES ===
            // θ₁ at B (cylinder): between GREEN perpendicular and GRAY rope
            // θ₂ at A (anchor):   between ORANGE horizontal  and GRAY rope
            const theta1 = Math.abs(theta);
            const theta2 = Math.PI / 2 - Math.abs(theta);
            const theta1Deg = theta1 * 180 / Math.PI;
            const theta2Deg = theta2 * 180 / Math.PI;

            // Directions at B
            const downAngle = Math.PI / 2; // B→C (straight down)
            const ropeBackAngle = Math.atan2(A.y - B.y, A.x - B.x); // B→A

            // Direction at A
            const ropeUpAngle = Math.atan2(B.y - A.y, B.x - A.x); // A→B

            // ============================================
            // θ₁ ARC at CYLINDER (B) — INSIDE triangle
            // Between green perpendicular (down) & rope (toward anchor)
            // ============================================
            const arcR1 = 42;
            ctx.strokeStyle = '#ff6b6b';
            ctx.lineWidth = 2.5;
            ctx.beginPath();
            if (isRight) {
                // ropeBackAngle is between PI/2 and PI
                // clockwise from straight-down to rope direction = INSIDE
                ctx.arc(B.x, B.y, arcR1, downAngle, ropeBackAngle, false);
            } else {
                // ropeBackAngle is between 0 and PI/2
                // counterclockwise from straight-down to rope direction = INSIDE
                ctx.arc(B.x, B.y, arcR1, downAngle, ropeBackAngle, true);
            }
            ctx.stroke();

            // θ₁ label — positioned INSIDE triangle
            const mid1 = (downAngle + ropeBackAngle) / 2;
            const l1x = B.x + Math.cos(mid1) * (arcR1 + 25);
            const l1y = B.y + Math.sin(mid1) * (arcR1 + 25);
            ctx.fillStyle = '#ff6b6b';
            ctx.font = 'bold 15px Arial';
            ctx.textAlign = 'center';
            ctx.fillText(`θ₁ = ${theta1Deg.toFixed(1)}°`, l1x, l1y);

            // ============================================
            // θ₂ ARC at ANCHOR (A) — INSIDE triangle
            // Between orange horizontal (ground) & rope (toward cylinder)
            // ============================================
            const arcR2 = 48;
            ctx.strokeStyle = '#00bfff';
            ctx.lineWidth = 2.5;
            ctx.beginPath();
            if (isRight) {
                // horizontal is 0 (right), rope is negative angle (up-right)
                // clockwise from rope up to horizontal = INSIDE
                ctx.arc(A.x, A.y, arcR2, ropeUpAngle, 0, false);
            } else {
                // horizontal is PI (left), rope is between -PI and -PI/2 (up-left)
                // clockwise from -PI to rope angle = INSIDE
                ctx.arc(A.x, A.y, arcR2, -Math.PI, ropeUpAngle, false);
            }
            ctx.stroke();

            // θ₂ label — positioned INSIDE triangle
            let mid2;
            if (isRight) {
                mid2 = ropeUpAngle / 2;
            } else {
                mid2 = (-Math.PI + ropeUpAngle) / 2;
            }
            const l2x = A.x + Math.cos(mid2) * (arcR2 + 25);
            const l2y = A.y + Math.sin(mid2) * (arcR2 + 25);
            ctx.fillStyle = '#00bfff';
            ctx.font = 'bold 15px Arial';
            ctx.textAlign = 'center';
            ctx.fillText(`θ₂ = ${theta2Deg.toFixed(1)}°`, l2x, l2y);

            // ============================================
            // INFO PANEL (top-left)
            // ============================================
            ctx.textAlign = 'left';
            ctx.fillStyle = '#ffffff';
            ctx.font = 'bold 14px Arial';
            ctx.fillText(`θ₁ + θ₂ + 90° = ${(theta1Deg + theta2Deg + 90).toFixed(1)}°  (= 180°)`, 20, 30);

            ctx.font = '13px Arial';
            ctx.fillStyle = '#ff6b6b';
            ctx.fillText(`θ₁ = ${theta1Deg.toFixed(1)}°  (green ↕ ↔ rope)`, 20, 55);
            ctx.fillStyle = '#00bfff';
            ctx.fillText(`θ₂ = ${theta2Deg.toFixed(1)}°  (orange ↔ ↔ rope)`, 20, 75);
            ctx.fillStyle = 'rgba(255,255,255,0.5)';
            ctx.fillText(`90°  (right angle at corner)`, 20, 95);

            ctx.fillStyle = '#aaa';
            ctx.font = '12px Arial';
            ctx.fillText(`h = L·cos(θ₁) = L·sin(θ₂) = ${Math.abs(verticalHeight).toFixed(1)}`, 20, 122);
            ctx.fillText(`d = L·sin(θ₁) = L·cos(θ₂) = ${Math.abs(horizontalDist).toFixed(1)}`, 20, 142);
            ctx.fillText(`L = ${ropeLen}  (hypotenuse / rope)`, 20, 162);
        }

        let flashAlpha = 0;
        let flashX = 0;

        function triggerFlash(x) {
            flashAlpha = 1;
            flashX = x;
        }

        function drawFlash() {
            if (flashAlpha > 0) {
                ctx.fillStyle = `rgba(255, 200, 100, ${flashAlpha})`;
                ctx.beginPath();
                ctx.arc(flashX, getGroundY(), 20 * flashAlpha, 0, Math.PI * 2);
                ctx.fill();
                flashAlpha -= 0.05;
            }
        }

        let lastDirection = 1;
        let frameCount = 0;

        function update() {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            drawBackground();

            const anchor = getAnchor();

            theta += 0.02 * speed * direction;

            if (theta >= Math.PI / 2) {
                theta = Math.PI / 2;
                direction = -1;
                if (lastDirection !== -1) {
                    triggerFlash(anchor.x + ropeLen);
                    lastDirection = -1;
                }
            } else if (theta <= -Math.PI / 2) {
                theta = -Math.PI / 2;
                direction = 1;
                if (lastDirection !== 1) {
                    triggerFlash(anchor.x - ropeLen);
                    lastDirection = 1;
                }
            }

            const pos = getCylPos(anchor);

            frameCount++;
            if (frameCount % 3 === 0) {
                addTrailPoint(pos.x, pos.y, theta);
            }

            drawGround();
            drawPath(anchor);
            drawGroundMarkers(anchor);
            drawTrail();
            drawFlash();
            drawTrigonometryLines(pos, anchor);
            drawCylShadow(pos);
            drawRope(anchor, pos);
            drawAnchor(anchor);
            drawCylinder(pos);

            requestAnimationFrame(update);
        }

        update();
    </script>
</body>
</html>
```
