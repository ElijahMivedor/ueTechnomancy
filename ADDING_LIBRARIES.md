# Adding Libraries

This guide walks you through extending TechnoMancy with your own RuneScript library functions. The plugin's 15 built-in libraries (`fire`, `ice`, `lightning`, ..., `mana`, `math`, etc.) are registered using the same API documented here — there's nothing privileged about them.

There are three kinds of extension you can do:

1. **Override a built-in** — same key, new behaviour (e.g. wire `fire.dot` to your status system)
2. **Add a method to a built-in namespace** — e.g. add `fire.napalm`
3. **Add a whole new namespace** — e.g. `runes.shadow` with all-new methods

---

## 1. Where to register

Library registrations are stored in a process-global singleton: `FRuneLibraryRegistry::Get()`. Built-ins register at runtime module startup, so any project-side registrations need to happen **after** the runtime module loads.

The two canonical places:

| Location | When it fires | Use for |
| --- | --- | --- |
| `UGameInstance::Init()` | Once, very early | Stable game-wide overrides and additions |
| `ARuneLibraryInitialiser::BeginPlay()` | Per level, after engine init | Level-scoped or BP-defined registration |

For most games you'll want `UGameInstance::Init()`. Subclass the game instance, override `Init`, and register your callbacks.

```cpp
// MyGameInstance.h
UCLASS()
class UMyGameInstance : public UGameInstance {
    GENERATED_BODY()
public:
    virtual void Init() override;
};
```

```cpp
// MyGameInstance.cpp
#include "MyGameInstance.h"
#include "RuneLibraryRegistry.h"
#include "RuneEffectHelpers.h"   // optional — for reusing SpawnProjectile etc.

void UMyGameInstance::Init() {
    Super::Init();
    auto& Reg = FRuneLibraryRegistry::Get();

    // your registrations here
}
```

Set your game instance class in `Project Settings → Maps & Modes → Game Instance Class`.

---

## 2. Override a built-in callback

The simplest extension. The last registration against a `(namespace, method)` key wins, so re-registering replaces the built-in.

Use case: the built-in `fire.dot` is a logged stub (no real DoT). Wire it to your status system.

```cpp
Reg.RegisterSimple(
    /*Namespace=*/      FName("fire"),
    /*FullPath=*/       FName("runes.fire"),
    /*Method=*/         FName("dot"),
    /*bRequiresImport=*/ true,
    /*ManaCost=*/       6.f,
    /*Description=*/    TEXT("Burn DoT on the last hit target"),
    /*ParamDocs=*/      TEXT("dmg_per_sec:float=5, duration:float=3"),
    /*Callback=*/       [](const FRuneLibCallArgs& Args, FRuneWeaponContext& Ctx) -> FRuneValue {
        const float Dps = (float)Args.GetFloat(FName("dmg_per_sec"), 5.0);
        const float Dur = (float)Args.GetFloat(FName("duration"),    3.0);
        AActor* Target  = Ctx.LastHitTarget.Get();
        if (!Target) return FRuneValue::MakeNil();

        // Hand off to your project's status manager:
        // UMyStatusManager::Get(Target)->ApplyBurn(Target, Dps, Dur);

        return FRuneValue::MakeNil();
    });
```

### Tips

