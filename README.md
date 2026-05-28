# TechnoMancy

An Unreal Engine 5.7 plugin that lets players program their weapons with an in-game scripting language called **RuneScript**. Spells are authored as text, compiled at runtime, and drive game effects (projectile spawns, AoE damage, status effects) through a registry of C++ library functions that game developers can override or extend.

This README covers two audiences:

- **Players** writing spells — see [RuneScript Language](#runescript-language) and [Library Reference](#library-reference). Or jump to [EXAMPLE_SPELLS.md](EXAMPLE_SPELLS.md) for a cookbook of working spells.
- **Game developers** integrating the plugin — see [Quick Start](#quick-start-for-game-developers) and [Integration Guide](#integration-guide). For extending the library system, see [ADDING_LIBRARIES.md](ADDING_LIBRARIES.md).

---

## Table of Contents

1. [Quick Start (for game developers)](#quick-start-for-game-developers)
2. [Architecture](#architecture)
3. [RuneScript Language](#runescript-language)
4. [Library Reference](#library-reference)
   - [Elemental libraries](#elemental-libraries) — `fire`, `ice`, `lightning`, `void`, `light`, `earth`, `wind`, `bio`
   - [Helper libraries](#helper-libraries) — `mana`, `math`, `target`, `self`, `time`, `status`, `chain`
5. [Integration Guide](#integration-guide)
6. [Customisation & Extension](#customisation--extension)
7. [Multiplayer Notes](#multiplayer-notes)
8. [Console Commands](#console-commands)
9. [Phase Status](#phase-status)

---

## Quick Start (for game developers)

### 1. Install

Clone this repo into your project's `Plugins/` folder:

```
YourGame/
  Plugins/
    TechnoMancy/              <- this repo (rename folder to "TechnoMancy" after cloning)
      TechnoMancy.uplugin
      Source/...
  YourGame.uproject
```

Right-click `YourGame.uproject` → "Generate Visual Studio project files", then rebuild.

Add the plugin to your `.uproject`:

```json
"Plugins": [
  { "Name": "TechnoMancy", "Enabled": true }
]
```

And to your game module's `.Build.cs` if you'll call the API from C++:

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core", "CoreUObject", "Engine", "TechnoMancy"
});
```

### 2. Add the weapon component

In your weapon Blueprint:

1. **Add Component → Rune Weapon**
2. In the Details panel, either:
   - Set **Spell Asset** to a `URuneSpellAsset` from the Content Browser, **or**
   - Set **Spell Source** directly to a RuneScript string
3. Optionally set **Projectile Class** to your own subclass of `ARuneProjectile` (for meshes / VFX)
4. Adjust **Storage Capacity** (default 128 chars) and **Max Mana** (default 50)

### 3. Invoke spells

Wire your input or game logic to call these on the component:

| Function | Called when |
| --- | --- |
| `Invoke Primary` | main fire / trigger pull |
| `Invoke On Hit` | projectile hit (after `Set Last Hit`) |
| `Invoke On Kill` | target died |
| `Invoke Tick` | every frame, automatically, if `Allow Tick Spell = true` |

Each `Invoke` function takes an `Out Errors` array — populate it from your error display logic (or ignore for now).

### Hello, world

A minimal fireball weapon:

```
import runes.fire
Fn Primary() {
  fire.shot(spd: 12, dmg: 15)
}
```

Bind your fire input to `Invoke Primary` on the component. Pull the trigger — an `ARuneProjectile` (the default invisible base) spawns at 1200 cm/s and applies 15 damage on hit. Subclass `ARuneProjectile` to add visuals.

---

## Architecture

```
TechnoMancy (Runtime module)
+-- RuneScript language       [Lexer -> Parser -> Interpreter]
+-- FRuneLibraryRegistry      [Singleton: namespace.method -> callback]
+-- Built-in libraries        [15 namespaces, 109 functions]
+-- URuneSpellAsset           [UDataAsset spell wrapper]
+-- URuneWeaponComponent      [Drop-in component, server-authoritative + replicated]
+-- ARuneProjectile           [Base projectile actor]
+-- URuneScriptBPLib          [Blueprint facade]
+-- ARuneLibraryInitialiser   [Optional BP-extensible registration actor]
+-- URuneCodeEditor           [UMG widget: SMultiLineEditableTextBox + marshaller]
+-- URuneTerminalWidget       [In-game terminal UUserWidget orchestrator]
+-- FRuneSyntaxHighlightMarshaller  [Slate text-layout marshaller for highlighting]

TechnoMancyEditor (Editor module — stripped from packaged builds)
+-- URuneSpellAssetFactory    [Content Browser "Add New" entry]
+-- FRuneSpellAssetActions    [Asset type registration]
+-- URuneEditorTerminalWidget [Designer authoring UEditorUtilityWidget base]
```

### Source layout

```
TechnoMancy/
  TechnoMancy.uplugin
  Source/TechnoMancy/
    TechnoMancy.Build.cs
    Public/         (engine + plugin API surface)
    Private/        (implementations + library bodies)
```

### Key types

| Type | Where it lives | What it does |
| --- | --- | --- |
| `FRuneInterpreter` | `RuneInterpreter.h` | Tree-walking interpreter; one per `URuneWeaponComponent` |
| `FRuneLibraryRegistry` | `RuneLibraryRegistry.h` | Singleton of `namespace.method` -> callback bindings |
| `FRuneWeaponContext` | `RuneTypes.h` | Per-invocation runtime state (owner, last hit, mana, aim) |
| `FRuneLibFunction` | `RuneTypes.h` | A registered library entry: mana cost, description, callback |
| `URuneSpellAsset` | `RuneSpellAsset.h` | UDataAsset holding source + capacity + mana caps |
| `URuneWeaponComponent` | `RuneWeaponComponent.h` | The component you drop on weapons |
| `ARuneProjectile` | `RuneProjectile.h` | Base projectile spawned by `*.shot` / `*.lance` / etc. |
| `URuneScriptBPLib` | `RuneScriptBPLib.h` | BP-callable validation / introspection helpers |

---

## RuneScript Language

RuneScript is dynamically typed, with implicit variable declaration (GML-style) and mandatory braces on control flow (C-style).

### Imports

Elemental libraries (`fire`, `ice`, `lightning`, `void`, `light`, `earth`, `wind`, `bio`) must be imported before use:

```
import runes.fire
import runes.ice
```

Helper libraries (`mana`, `math`, `target`, `self`, `time`, `status`, `chain`) are always available — no import needed.

Calling a method from a namespace you haven't imported is a runtime error with a helpful message:

```
Line 6: 'void' is not imported -- add 'import runes.void' at the top
```

### Entry-point functions

`URuneWeaponComponent` looks for these four function names. Define the ones your spell uses; the rest are silent no-ops.

| Function | Invoked by | Use for |
| --- | --- | --- |
| `Fn Primary()` | `InvokePrimary` | Main fire action |
| `Fn OnHit()` | `InvokeOnHit` (after `SetLastHit`) | What happens when your projectile lands |
| `Fn OnKill()` | `InvokeOnKill` | What happens when your hit kills a target |
| `Fn Tick()` | every frame, if `bAllowTickSpell = true` | Persistent / channeled effects |

A complete spell with all four:

```
import runes.fire
import runes.bio

Fn Primary() {
  fire.shot(spd: 12, dmg: 10)
}

Fn OnHit() {
  fire.burst(dmg: 8, radius: 3)
  bio.poison(dmg_per_sec: 4, duration: 5)
}

Fn OnKill() {
  mana.steal(gain: 10)
}

Fn Tick() {
  // runs every frame -- be careful with mana
}
```

### Helper functions

Declare extra functions for reuse:

```
Fn DoubleShot() {
  fire.shot()
  fire.shot()
}

Fn Primary() {
  DoubleShot()
}
```

Functions can take positional arguments and `return` a value:

```
Fn ShotDamage(base) {
  return base + 5
}

Fn Primary() {
  d = ShotDamage(10)   // d == 15
  fire.shot(dmg: d)
}
```

### Variables

No type declaration. Just assign:

```
x = 3
charge = 0.5
ready = true
```

Undefined variables read as `0`. Variables are scoped per-invocation — they reset every time `Primary` / `OnHit` / etc. fires.

### Control flow

All `if`, `else`, `while`, `for` bodies require braces. Missing braces is a compile error.

```
if x > 5 {
  fire.shot()
}

if x > 5 {
  fire.shot()
} else if x > 2 {
  ice.shot()
} else {
  void.shot()
}

while mana.current() > 10 {
  fire.shot()
}

for i in 0..3 {
  fire.shot()
}
```

`for i in N..M` is exclusive on the upper bound. `for i in 5..2` runs zero times (silent no-op).

### Operators

| Category | Operators |
| --- | --- |
| Arithmetic | `+`, `-`, `*`, `/`, `%` |
| Comparison | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| Logical | `&&`, `\|\|`, `!` |
| Assignment | `=` |

`&&` and `||` short-circuit.

### Library calls

Library calls use `namespace.method(named: args)`. **Arguments must be named** — `fire.shot(9)` is a runtime error; use `fire.shot(spd: 9)`.

```
fire.shot(spd: 12, dmg: 8, pierce: true)
ice.lance(spd: 14, dmg: 18, pierce_count: 3)
```

Argument order is irrelevant. Omitted arguments use the library's defaults (listed in the [Library Reference](#library-reference)).

### Comments

`//` to end of line:

```
// charge up
for i in 0..3 {
  fire.shot()     // 8 mana each
}
```

### Limits

| Limit | Value |
| --- | --- |
| Instructions per invocation | 10,000 (crash beyond) |
| Call stack depth | 32 |
| Identifier length | 64 chars |
| Spell source length | `StorageCapacity` on the component (default 128) |

Going over the instruction limit raises a "CRASH: Execution limit reached" error — useful for catching accidental infinite loops.

### Common patterns

**Conditional spending:**

```
import runes.fire

Fn Primary() {
  if mana.pct() > 0.5 {
    fire.burst(dmg: 25, radius: 4)
  } else {
    fire.shot()
  }
}
```

**Charge-up multi-shot:**

```
import runes.lightning

Fn Primary() {
  shots = math.floor(mana.pct() * 5)
  for i in 0..shots {
    lightning.bolt(spd: 20, dmg: 6)
  }
}
```

**Finisher on low-HP targets:**

```
import runes.void

Fn OnHit() {
  if target.health_pct() < 0.2 {
    void.detonate(radius: 4, dmg: 30)
  } else {
    void.mark(duration: 5, amp: 1.5)
  }
}
```

---

## Library Reference

Every callable function in the system. Parameter defaults are shown after `=`. The `Real` column marks functions that drive real game effects in the current plugin build — functions marked `stub` log the call but otherwise do nothing, and game developers are expected to override them ([see Customisation](#customisation--extension)) once they have status / effect systems in place.

### Elemental libraries

Require `import runes.<name>`.

#### `runes.fire`

| Function | Mana | Params (defaults) | Returns | Real | Description |
| --- | ---: | --- | --- | --- | --- |
| `fire.shot` | 8 | `spd=10, dmg=8, pierce=false, size=1` | nil | yes | Spawns a fire projectile |
| `fire.burst` | 15 | `dmg=15, radius=3, falloff=0.5` | nil | yes | AoE explosion at last hit position |
| `fire.dot` | 6 | `dmg_per_sec=5, duration=3` | nil | stub | Apply burn DoT to last hit target |
| `fire.wall` | 20 | `width=4, height=3, duration=5` | nil | stub | Spawn fire wall in front of owner |
| `fire.nova` | 18 | `dmg=12, radius=5` | nil | yes | 360-degree fire explosion from owner |
| `fire.trail` | 10 | `dmg=3, duration=4` | nil | stub | Leave damage fire trail on ground |
| `fire.homing` | 14 | `spd=8, dmg=12, turn_rate=3` | nil | yes | Self-guided homing fireball |
| `fire.pillar` | 16 | `dmg=20, duration=3` | nil | yes | Summon fire pillar at last hit pos |

#### `runes.ice`

| Function | Mana | Params (defaults) | Returns | Real | Description |
| --- | ---: | --- | --- | --- | --- |
| `ice.shot` | 7 | `spd=9, dmg=6, slow_pct=0.3, slow_dur=2` | nil | yes | Slowing ice projectile |
| `ice.freeze` | 12 | `duration=2` | nil | stub | Freeze last hit target solid |
| `ice.shatter` | 10 | `dmg=20` | nil | yes | Hit target burst damage |
| `ice.nova` | 16 | `radius=4, slow_pct=0.4, slow_dur=2` | nil | yes | Radial ice slow burst |
| `ice.wall` | 18 | `width=4, height=3, duration=6` | nil | stub | Solid ice barrier |
| `ice.spikes` | 14 | `count=3, dmg=10` | nil | yes | Ice spikes from ground at target |
| `ice.armor` | 10 | `absorption=20, duration=6` | nil | stub | Ice armor buff on self |
| `ice.lance` | 12 | `spd=12, dmg=14, pierce_count=2` | nil | yes | Piercing ice lance |

#### `runes.lightning`

| Function | Mana | Params (defaults) | Returns | Real | Description |
| --- | ---: | --- | --- | --- | --- |
| `lightning.bolt` | 9 | `spd=20, dmg=10` | nil | yes | Fast lightning bolt |
| `lightning.chain` | 14 | `jumps=3, dmg=12, falloff=0.7` | nil | stub | Chain lightning between nearby enemies |
| `lightning.stun` | 8 | `duration=1.5` | nil | stub | Stun last hit target |
| `lightning.storm` | 25 | `radius=8, strikes=5, dmg=8` | nil | yes | Lightning storm AoE (N random strikes) |
| `lightning.arc` | 12 | `angle=45, range=6, dmg=10` | nil | yes | Cone lightning arc (line trace) |
| `lightning.overload` | 20 | `dmg_mult=2, duration=3` | nil | stub | Buff next hit's damage |
| `lightning.static` | 10 | `charges=3, dmg=6` | nil | stub | Apply static charges to target |
| `lightning.pulse` | 16 | `radius=4, dmg=8, stun_dur=0.5` | nil | yes | EMP radial pulse |

#### `runes.void`

| Function | Mana | Params (defaults) | Returns | Real | Description |
| --- | ---: | --- | --- | --- | --- |
| `void.shot` | 8 | `spd=9, dmg=7, pierce=false` | nil | yes | Void projectile |
| `void.phase` | 12 | `duration=0.5` | nil | stub | Brief owner invulnerability |
| `void.mark` | 5 | `duration=5, amp=1.5` | nil | stub | Mark target; amplifies damage received |
| `void.slow` | 6 | `amt=0.4, duration=2` | nil | stub | Slow last hit target |
| `void.echo` | 8 | `delay=0.2, dmg_mult=0.8` | nil | stub | Delayed repeat of last shot |
| `void.siphon` | 0 | `dmg=10, mana_gain=8` | float | yes | Deal dmg, refund mana |
| `void.rift` | 22 | `radius=5, duration=4` | nil | stub | Gravity rift pulling enemies |
| `void.detonate` | 18 | `radius=4, dmg=18` | nil | yes | Void explosion at last hit pos |

#### `runes.light`

| Function | Mana | Params (defaults) | Returns | Real | Description |
| --- | ---: | --- | --- | --- | --- |
| `light.beam` | 10 | `range=12, dmg=8, width=0.5` | nil | yes | Continuous damage beam (one frame line trace) |
| `light.construct` | 16 | `type=0, duration=6` | nil | stub | 0=shield wall, 1=platform, 2=cage |
| `light.burst` | 12 | `dmg=10, radius=4, blind_dur=1` | nil | yes | Flash burst at last hit pos |
| `light.shield` | 8 | `absorption=15, duration=4` | nil | stub | Personal hardlight shield |
| `light.lance` | 11 | `spd=15, dmg=12` | nil | yes | Fast piercing hardlight lance |
| `light.prism` | 18 | `bounces=3, dmg=8` | nil | yes | Bouncing light projectile |
| `light.beacon` | 14 | `duration=5, pulse_dmg=5` | nil | stub | Pulsing damage beacon at target pos |
| `light.blade` | 12 | `dmg=15, range=2, arc=90` | nil | yes | Melee hardlight slash (short line trace) |

#### `runes.earth`

| Function | Mana | Params (defaults) | Returns | Real | Description |
| --- | ---: | --- | --- | --- | --- |
| `earth.spike` | 10 | `dmg=12, height=3` | nil | yes | Stone spike from ground at target |
| `earth.wall` | 18 | `width=5, height=4, duration=8` | nil | stub | Stone barrier wall |
| `earth.shockwave` | 15 | `range=6, dmg=8, knockback=500` | nil | yes | Ground shockwave radial (damage + knockback) |
| `earth.boulder` | 12 | `spd=7, dmg=20, size=1` | nil | yes | Rolling boulder projectile |
| `earth.quake` | 22 | `radius=8, dmg=6, duration=3` | nil | yes | Earthquake AoE around owner |
| `earth.armor` | 10 | `absorption=25, duration=8` | nil | stub | Stone armor buff on self |
| `earth.pull` | 8 | `force=600` | nil | stub | Gravitational pull on last hit target |
| `earth.raise` | 20 | `count=3, duration=6` | nil | stub | Raise terrain obstacles |

#### `runes.wind`

| Function | Mana | Params (defaults) | Returns | Real | Description |
| --- | ---: | --- | --- | --- | --- |
| `wind.blast` | 8 | `range=5, force=600, dmg=5` | nil | yes | Knockback blast in aim direction |
| `wind.dash` | 10 | `dist=500, dir_x=0, dir_z=0` | nil | stub | Owner dash |
| `wind.vortex` | 16 | `radius=4, duration=4` | nil | stub | Wind vortex trapping enemies |
| `wind.gust` | 6 | `force=400` | nil | stub | Push single last hit target |
| `wind.shield` | 12 | `deflect_pct=0.3, duration=4` | nil | stub | Projectile deflection shield |
| `wind.haste` | 8 | `speed_mult=1.5, duration=4` | nil | stub | Owner movement speed boost |
| `wind.tornado` | 24 | `radius=5, dmg=4, duration=6` | nil | yes | Tornado AoE at last hit pos |
| `wind.shot` | 7 | `spd=14, dmg=6, knockback=300` | nil | yes | Knockback wind projectile |

#### `runes.bio`

| Function | Mana | Params (defaults) | Returns | Real | Description |
| --- | ---: | --- | --- | --- | --- |
| `bio.poison` | 8 | `dmg_per_sec=4, duration=5` | nil | stub | Poison DoT on last hit target |
| `bio.drain` | 14 | `dmg=10, health_gain=5` | float | yes | Life drain; returns health gained |
| `bio.spores` | 12 | `radius=4, duration=5, slow_pct=0.3` | nil | yes | Spore cloud AoE at last hit pos |
| `bio.regenerate` | 10 | `hp_per_sec=5, duration=5` | nil | stub | Self-heal over time |
| `bio.mutate` | 16 | `dmg_amp=1.25, duration=4` | nil | stub | Weaken target; amplifies damage taken |
| `bio.plague` | 20 | `spread_radius=6, duration=6` | nil | stub | Spreading plague to nearby enemies |
| `bio.cocoon` | 18 | `duration=3, hp_per_sec=10` | nil | stub | Stun self but regenerate rapidly |
| `bio.sap` | 8 | `slow_pct=0.3, duration=3` | nil | stub | Weaken and slow last hit target |

### Helper libraries

No import required. All return real values; nothing here is a stub.

#### `mana`

| Function | Mana | Params (defaults) | Returns | Description |
| --- | ---: | --- | --- | --- |
| `mana.steal` | 0 | `gain=5` | float | Add mana to pool; returns new total (clamped at `MaxMana`) |
| `mana.burst` | 0 | `dmg_mult=1.5` | float | Drain all mana to 0; returns the multiplier (use it for one big effect) |
| `mana.save` | 0 | `pct=0.2` | nil | Reduce next spell cost by pct (stub flag, see Customisation) |
| `mana.current` | 0 | — | float | Current mana value |
| `mana.max` | 0 | — | float | Max mana value |
| `mana.pct` | 0 | — | float | Current / Max (0..1) |

#### `math`

All return real values. All cost 0 mana.

| Function | Params (defaults) | Returns | Description |
| --- | --- | --- | --- |
| `math.rnd` | `lo=0, hi=1` | float | Random float in `[lo, hi]` |
| `math.rndi` | `lo=0, hi=10` | float | Random integer in `[lo, hi]` (truncated) |
| `math.clamp` | `val, lo, hi` | float | Clamp `val` between `lo` and `hi` |
| `math.lerp` | `a, b, t` | float | Linear interpolation |
| `math.abs` | `val` | float | Absolute value |
| `math.sin` | `val` | float | Sine (radians) |
| `math.cos` | `val` | float | Cosine (radians) |
| `math.floor` | `val` | float | Floor |
| `math.ceil` | `val` | float | Ceiling |
| `math.pow` | `base, exp` | float | Power |
| `math.min` | `a, b` | float | Minimum |
| `math.max` | `a, b` | float | Maximum |

#### `target`

Information about the last hit target. Position-based queries use `LastHitPos` set by `SetLastHit`.

| Function | Mana | Params (defaults) | Returns | Description |
| --- | ---: | --- | --- | --- |
| `target.count` | 0 | `radius=8` | float | Count actors within `radius` (designer units) of last hit pos |
| `target.health_pct` | 0 | — | float | 1.0 if target is set, 0.0 if not (override for real HP) |
| `target.distance` | 0 | — | float | World distance from owner to last hit target |
| `target.is_frozen` | 0 | — | float | 1 if frozen, 0 otherwise (override for real status check) |
| `target.is_burning` | 0 | — | float | 1 if burning (override) |
| `target.is_marked` | 0 | — | float | 1 if void-marked (override) |
| `target.is_stunned` | 0 | — | float | 1 if stunned (override) |
| `target.is_poisoned` | 0 | — | float | 1 if poisoned (override) |
| `target.apply_force` | 4 | `force=400, dir=0` | nil | Apply physics impulse (override) |

#### `self`

Information about / buffs on the weapon owner.

| Function | Mana | Params (defaults) | Returns | Description |
| --- | ---: | --- | --- | --- |
| `self.health_pct` | 0 | — | float | Owner HP as 0..1 (override for real HP) |
| `self.mana_pct` | 0 | — | float | Same as `mana.pct()` |
| `self.is_moving` | 0 | — | float | 1 if owner velocity > 10 |
| `self.speed` | 0 | — | float | Owner current speed (cm/s) |
| `self.heal` | 8 | `amount=10` | float | Heal owner (override to wire to your HP system); returns amount |
| `self.boost_speed` | 8 | `mult=1.5, duration=3` | nil | Temporary speed buff (override) |
| `self.boost_damage` | 10 | `mult=1.5, duration=3` | nil | Temporary damage buff (override) |

#### `time`

| Function | Mana | Params (defaults) | Returns | Description |
| --- | ---: | --- | --- | --- |
| `time.elapsed` | 0 | — | float | Seconds since weapon drawn (override; returns 0 by default) |
| `time.game_time` | 0 | — | float | World time in seconds |
| `time.sin_wave` | 0 | `freq=1, amp=1` | float | `sin(game_time * freq) * amp` |
| `time.slow_field` | 20 | `radius=4, factor=0.5, duration=3` | nil | Time dilation field (override) |

#### `status`

Generic status effect management. All stubs — wire up to your status system.

| Function | Mana | Params (defaults) | Returns | Description |
| --- | ---: | --- | --- | --- |
| `status.apply` | 5 | `type, duration, power=1` | nil | Types: 0=burn,1=freeze,2=stun,3=slow,4=poison,5=mark,6=bleed |
| `status.remove` | 3 | `type` | nil | Remove status type from target |
| `status.has` | 0 | `type` | float | 1 if target has status, 0 otherwise |
| `status.count` | 0 | — | float | Number of active statuses on target |

#### `chain`

Projectile modifier flags. These are stubs — wiring them requires per-projectile state that the plugin doesn't model out of the box. Override in your projectile subclass.

| Function | Mana | Params (defaults) | Returns | Description |
| --- | ---: | --- | --- | --- |
| `chain.bounce` | 5 | `count=2, dmg_falloff=0.7` | nil | Next projectile bounces N times |
| `chain.pierce` | 4 | `count=2` | nil | Next projectile pierces N targets |
| `chain.split` | 8 | `count=2, angle=15` | nil | Next projectile splits on first hit |

### Partial implementations

These functions are marked "real" because they invoke a real effect helper, but their full intended behaviour requires game-side systems the plugin doesn't ship with. Override them once you have:

| Function | What works today | What's still missing |
| --- | --- | --- |
| `fire.homing` | Spawns a fire projectile | No homing / tracking logic |
| `light.prism` | Spawns a light projectile | No bouncing |
| `wind.tornado` | One AoE burst at last hit pos | No sustained 6-second damage field |
| `bio.spores` | One AoE burst at last hit pos | No slow / DoT cloud |
| `lightning.storm` | N random burst-AoEs in radius | No visible lightning strikes |
| `earth.quake` | One AoE around owner | No sustained 3-second damage |
| `earth.shockwave` | Damage + knockback radial | Ground-impact VFX is up to your projectile subclass |

### Unit conventions

Library helpers (`SpawnProjectile`, `DamageSphere`, `LineDamage`, `ApplyKnockback`) multiply radius / range / speed inputs by `UnitScale = 100` when translating to Unreal cm. So `radius: 5` is a 500 cm (5 m) sphere, `spd: 12` is 1200 cm/s. Damage values pass through unchanged — they are abstract HP. If your project uses a different scale, override the affected library callbacks (see [Customisation](#customisation--extension)).

---

## Integration Guide

### Wiring up a weapon (C++)

```cpp
// AYourWeapon.h
#include "RuneWeaponComponent.h"

UCLASS()
class AYourWeapon : public AActor {
    GENERATED_BODY()
public:
    AYourWeapon();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="RuneScript")
    TObjectPtr<URuneWeaponComponent> RuneComp;

    UFUNCTION(BlueprintCallable)
    void OnFire();

    UFUNCTION(BlueprintCallable)
    void HandleProjectileHit(AActor* HitActor, FVector HitPos);
};

// AYourWeapon.cpp
AYourWeapon::AYourWeapon() {
    RuneComp = CreateDefaultSubobject<URuneWeaponComponent>(TEXT("RuneScript"));
}

void AYourWeapon::OnFire() {
    TArray<FString> Errors;
    RuneComp->InvokePrimary(Errors);
    // Optionally surface errors to UI
}

void AYourWeapon::HandleProjectileHit(AActor* HitActor, FVector HitPos) {
    RuneComp->SetLastHit(HitActor, HitPos);
    TArray<FString> Errors;
    RuneComp->InvokeOnHit(Errors);
}
```

### Wiring up a weapon (Blueprint)

The same flow, all visual nodes:

1. On weapon construction, **Add Component → Rune Weapon**
2. On trigger input, call **Invoke Primary** on the component
3. On projectile hit event:
   1. Call **Set Last Hit** with the hit actor and hit position
   2. Call **Invoke On Hit**

### Creating a spell asset

Two paths:

**Asset-based (recommended for designer-authored spells):**

1. Content Browser → right-click → **Miscellaneous → Data Asset**
2. Pick **RuneSpellAsset**
3. Open the asset, set **Source Code**, **Storage Capacity**, **Max Mana**, **Display Name**, **Description**
4. Assign to the component's **Spell Asset** property

The component pulls these values on `BeginPlay`. Component-side `Storage Capacity` and `Max Mana` overrides take precedence when set to non-default values.

**Runtime-authored (for player-typed spells, modded content, generated content):**

```cpp
#include "RuneScriptBPLib.h"

URuneSpellAsset* Spell = URuneScriptBPLib::CreateRuneSpellAsset(
    /*WorldContextObject=*/ this,
    TEXT("import runes.fire\nFn Primary() { fire.shot() }"),
    /*StorageCapacity=*/ 256,
    /*MaxMana=*/ 100.f);

RuneComp->SpellAsset = Spell;
```

Or compile directly without an asset:

```cpp
TArray<FString> Errors;
bool bOk = RuneComp->CompileSpell(NewSourceFromPlayerInput, Errors);
```

### Tick spells

Set `bAllowTickSpell = true` on the component to fire `Tick(dt)` every frame.

```
import runes.fire

Fn Tick() {
  // every frame
  if math.rnd(lo: 0, hi: 1) < 0.05 {
    fire.shot(dmg: 2)
  }
}
```

Be careful with mana — a Tick that calls a mana-positive lib every frame will quickly hit 0 (insufficient mana raises an error, halting the spell). The 10,000-instruction limit is per-tick, so very large loops in Tick will crash the spell.

### Custom projectile classes

Subclass `ARuneProjectile`:

```cpp
UCLASS()
class AYourFireballProjectile : public ARuneProjectile {
    GENERATED_BODY()
public:
    AYourFireballProjectile();

protected:
    virtual void BeginPlay() override;
};

AYourFireballProjectile::AYourFireballProjectile() {
    // Add mesh, particles, audio in your constructor
}

void AYourFireballProjectile::BeginPlay() {
    Super::BeginPlay();
    // Drive visuals from Element ("fire", "ice", "lightning", etc.)
    // Element / Damage / PierceCount are already set by the spawn helper
}
```

Set `URuneWeaponComponent::ProjectileClass` to your subclass in the details panel. All `*.shot` / `*.lance` / `*.bolt` / `*.boulder` calls will spawn it.

### Mana management

Mana doesn't auto-regen. Options:

- Call `RuneComp->RefillMana()` on a cooldown timer
- Have your game's existing mana system write directly to `CurrentMana`
- Use the `mana.steal` library function inside spells for "lifesteal" mana mechanics

### Mana semantics

- Library functions deduct `ManaCost` **before** running the callback
- A call where `CurrentMana < ManaCost` raises a runtime error; the call does not run and the spell halts
- The remaining `CurrentMana` stays where it was (no partial deduction)
- `mana.burst` is special — it drains all mana to 0 and returns a multiplier

### Static mana estimate

Before flashing a spell, you can compute its mana cost statically:

```cpp
float Cost = URuneScriptBPLib::GetManaEstimate(Source, FName("Primary"));
```

This sums all library-call costs in the parsed AST. Loops and branches are summed once (no runtime simulation), so it's a lower bound for branchy spells and an exact answer for straight-line code.

---

## Customisation & Extension

### Overriding library callbacks

The most powerful extension point. Re-register against the same key to replace a built-in's behaviour:

```cpp
#include "RuneLibraryRegistry.h"

void UMyGameInstance::Init() {
    Super::Init();
    auto& Reg = FRuneLibraryRegistry::Get();

    // Replace fire.shot with a project-specific spawn
    Reg.RegisterSimple(
        FName("fire"), FName("runes.fire"), FName("shot"),
        /*requires_import=*/ true,
        /*mana_cost=*/ 8.f,
        TEXT("Project-specific fire shot"),
        TEXT("spd:float=10, dmg:float=8"),
        [](const FRuneLibCallArgs& Args, FRuneWeaponContext& Ctx) -> FRuneValue {
            // Your custom logic
            const float Spd = (float)Args.GetFloat(FName("spd"), 10.0);
            const float Dmg = (float)Args.GetFloat(FName("dmg"), 8.0);
            // ... spawn your own projectile, play VFX, etc.
            return FRuneValue::MakeNil();
        });
}
```

The last registration wins. Built-ins register at module startup (`LoadingPhase: Default`), so override later — `UGameInstance::Init` is the canonical place. You can also use the `ARuneLibraryInitialiser` actor's `RegisterAdditionalLibraries` BP event for level-scoped customisation.

### Wiring up the stub libraries

Most non-projectile elemental functions (`*.dot`, `*.wall`, `*.freeze`, `*.armor`, etc.) ship as stubs because they need a status / effect system the plugin doesn't ship with. Wire them to your game's systems via the same override pattern:

```cpp
Reg.RegisterSimple(
    FName("fire"), FName("runes.fire"), FName("dot"),
    true, 6.f,
    TEXT("Apply burn DoT"), TEXT("dmg_per_sec:float=5, duration:float=3"),
    [](const FRuneLibCallArgs& Args, FRuneWeaponContext& Ctx) -> FRuneValue {
        const float Dps = (float)Args.GetFloat(FName("dmg_per_sec"), 5.0);
        const float Dur = (float)Args.GetFloat(FName("duration"), 3.0);
        AActor* Target = Ctx.LastHitTarget.Get();
        if (!Target) return FRuneValue::MakeNil();
        // Hand off to your status effect manager:
        // UMyStatusManager::Get(Target->GetWorld())->ApplyBurn(Target, Dps, Dur);
        return FRuneValue::MakeNil();
    });
```

The same pattern wires `target.is_*` and `status.*` to read from your status manager.

### Adding new libraries

You can register entirely new namespaces — call them from RuneScript with `mycompany.mything(...)`:

```cpp
Reg.RegisterSimple(
    FName("mycompany"), FName("runes.mycompany"), FName("smite"),
    /*requires_import=*/ true,
    /*mana_cost=*/ 20.f,
    TEXT("Custom smite ability"),
    TEXT("radius:float=5, dmg:float=20"),
    [](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue {
        const float R = (float)A.GetFloat(FName("radius"), 5.0);
        const float D = (float)A.GetFloat(FName("dmg"), 20.0);
        // ...
        return FRuneValue::MakeNil();
    });
```

Players then write `import runes.mycompany` and call `mycompany.smite(radius: 8, dmg: 30)`.

For helper-style libraries that don't require import, pass `false` to the `requires_import` flag.

### Using shared effect helpers

If you're writing custom callbacks and want to reuse the built-in spawn / damage logic:

```cpp
#include "RuneEffectHelpers.h"

// In your callback
TechnoMancy::Effects::SpawnProjectile(Ctx, FName("fire"), /*spd=*/12, /*dmg=*/8);
TechnoMancy::Effects::DamageSphere(Ctx, Ctx.LastHitPos, /*radius=*/5, /*dmg=*/15);
TechnoMancy::Effects::LineDamage(Ctx, /*range=*/12, /*dmg=*/8);
```

(`RuneEffectHelpers.h` is a private header — to use it from your game module, copy the helpers into your own translation unit, or refactor it to `Public/` in your fork. The interfaces are stable.)

### Blueprint introspection

`URuneScriptBPLib` exposes:

| BP node | Description |
| --- | --- |
| `Validate Spell Source` | Returns true if the source would compile; populates an error list otherwise |
| `Get Library Function List` | All registered library entries (namespace, method, mana cost, description, params) |
| `Get Mana Estimate` | Static cost calculation for a named entry point in a source string |
| `Create Rune Spell Asset` | Transient asset (in-memory only) |
| `Get Spell Character Count` | `Source.Len()` |
| `Get Spell Storage Remaining` | `StorageCapacity - Source.Len()` |

Use these to power a terminal UI's "Validate", "Estimate Mana", and autocomplete features.

---

## Multiplayer Notes

The interpreter is **server-authoritative**. The component:

- Replicates `CurrentMana` and `SpellSource` (`UPROPERTY(Replicated)` and `ReplicatedUsing=OnRep_SpellSource`)
- Exposes `RequestCompile(NewSource)` as the client-side entry point. Calling it on a client forwards to `Server_RequestCompile`. Calling it on the server compiles directly.
- Fires `OnCompileResult(bSuccess, Errors)` on the originating client after `Server_RequestCompile` finishes.
- Fires `OnSpellSourceChanged(NewSource)` on every client when `SpellSource` replicates.
- Short-circuits all `Invoke*` calls on clients (`GetOwnerRole() != ROLE_Authority` returns `true` immediately).

### Client/server flow

A typical "player flashes a new spell" sequence:

1. Player types in `URuneTerminalWidget`, clicks **Flash**
2. Widget calls `WeaponComponent->RequestCompile(source)`
3. On a client, this triggers `Server_RequestCompile_Implementation` on the server's component
4. Server validates `bAllowPlayerEdits`, compiles, updates `SpellSource` (replicated) and `bReady`
5. Server calls `Client_OnCompileResult_Implementation` back to the originating client
6. Client's `URuneTerminalWidget` displays the error list or "Flashed successfully"

The `URuneCodeEditor`'s **Simulate** button runs the lexer + parser locally — no RPC needed for syntax validation or static mana estimates.

### Locking spells to designer presets

Set `bAllowPlayerEdits = false` on the `URuneWeaponComponent` to lock the weapon to its `DefaultSpellAsset`. `Server_RequestCompile` rejects all incoming source with the error `"This weapon's spell is locked by the designer"`. The in-game terminal widget displays the source as read-only when bound to a locked weapon.

---

## Console Commands

| Command | Description |
| --- | --- |
| `runescript.test` | Runs the built-in self-test (compiles + runs a known program, asserts result is 4). Useful to verify the module loaded and the language pipeline is working. |

---

## Phase Status

All seven phases of the plugin (per the PRD) are now in place:

| Phase | Scope | Status |
| --- | --- | --- |
| 1 | Core language (lexer, parser, interpreter) | Complete |
| 2 | All 15 libraries registered with stubs | Complete |
| 3 | Weapon component, spell asset, BP library, real effects | Complete |
| 4 | Multiplayer replication (RPCs, OnRep, authority guards) | Complete |
| 5 | In-game terminal widget with syntax highlighting | Complete (C++) |
| 6 | Editor authoring + DefaultSpellAsset preset workflow | Complete (C++) |
| 7 | Docs + library extension guide + example spells | Complete |

**31 of 64 elemental library functions** are wired to real game effects (all projectile-spawning, AoE damage, beams, shockwaves, and the mana-bridging utilities `void.siphon` / `bio.drain`). The remaining 33 (walls, DoTs, status effects, buffs, movement, time dilation, chain modifiers) are intentionally logged stubs — game projects register their own callbacks against those keys (see [ADDING_LIBRARIES.md](ADDING_LIBRARIES.md)).

### What's still designer work

Some pieces of Phases 5 and 6 can only be authored inside the UE Editor and are not in this repo:

- `WBP_RuneTerminal.uasset` — Blueprint subclass of `URuneTerminalWidget`. Create in your project, lay out the UMG, and name child widgets to match the `BindWidget` properties (see [Creating WBP_RuneTerminal](#creating-wbp_runeterminal)).
- `EUW_RuneEditorTerminal.uasset` — Editor Utility Widget Blueprint subclass of `URuneEditorTerminalWidget`. Similar story for the editor authoring experience.
- An example map with a sample weapon BP + sample `DA_Spell_*` assets — left for the consuming project.

### Creating WBP_RuneTerminal

1. Content Browser → **Add New → User Interface → Widget Blueprint**
2. When prompted for parent class, pick **RuneTerminalWidget**
3. In the Designer tab, drag children into the canvas. Name them exactly:
   - `CodeEditor` — drag a **Rune Code Editor** widget (under TechnoMancy in the palette). Required.
   - `FlashButton` — a Button. Required.
   - `SimulateButton`, `ResetButton` — Buttons. Optional.
   - `CharCounterText`, `ManaEstimateText`, `ErrorListText`, `StatusText`, `SpellNameText` — Text Blocks. Optional.
4. Call `Set Weapon Component` from your HUD code with the player's active `URuneWeaponComponent`.

---

## License

To be determined.
