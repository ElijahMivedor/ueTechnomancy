# Example Spells

A cookbook of useful RuneScript spells, organised by complexity. Most are short enough to fit in the default 128-character storage cap — the few that don't note the required storage in a comment.

For language syntax see the [README](README.md#runescript-language). For function references see the [Library Reference](README.md#library-reference).

---

## Tier 0 — One-liners

The minimum viable spell. Suitable for a player's first weapon.

### Fireball

```
import runes.fire
Fn Primary() { fire.shot() }
```

### Ice lance

```
import runes.ice
Fn Primary() { ice.lance(spd: 14, dmg: 18) }
```

### Lightning bolt

```
import runes.lightning
Fn Primary() { lightning.bolt(spd: 20, dmg: 10) }
```

### Void shot

```
import runes.void
Fn Primary() { void.shot(spd: 9, dmg: 7) }
```

---

## Tier 1 — Two-stage spells

A primary fire plus an on-hit reaction.

### Sticky fire

```
import runes.fire

Fn Primary() {
  fire.shot()
}

Fn OnHit() {
  fire.burst(dmg: 8, radius: 3)
}
```

### Chill into shatter

```
import runes.ice

Fn Primary() {
  ice.shot(slow_pct: 0.5, slow_dur: 3)
}

Fn OnHit() {
  ice.shatter(dmg: 25)
}
```

### Mark and detonate

```
import runes.void

Fn Primary() {
  void.shot()
}

Fn OnHit() {
  void.mark(duration: 5, amp: 2)
  void.detonate(radius: 4, dmg: 15)
}
```

### Lifesteal shot

```
import runes.bio

Fn Primary() {
  bio.drain(dmg: 10, health_gain: 6)
}
```

---

## Tier 2 — Conditional logic

Spells that change behaviour based on state.

### Mana-conditional power shot

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

### Finisher on low HP

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

### Crowd-control selector

When facing many enemies, AoE; otherwise, single-target.

```
import runes.fire

Fn Primary() {
  if target.count(radius: 4) > 2 {
    fire.nova(dmg: 12, radius: 5)
  } else {
    fire.shot()
  }
}
```

### Speed-conditional dash

```
import runes.wind

Fn Primary() {
  if self.is_moving() == 1 {
    wind.blast(range: 5, force: 800)
  } else {
    wind.shot(spd: 14, dmg: 8)
  }
}
```

---

## Tier 3 — Loops & multi-shot

Spells that fire multiple times via `for` loops.

### Triple shot

```
import runes.fire

Fn Primary() {
  for i in 0..3 {
    fire.shot()
  }
}
```

Costs 24 mana (8 per shot). Will fail mid-loop if you run out.

### Charge-scaled barrage

The number of shots scales with current mana.

```
import runes.lightning

Fn Primary() {
  shots = math.floor(mana.pct() * 5)
  for i in 0..shots {
    lightning.bolt(spd: 20, dmg: 6)
  }
}
```

### Damage falloff bursts

```
import runes.fire

Fn OnHit() {
  for i in 0..3 {
    fire.burst(dmg: 15 - i * 4, radius: 3)
  }
}
```

---

## Tier 4 — Mana economies

Spells that generate or recycle mana to enable longer combos.

### Vampire spell

Drains health for mana, then spends it on damage.

```
import runes.void
import runes.fire

Fn Primary() {
  void.siphon(dmg: 10, mana_gain: 8)
  if mana.current() > 15 {
    fire.burst(dmg: 18, radius: 3)
  }
}
```

### Burst-then-recover

```
import runes.fire

Fn Primary() {
  m = mana.burst(dmg_mult: 1.8)
  fire.nova(dmg: 18 * m, radius: 5)
}

Fn OnKill() {
  mana.steal(gain: 20)
}
```

### Cycle: small shot regen

```
import runes.bio

Fn Primary() {
  bio.drain(dmg: 8, health_gain: 4)
  mana.steal(gain: 3)
}
```

---

## Tier 5 — Helper-function spells

Using `Fn` for custom helpers.

### Reusable scatter

Storage cap may need to be raised above 128.

```
// Capacity: 256
import runes.fire

Fn Spread(base_dmg) {
  for i in 0..3 {
    fire.shot(dmg: base_dmg)
  }
}

Fn Primary() {
  Spread(8)
}

Fn OnHit() {
  Spread(4)
}
```

### Recursive escalation

Note: max call depth is 32, so don't recurse too deeply.

```
// Capacity: 256
import runes.fire

Fn Burn(times) {
  if times > 0 {
    fire.shot()
    Burn(times - 1)
  }
}

Fn Primary() {
  Burn(3)
}
```

---

## Tier 6 — Tick spells

Spells that run every frame. Requires `Allow Tick Spell = true` on the component.

### Heat-up beam

```
// Capacity: 256
import runes.fire

Fn Tick() {
  if mana.current() > 5 {
    fire.shot(dmg: 1, spd: 30)
  }
}
```

### Pulse field

```
// Capacity: 256
import runes.lightning

Fn Tick() {
  pulse = time.sin_wave(freq: 4, amp: 1)
  if pulse > 0.8 && mana.current() > 16 {
    lightning.pulse(radius: 5, dmg: 6)
  }
}
```

### Aura — heal while standing still

```
// Capacity: 256
import runes.bio

Fn Tick() {
  if self.is_moving() == 0 && mana.current() > 10 {
    bio.regenerate(hp_per_sec: 5, duration: 1)
  }
}
```

---

## Tier 7 — Hybrid full kit

A complete spell using all four entry points.

### Elemental striker

```
// Capacity: 512
import runes.fire
import runes.void

Fn Primary() {
  if mana.pct() < 0.2 {
    void.siphon(dmg: 5, mana_gain: 10)
  } else {
    fire.shot(spd: 14, dmg: 12)
  }
}

Fn OnHit() {
  if target.health_pct() < 0.5 {
    void.detonate(radius: 4, dmg: 25)
  } else {
    fire.burst(dmg: 8, radius: 2)
  }
}

Fn OnKill() {
  mana.steal(gain: 15)
  fire.nova(dmg: 8, radius: 6)
}

Fn Tick() {
  // optional ambient buff cost
}
```

---

## Tips

### Estimating mana cost

Before flashing a spell, use the terminal's "Simulate" button or call `URuneScriptBPLib::GetManaEstimate(Source, FName("Primary"))` to compute the static cost. The value sums all library calls in the AST — loops are counted once (so a `for i in 0..N { fire.shot() }` estimates as 8 mana, not 8*N).

### Storage capacity

Default cap is 128 characters. Designers can raise it on the `URuneSpellAsset` or `URuneWeaponComponent` (max 4096). Spells that go over compile with an error: `"Spell is N characters — storage limit is M"`.

### Mana failure

When a library call requires more mana than you have, the call **does not execute** and the spell halts with a runtime error. Defensive spells should check `mana.current() > ...` before expensive calls:

```
if mana.current() > 20 {
  fire.burst(dmg: 20, radius: 4)
}
```

### Crash safety

The interpreter aborts after 10,000 instructions per invocation. Infinite loops won't crash your game — they crash the spell with a clear message. The 32-frame call stack means deep recursion fails fast too.

### Stubs vs. real effects

Per the [Library Reference](README.md#library-reference), some functions are wired to real effects, others are logged stubs that the game developer customises. A spell using `ice.freeze()` will compile and "cost" mana, but the target won't actually freeze unless the game's status system is hooked up to the `ice.freeze` callback. Check the table's `Real` column.