- **`FRuneLibCallArgs`** exposes `GetFloat`, `GetBool`, `GetInt`, and `Get` (raw value). Pass defaults matching what your `ParamDocs` advertises.
- **`FRuneWeaponContext`** gives you the runtime state — `WeaponOwner`, `LastHitTarget`, `LastHitPos`, `AimDirection`, `Mana`, `MaxMana`, `DeltaTime`, and `ProjectileClass`.
- **Mana** is deducted **before** the callback runs. If you need conditional cost (e.g. `mana.burst`'s "spend all"), set `ManaCost` to 0 and modify `Ctx.Mana` inside the callback.
- **Return values**: return `FRuneValue::MakeNumber(X)` to give the spell author something to use. `FRuneValue::MakeNil()` for fire-and-forget effects.

---

## 3. Add a method to an existing namespace

Same pattern as override — just use a method name that isn't already taken.

```cpp
// Add fire.napalm — sticky AoE that ticks
Reg.RegisterSimple(
    FName("fire"), FName("runes.fire"), FName("napalm"),
    true, 18.f,
    TEXT("Sticky fire pool with damage ticks"),
    TEXT("radius:float=4, dmg_per_sec:float=8, duration:float=5"),
    [](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue {
        // your impl
        return FRuneValue::MakeNil();
    });
```

Players write `fire.napalm(radius: 5)` after `import runes.fire`. The new method shows up in `URuneScriptBPLib::GetLibraryFunctionList()` so terminal autocompletion picks it up automatically.

---

## 4. Add a whole new namespace

To create a brand-new namespace (e.g. `runes.shadow`):

```cpp
const FName NS(TEXT("shadow"));
const FName FP(TEXT("runes.shadow"));

Reg.RegisterSimple(NS, FP, TEXT("strike"), /*bRequiresImport=*/ true, 10.f,
    TEXT("Shadow strike from behind"),
    TEXT("dmg:float=20"),
    [](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue {
        TechnoMancy::Effects::DamageLastHit(C, (float)A.GetFloat(FName("dmg"), 20.0));
        return FRuneValue::MakeNil();
    });

Reg.RegisterSimple(NS, FP, TEXT("cloak"), true, 12.f,
    TEXT("Brief invisibility"),
    TEXT("duration:float=3"),
    [](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue {
        // your impl
        return FRuneValue::MakeNil();
    });
```

Players write:

```
import runes.shadow

Fn Primary() {
  shadow.cloak(duration: 5)
  shadow.strike(dmg: 30)
}
```

### Helper-style namespaces (no import required)

Pass `false` for `bRequiresImport`. Helper namespaces are always available — no `import` line needed. Use this for utility libraries (math-like, query-only) that players will want everywhere.

```cpp
const FName NS(TEXT("inventory"));
Reg.RegisterSimple(NS, NS, TEXT("count"), /*bRequiresImport=*/ false, 0.f,
    TEXT("Returns count of an item in inventory"),
    TEXT("item:float"),
    [](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue {
        // ... read from your inventory system ...
        return FRuneValue::MakeNumber(0);
    });
```

Players use `inventory.count(item: 5)` directly — no import.

---

## 5. Reusing the built-in effect helpers

If you want your custom callback to spawn a projectile or do AoE damage in the same way the built-ins do, use `TechnoMancy::Effects`:

```cpp
#include "RuneEffectHelpers.h"

// Spawn a projectile
TechnoMancy::Effects::SpawnProjectile(Ctx, FName("fire"), /*spd=*/12, /*dmg=*/8);

// Sphere damage
TechnoMancy::Effects::DamageSphere(Ctx, Ctx.LastHitPos, /*radius=*/5, /*dmg=*/15);

// Line trace damage
TechnoMancy::Effects::LineDamage(Ctx, /*range=*/12, /*dmg=*/8);

// Single-target damage
TechnoMancy::Effects::DamageLastHit(Ctx, /*dmg=*/20);

// Knockback
TechnoMancy::Effects::ApplyKnockback(Ctx, WorldPos, /*radius=*/5, /*force=*/600);

// Count actors in a sphere
int32 N = TechnoMancy::Effects::CountActorsInSphere(Ctx, WorldPos, /*radius=*/8);
```

Note: `RuneEffectHelpers.h` lives in the runtime module's `Private/` folder. To use it from another module, copy the file to `Public/` in your fork (the interfaces are stable), or copy the implementation into your own translation unit.

---

## 6. Mana semantics

The interpreter deducts `ManaCost` from `Ctx.Mana` **before** invoking your callback. The callback runs only if `Ctx.Mana >= ManaCost` (otherwise the interpreter throws and aborts the spell).

This means:

- **You don't need to check mana** in your callback — the interpreter already did.
- **You can modify `Ctx.Mana` further** inside the callback (e.g. `mana.steal` adds to it, `mana.burst` zeroes it out).
- **Free utility calls** (lookups, math) should have `ManaCost = 0`.

If you want a non-linear cost (e.g. "this costs 5 mana when the target is at full HP, 20 when low"), set `ManaCost = 0` and compute the cost inside the callback. Throw `FRuneManaError` to abort:

```cpp
[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue {
    const float Cost = ComputeDynamicCost(C);
    if (C.Mana < Cost) {
        throw FRuneManaError(FString::Printf(
            TEXT("Need %.0f mana, have %.0f"), Cost, C.Mana));
    }
    C.Mana = FMath::Max(0.f, C.Mana - Cost);
    // ... effect ...
    return FRuneValue::MakeNil();
}
```

---

## 7. Threading & ownership

- **Registration is single-threaded.** Only register from `Init()` / `BeginPlay()` / similar one-off points. Don't race two threads to register the same key.
- **Callbacks run on the game thread.** It's safe to spawn actors, apply damage, call any UE API.
- **Lambdas are stored by value** inside `TFunction`. Capture state by value (`[X = ...]`); avoid capturing `this` on transient objects.

---

## 8. Inspecting what's registered

`URuneScriptBPLib::GetLibraryFunctionList()` returns a `TArray<FRuneLibraryEntry>` with every registered function — namespace, method, mana cost, description, param docs. Use it to build:

- Autocomplete in the terminal widget
- An in-game spell reference / encyclopedia
- Documentation generators

Or from C++:

```cpp
TArray<FRuneLibraryEntry> All = FRuneLibraryRegistry::Get().GetAllEntries();
for (const FRuneLibraryEntry& E : All) {
    UE_LOG(LogTemp, Display, TEXT("%s.%s costs %.0f mana"),
        *E.Namespace, *E.Method, E.ManaCost);
}
```

---

## 9. Common patterns

### Tying to a status system

```cpp
auto MakeStatusCallback = [](EMyStatusType StatusType, float DefaultDuration) {
    return [StatusType, DefaultDuration](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue {
        const float Dur = (float)A.GetFloat(FName("duration"), DefaultDuration);
        if (AActor* T = C.LastHitTarget.Get()) {
            UMyStatusManager::Get(T)->Apply(T, StatusType, Dur);
        }
        return FRuneValue::MakeNil();
    };
};

Reg.RegisterSimple(FName("ice"), FName("runes.ice"), FName("freeze"),
    true, 12.f, TEXT("Freeze target"), TEXT("duration:float=2"),
    MakeStatusCallback(EMyStatusType::Frozen, 2.f));

Reg.RegisterSimple(FName("lightning"), FName("runes.lightning"), FName("stun"),
    true, 8.f, TEXT("Stun target"), TEXT("duration:float=1.5"),
    MakeStatusCallback(EMyStatusType::Stunned, 1.5f));
```

### Reading game state inside callbacks

```cpp
Reg.RegisterSimple(FName("inventory"), FName("inventory"), FName("ammo_for"),
    false, 0.f,
    TEXT("Returns current ammo count for the given weapon ID"),
    TEXT("weapon_id:float"),
    [](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue {
        AActor* Owner = C.WeaponOwner.Get();
        if (!Owner) return FRuneValue::MakeNumber(0);

        const int32 WeaponId = A.GetInt(FName("weapon_id"), 0);
        if (UMyInventory* Inv = Owner->FindComponentByClass<UMyInventory>()) {
            return FRuneValue::MakeNumber(Inv->GetAmmoFor(WeaponId));
        }
        return FRuneValue::MakeNumber(0);
    });
```

### Per-weapon configuration via `FRuneWeaponContext`

The plain `UClass*` field on `FRuneWeaponContext::ProjectileClass` is how the built-ins customise per-weapon projectiles. If you want similar per-weapon config for your callbacks, the cleanest approach is to extend `URuneWeaponComponent` with project-specific properties, fork the context struct, and populate it in `RefreshContext()`.

Or, look up your config from `WeaponOwner` directly inside the callback — simpler if your config lives on the weapon actor anyway.

---

## 10. Removing a registration

There's no public unregister API in v1. The registry is a process-global singleton — once registered, callbacks live until the module shuts down. To "remove" a callback in practice, re-register with the same key and a no-op body.

This matches the design intent: callbacks are configured once at startup, not toggled at runtime.

---

## 11. Testing your library

```cpp
#include "RuneInterpreter.h"
#include "RuneTypes.h"

bool TestMyLibrary() {
    FRuneInterpreter Interp;
    TArray<FString> Errors;
    bool bOk = Interp.Compile(TEXT(
        "import runes.shadow\n"
        "Fn Primary() {\n"
        "  shadow.strike(dmg: 25)\n"
        "}\n"), Errors);
    if (!bOk) return false;

    FRuneWeaponContext Ctx;
    Ctx.Mana = 100;
    Ctx.MaxMana = 100;
    return Interp.Invoke(FName("Primary"), Ctx, Errors);
}
```

Or just use the console command `runescript.test` after registering — it runs the built-in self-test which exercises the lex/parse/execute path.
