# Antenna designer

Tools > Antenna designer models wire antennas, the way 4nec2, xnec2c, MMANA-GAL and EZNEC do. You can start from a template, import a NEC
deck or draw the wires yourself in the 3D view. It then shows the input impedance and SWR across a band, the radiation pattern (in 3D and as azimuth and elevation cuts), the gain in
dBi and dBd, the front-to-back ratio, the beamwidths, the take-off angle, the efficiency and the current on every wire. The Optimise tab
trims or optimises the dimensions, the Calculators tab covers the feed side, and the pattern can be exported for Tools > Propagation.

Everything runs locally. Nothing is downloaded.

- Solver: `core/include/dect2/antd/antd.h`, `core/src/antd/antd_mom.cpp`, `antd_lu.cpp` (pure functions, no I/O)
- Templates, calculators, optimiser, files: `core/include/dect2/antd/antd_tools.h`, `core/src/antd/antd_{templates,calc,opt,io}.cpp`
- Window: `app/antd/antd_ui.cpp` (model, 3D view, results), `app/antd/antd_calc_ui.cpp` (calculators, polar plot)
- Tests: `tests/antd/test_antd_mom.cpp` (analytic cases), `tests/antd/test_antd_tools.cpp` (templates, calculators, files, optimiser)

## The solver

This is a clean-room thin-wire method of moments, written from the textbook formulation (Harrington, *Field Computation by Moment Methods*;
Balanis, *Antenna Theory*, chapter 8). No NEC code is used.

