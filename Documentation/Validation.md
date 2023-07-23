# Validation

Checked on Windows with Unreal Engine **5.5.4**, Visual Studio 2022 / MSVC 14.38, and Windows SDK 10.0.22621.0.

Both **AssignmentEditor / Win64 / Development** and **Assignment / Win64 / Development** compiled and linked successfully.

## Automated gameplay checks

**7 passed, 0 failed, 0 warnings.** Run through Unreal's automation controller with `-NullRHI -DisablePlugins=Fab` and `Automation RunTests VynixClimb`.

| Test | What it checks |
| --- | --- |
| `Rules.JumpTrajectory` | Exact grip endpoints, upward arc, mirrored lane jumps, clamped progress |
| `Rules.Score` | Height/time/bonus contributions, monotonic progress, overflow saturation |
| `Rules.Health` | Normal damage, health bounds, rejected negative/NaN/infinite damage |
| `Runtime.BufferedDodge` | Mid-jump and cooldown input, last valid direction, one-time consumption |
| `Runtime.DamageGraceAndGameOver` | One hit during immunity, subsequent hits, fatal damage, frozen final score |
| `Runtime.EndlessRunAndRecycling` | 100 jumps / 180 metres, capped combo, health milestones, fixed scenery instance counts |
| `Runtime.RockfallTelegraph` | Five warning/release/clear cycles, escape time, one active hazard, sphere physics setup, random delay bounds |

Actor tests use an isolated preview world and do not write player saves. The director test advances its schedule deterministically; it verifies physics configuration rather than pretending to benchmark Chaos simulation.

## Runtime visual check

Launched the actual `EndlessClimb` map through `UnrealEditor-Cmd -game -RenderOffscreen -VynixCapture`. Inspected captures of the initial grip, lateral jump, red rockfall countdown, health loss, game over, pause, and restart. The process exited successfully.

- Cliff materials render with instancing enabled; decorative rocks leave the player and both lanes visible.
- The Canvas text renders at its display size. Score, health, direction hints, and menu controls are readable.
- Pause freezes survival time; resume continues the same run.
- Restart reloads the level and restores full health, zero height, and active run state.
- The capture produces no game errors and does not modify persistent best-run records.

![Game-over UI](Images/game-over.png)

## Limits of verification

This does not establish Android-device performance, physical-controller compatibility, or multi-day runtime stability. The original free-climbing prototype remains available, but the automated gameplay suite targets the new endless mode. Packaged distribution still requires Unreal's normal cook/package step.

The `Fab` browser is disabled only in the headless test process because restoring its editor tab without a rendering interface crashes the installed engine plugin. It remains available during normal editing.
