# G80 visual resources

The G80 editor is fixed at **760 × 438 logical pixels**. Production rasters are
exported at 2× and compiled into the plug-in through `juce_add_binary_data`; no
runtime path under `Resources/` is required on an end-user Mac.

## Compiled resources

| File | Pixels | Alpha | Use |
| --- | ---: | :---: | --- |
| `export/chassis-panel-typography-v2@2x.png` | 1520 × 876 | no | Leather chassis, recessed green panel, aluminium plate, fixed decoration and footer |
| `export/input-vu-housing@2x.png` | 320 × 220 | yes | INPUT bezel, paper, inner shadow and glass; no live scale or needle |
| `export/output-vu-housing@2x.png` | 320 × 220 | yes | OUTPUT equivalent, same geometry |
| `export/gain-knob-metal@2x.png` | 250 × 250 | yes | GAIN body without a fixed indicator |
| `export/fatty-knob-metal@2x.png` | 330 × 330 | yes | FATTY body without a fixed indicator |
| `source/switch/switch-base.png` | 1024 × 1024 | yes | Static UPPER/LOWER switch base |
| `source/switch/switch-lever-upper.png` | 1024 × 1024 | yes | UPPER lever layer (160 Hz) |
| `source/switch/switch-lever-lower.png` | 1024 × 1024 | yes | LOWER lever layer (80 Hz) |

## Canonical G80 layout

Coordinates are derived from the approved 1652 × 952 artwork and normalized to
the 760 × 438 editor. `Source/PluginEditor.cpp` is the executable authority.

| Z | Element | Canonical geometry | Dynamic ownership |
| ---: | --- | --- | --- |
| 0 | Chassis/panel | `(0, 0, 760, 438)` | static PNG |
| 1 | INPUT VU housing | rect `(76, 122, 150, 106)` | static PNG |
| 1 | OUTPUT VU housing | rect `(76, 240, 150, 106)` | static PNG |
| 2 | INPUT/OUTPUT scale and captions | within their housing rects | JUCE vector |
| 3 | INPUT/OUTPUT needle and pivot | same meter geometry | JUCE, RMS-driven |
| 1 | GAIN body | centre `(322.6, 230.4)`, exterior diameter `115` | static PNG |
| 2 | GAIN scale, label, temporary value and engraved indicator | same centre | JUCE, parameter `gain` |
| 1 | Selector base | mechanical centre `(451.8, 232.3)`, visible base diameter `46` | static PNG |
| 2 | Selector lever / UPPER / LOWER labels | same mechanical pivot | JUCE state layer, parameter `fattyMode` |
| 1 | FATTY body | centre `(595.3, 232.5)`, exterior diameter `143` | static PNG |
| 2 | FATTY scale, label, temporary value and engraved indicator | same centre | JUCE, parameter `bass` |

The GAIN/FATTY indicators, all scales and values remain dynamic. This prevents
illustrative pointers from being visible under a live parameter indicator.

## VU visual calibration

Audio measurement remains outside these assets: block RMS power, 300 ms
one-pole smoothing, `0 VU = −18 dBFS RMS`, and a 30 Hz GUI update. The shared
visual PCHIP map used by the needle, ticks and labels is:

| VU | Angle |
| ---: | ---: |
| −30 | 207° |
| −20 | 222° |
| −10 | 263° |
| 0 | 304° |
| +3 | 329° |
| +6 | 354° |

The printed arc is deliberately limited to −20 through +3 VU; its red zone
starts at 0 VU. The needle may travel across the full −30 through +6 VU map.

## Repository scope

Only the exported assets and switch layers listed above are tracked because
they are the files embedded by CMake. Local previews, generated source imagery,
prompts and helper tooling remain deliberately outside the development source
release; none is required to build or run the plug-in.