- **Equation**: the electric-field integral equation in mixed-potential form (vector potential plus scalar potential, the Pocklington
  equation integrated by parts), with the reduced thin-wire kernel G = e^(-jkR) / (4 pi R), where R = sqrt(|r - r'|^2 + a^2).
- **Basis and testing**: triangle (rooftop) functions on straight segments, used for both. Each triangle spans two segments that share a
  node (Galerkin). A junction of n wire ends gets n - 1 triangles, which enforces Kirchhoff's current law. A free wire end carries no
  current. A wire end on the ground gets a half triangle, whose current continues into the image.
- **Integrals**: the static 1/R part has a closed form (asinh), and the smooth remainder (e^(-jkR) - 1) / R uses Gauss quadrature. Near
  pairs use 2 x 6 test points and 4 source points; far pairs use 3 x 3 or 3 x 4 with the full kernel.
- **Sources**: delta-gap voltage sources, as many as needed, each with a complex voltage (so a turnstile can be phased). A source can also
  drive the far end of a transmission line, which models a feeder such as the G5RV ladder line or a coax run.
- **Loads**: R + jX, series RLC and parallel RLC, placed in series in the wire at a point. Lossy wires use the skin-effect internal
  impedance per metre, (1 + j) Rs / (2 pi a) with Rs = sqrt(omega mu0 / (2 sigma)), combined with the DC resistance for thin wires.
- **Transmission lines** (as the NEC TL card): two-port admittance Y = Z^-1 with Z11 = Z0 coth(gamma l) and Z12 = Z0 csch(gamma l), with
  a velocity factor, a loss figure and an optional crossed connection. Lines, a source and the wire at one point are in parallel across
  the gap. Line ends without a source are folded into the matrix as Z += Yff^-1. Ends with a source are held at the source voltage.
- **Ground**:
  - free space;
  - perfect ground, using exact images (horizontal current and charge reversed, vertical current kept);
  - real ground with a relative permittivity and a conductivity. The near interactions use quasi-static complex images with
    K = (eps_c - 1) / (eps_c + 1), as MININEC-class programs do. The far field uses the Fresnel reflection coefficients Rv and Rh at the
    elevation angle, like NEC's reflection-coefficient approximation (GN 0), with eps_c = eps_r - j sigma / (omega eps0). Buried wires
    and radials are not modelled.
- **LU**: dense complex float64 LU with partial pivoting. It is right-looking and blocked (48-column panels). For large matrices the
  trailing update is split by rows over up to 8 threads. Measured: n = 1500 takes 0.8 s on one thread and 0.26 s on eight, and a
  1200-segment wire solves in 0.3 s (fill plus LU).
- **Sweeps**: one mesh, made dense enough for the top of the band, and the frequencies solved in parallel.

### Outputs and definitions

- **Gain** is relative to the input power, so it includes the losses in loads, lossy wire and, over real ground, the ground:
  G = 4 pi U / P_in, where U = r^2 |E|^2 / (2 eta0). The vertical part comes from E_theta and the horizontal part from E_phi.
  dBd = dBi - 2.15.
- **Efficiency** = P_rad / P_in, where P_rad is the gain integrated over the sphere, or over the upper hemisphere when there is a ground.
  It is clamped to 0..1, because over real ground the far field and the currents use different ground models. **Directivity** is the
  maximum gain minus 10 log10(efficiency).
- **Angles**: theta is measured from the zenith and phi from +x, counter-clockwise (the NEC convention). Elevation = 90 - theta. The
  directional templates beam toward +x (phi 0).
- **Front to back** is the maximum against phi + 180 at the same elevation. Front to side uses phi + 90.
- **Beamwidths** are the -3 dB widths of the one-degree azimuth cut at the elevation of the maximum, and of the elevation cut at the
  azimuth of the maximum. **Take-off angle** is the elevation of the maximum over a ground.
- **Current distribution**: the current at both ends of every segment, for a 1 V feed. It is shown as wire colours in the 3D view and as
  magnitude and phase charts.
- **Near field** (`nearField`, core only): the E field at a point, -j omega A - grad Phi, with the images included.
- **Convergence** ("Check convergence"): the model is solved again with twice the segments. A change under 2 % counts as converged.

### Segmentation guidance

Segment counts are automatic by default: 20 per wavelength at the highest frequency in use, and at least 3 on a wire with a free end. A
wire whose ends both connect may have a single segment. The designer puts a node exactly at every source, load and line position. It
warns about the following:

- segments longer than a tenth of a wavelength;
- segments shorter than about 2 radii (the thin-wire limit; 8 radii or more is better);
- wires thicker than a thirtieth of a wavelength;
- junctions of very different radii;
- overlapping wires;
- wires below the ground;
- sources or loads on a free end.

To connect, wires must share an end point. A wire that crosses another in its middle is not joined to it.

### Accuracy (the tests)

| Case | Result | Reference |
|---|---|---|
| Half-wave dipole, exactly 0.5 lambda, a = 1e-5 lambda | 77.5 + j42.4 ohm, 2.16 dBi, 78 degree E-plane beamwidth | 73 + j42.5 (induced EMF, infinitely thin); 2.15 dBi |
| Same dipole at resonance | 0.488 lambda, 71.9 ohm | about 0.48-0.49 lambda, about 70-73 ohm |
| Short dipole 0.05 lambda | R = 20 pi^2 (L / lambda)^2 within 0.03 ohm, 1.76 dBi | triangular current; directivity 1.5 |
| Quarter-wave monopole, perfect ground | 38.7 + j21.2 ohm (exactly half the dipole), 5.17 dBi, maximum on the horizon | half the dipole; 5.16 dBi |
| One-wavelength square loop | resonant at a perimeter of 1.06 lambda, R = 126 ohm, about 3.3 dBi | about 1.05 lambda, 100-130 ohm |
| 2-element Yagi (reflector 0.5 lambda, 0.15 lambda spacing) | 6.4 dBi, front to back 10.6 dB | 6-7.5 dBi |
| Lossless antenna | the integrated far field equals the input power (efficiency 1.00) | energy balance |
| Segment refinement | the change in R shrinks every time the density doubles | convergence |
| Line from the source: lambda / 2, lambda / 4 | the dipole impedance repeated, and Z0^2 / Z | line theory |

## Templates

Each template takes the design frequency and a few parameters. The defaults come from the usual handbook formulas, and the solver shows
how close each formula is (some cutting formulas such as 142.65 / f come out slightly short in a model, because they also allow for
insulation and end fittings). **Trim to resonance** moves the main dimension until the feed reactance is zero at the selected frequency.

| Template | Formula / defaults |
|---|---|
| Half-wave dipole | 142.65 / f(MHz) m in total, fed in the middle, along y |
| Inverted-V | legs of 73 / f(MHz) m each, drooping 45 degrees from the apex |
| Folded dipole | 0.47 lambda, spacing lambda / 100, about 300 ohm |
| Off-centre-fed dipole | 0.49 lambda, fed at 33 % (4:1 balun, about 200 ohm) |
| End-fed half-wave | 0.49 lambda against a 0.05 lambda counterpoise (49:1, 2450 ohm) |
| G5RV-like | flat top 1.47 lambda (31.1 m at 14.15 MHz), 0.49 lambda of 450-ohm line (VF 0.97) driven by the source |
| Delta loop / quad loop | perimeter 306.3 / f(MHz) m, fed in the middle of the bottom side |
| Magnetic loop | diameter up to 0.15 lambda / pi (3 m at most), 11 mm copper tube, capacitor tuned by the solver from C = 1 / (omega^2 L) with L = mu0 R (ln(8R / a) - 2), coupling loop of about a fifth of the diameter (the size with the best 50-ohm match is picked) |
| Yagi-Uda | 2-12 elements: reflector 0.495 lambda, driven element 0.473 lambda, first director 0.44 lambda (each further one 0.004 lambda shorter); spacings 0.2 (reflector), 0.1, 0.2, 0.25, 0.3 ... lambda times a scale factor |
| Cubical quad | driven loop 1.02 lambda, reflector loop 1.05 lambda, 0.15 lambda apart |
| Vertical with radials | 71.3 / f(MHz) m, 4 radials of 0.25 lambda just above the ground (with the base at 0 over a ground it is a monopole into the ground) |
| Ground plane | quarter-wave whip with 4 radials drooping 45 degrees (about 50 ohm) |
| J-pole | 0.48 lambda radiator on a 0.25 lambda stub, spacing lambda / 50, tap at 0.018 lambda |
| Slim jim | 0.48 lambda folded radiator on a 0.24 lambda stub, gap 0.012 lambda, tap at 0.02 lambda |
| Axial-mode helix | 6 turns of 1 lambda circumference, pitch 0.23 lambda (C tan 13 degrees), over a perfect ground that stands for the reflector |
| Turnstile | two crossed 0.47 lambda dipoles fed 90 degrees apart, with crossed reflectors 0.25 lambda below |
| Log-periodic | tau 0.9, sigma 0.06: longest element 1.05 x lambda_low / 2, each next element tau times shorter and 2 sigma L further on, crossed 100-ohm feeder, fed at the short end |
| Discone (sketch) | 8 + 8 wires, cone slant lambda_low / 4, half angle 30 degrees, disc 0.7 times the cone's base diameter |

HF templates default to average real ground at a sensible height. VHF and up default to free space.

## Calculators

- Wavelength (with a velocity factor), and practical wire lengths: dipole 142.65 / f, quarter-wave 71.3 / f, loop 306.3 / f, EFHW 0.49
  lambda, 5/8 wave, inverted-V 146 / f.
- Cable loss. The matched loss is k1 sqrt(f) + k2 f in dB per 100 m, fitted to typical datasheet values for 18 common lines (RG-174 to
  7/8" Heliax, plus twin lead, window line and open wire). These are approximate.
- Loss with a mismatched load, with a = 10^(A / 10): total = 10 log10((a^2 - |Gamma|^2) / (a (1 - |Gamma|^2))), and SWR at the input
  from |Gamma| / a.
- SWR, return loss, mismatch loss and reflected power, from an SWR, a return loss or an impedance.
- L network: both solutions (Pozar section 5.1), with component values.
- Single shunt stub, open or shorted: both solutions (Pozar 5.2), with lengths for a velocity factor.
- Quarter-wave transformer: Z = sqrt(Z0 R).
- Gamma match starting dimensions (ARRL rules of thumb): rod 0.045 lambda, spacing 0.007 lambda, rod a third of the element's diameter,
  about 7 pF per metre of wavelength.
- Balun / choke: turns ratio sqrt(Z_high / Z_low), and the inductance for a common-mode choke reactance.
- Impedance through a line (lossy: Z0 (ZL + Z0 tanh(gamma l)) / (Z0 + ZL tanh(gamma l))), and series to parallel equivalents.
- EIRP / ERP (ERP = EIRP - 2.15 dB).
- Antenna factor: AF = sqrt(4 pi eta0 / (Z0 G)) / lambda, which for 50 ohm is 20 log f(MHz) - G(dBi) - 29.77.
- Gain from an aperture: 4 pi A / lambda^2.
- Band presets: IARU region 1 ham bands 160 m to 23 cm, CB, FM broadcast, airband, weather satellites, marine VHF, DAB band III, ISM 433,
  PMR446, DVB-T UHF (470-694 MHz), SRD/LoRa 868, ADS-B, GPS L1, L-band HRPT and Wi-Fi 2.4 GHz. Clicking a preset sweeps that band.

## Optimiser

Nelder-Mead (downhill simplex) works in the box between each chosen parameter's minimum and maximum. The cost has three parts:

- the worst SWR over the band (at 1-9 points) above its target, plus 2 % of the SWR so that it keeps improving;
- the gain toward phi 0 at the chosen elevation below its target;
- the front to back below its target.

Each part has its own weight. Every improvement is logged. Stop ends the search early, and Apply copies the result into the template. It
works on template parameters. To optimise a hand-edited model, go back to a template first.

## Files

### NEC decks

**Import** reads these cards:

- CM / CE
- GW, GA (arc in the x-z plane), GH (NEC-2 helix)
- GS (scale), GM (rotate / translate / copy)
- GE, GN (-1 free, 1 perfect, 0 / 2 real with eps_r and sigma)
- EX 0 (voltage source)
- LD 0 / 1 / 4 / 5
- TL (negative Z0 = crossed)
- FR

Output requests (RP, NE, XQ, ...) are ignored. Surface patches, cylinders and Sommerfeld options are refused with a message. A source on
segment k of n is placed at (k - 0.5) / n along the wire. A source on segment 1 of a wire standing on the ground is placed at the ground.

**Export** writes GW (with an odd segment count where a centre feed needs a centre segment), GE, GN, LD, TL, FR, EX, RP and EN. A source
that drives a line end becomes a tiny stub wire away from the antenna, as NEC needs one. A velocity factor is written as a longer line,
because NEC's TL has no velocity factor.

### The pattern file (for Tools > Propagation)

"Export the pattern for Propagation" writes `<data folder>/antennas/<name>.csv`. On macOS the data folder is
`~/Library/Application Support/OnAir`; elsewhere it is OnAir's data directory (`plat::dataDir()`). "Pattern as..." writes the same file
wherever you choose. The format is plain text:

```
# OnAir antenna pattern v1
# name: Yagi-Uda
# freq_mhz: 145.000000
# ground: free                       (or: perfect, or: real epsr=13 sigma=0.005)
# max_gain_dbi: 7.81
# az_step_deg: 5
# el_step_deg: 5
# el_min_deg: -90                    (0 over a ground)
# azimuth: compass bearing in degrees, 0 = the model's +y (north), 90 = +x (east)
# elevation: degrees above the horizon; gains in dBi (total, vertical, horizontal), -99 = none
az_deg,el_deg,gain_dbi,gain_v_dbi,gain_h_dbi
0,-90,-12.31,-14.02,-17.11
5,-90,...
```

- Lines that start with `#` are header lines of the form `key: value`. Readers must ignore keys they do not know.
- One data row per grid point, elevation rows from `el_min_deg` to 90, azimuth 0 to 360 - step (no duplicate 360 column).
- The azimuth is a compass bearing: the model's +y is north and +x is east. The model's phi (from +x, counter-clockwise) is
  phi = 90 - azimuth. So a Yagi built along +x beams east (azimuth 90). The propagation tool rotates the file to the real antenna
  heading by adding the heading minus 90 degrees.
- The gains are absolute dBi, including the losses, so the transmitter EIRP toward a bearing is P + gain(bearing, elevation) - feeder
  loss.
- Readers can use `dect2::antd::readPatternCsv` and `patternGainDbi(file, azimuth, elevation)` (bilinear, wraps in azimuth, -999 outside
  the elevation range). These are pure functions in `antd_tools.h`, so the propagation code can link them without the designer.

### Tables

- Sweep: `f_mhz,r_ohm,x_ohm,swr_<z0>,return_loss_db,gain_dbi,fb_db`.
- Currents: `wire,segment,x_m,y_m,z_m,current_ma,phase_deg` (the segment centre, for a 1 V feed).

The model you are working on is kept between sessions: either the template and its parameters, or a hand-edited model saved as
`antennas/last.nec`.

## The 3D editor

- Drag to orbit, use the wheel to zoom, right-drag or middle-drag to pan, and double-click to reset the view.
- Click a wire to select it. Its ends get circles: drag an end over the horizontal plane through it, or hold Shift and drag to move it up
  and down. Ends snap to the grid. Other wire ends joined to the moved end follow it.
- In the Wires tab you can type exact coordinates (metric or imperial), set the radius, segment count and conductor (perfect, copper,
  aluminium, brass, steel), add a wire (also continuing from the selected end), delete it, put the feed on it or add a load to it.
- Move, mirror (copy in x / y / z), rotate 90 degrees, and array the selected wire or all wires by a step vector.
- The feeds, loads and lines editor sets each feed's wire, position, magnitude and phase, each load's values, and each line's Z0, length,
  velocity factor and crossed setting.
- The view overlays the 3D pattern as a mesh coloured by gain (30 dB range), the current as wire colours, the segment nodes, and the
  ground grid (brown over a ground).

## Limits

- Thin wires only. There are no surface patches, no wires thicker than about lambda / 30, and no exact handling of very close parallel
  wires (keep them several radii apart).
- Real ground uses the quasi-static images and the reflection coefficient. There is no Sommerfeld integration, and no buried radials or
  wires. Over real ground, the impedance of an antenna very low over the ground is approximate.
- No insulated wires and no wire coatings.
- Lumped loads only (no distributed loading along a wire, except the conductor loss).
- The near field is in the core only. The designer does not plot it.
