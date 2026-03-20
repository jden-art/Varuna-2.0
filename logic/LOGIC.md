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
