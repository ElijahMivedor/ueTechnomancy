// Copyright TechnoMancy. All rights reserved.
//
// Shared world-touching helpers used by library callbacks. Centralising these
// keeps RuneLibraries_*.cpp free of UE_LOG/ApplyDamage boilerplate.
//
// Numeric convention: RuneScript spells use small "designer" numbers (e.g.
// radius:3, spd:10). These helpers multiply by `UnitScale` (default 100) when
// translating to Unreal cm — radius 3 → 300 cm, spd 10 → 1000 cm/s. Game
// projects override callbacks if a different scale is wanted.

#pragma once

#include "CoreMinimal.h"
#include "RuneTypes.h"

namespace TechnoMancy::Effects
{
	/** cm-per-RuneScript-unit. Most public callbacks treat radius/range/speed as "designer units". */
	constexpr float UnitScale = 100.f;

	/**
	 * Spawn the configured projectile class out of the weapon owner along its aim direction.
	 * Returns the spawned actor (nullptr if no valid world or owner).
	 *
	 * Speed is multiplied by UnitScale before being written to the projectile's movement component.
	 * Damage and PierceCount are passed through; SizeScale sets the spawned actor's uniform scale.
	 */
	AActor* SpawnProjectile(FRuneWeaponContext& Ctx,
							FName Element,
							float Speed,
							float Damage,
							int32 PierceCount = 0,
							float SizeScale = 1.f);

	/** Apply damage to every overlapping actor in a sphere at WorldPos. Returns hit count. */
	int32 DamageSphere(FRuneWeaponContext& Ctx, FVector WorldPos, float RadiusUnits, float Damage);

	/** Single-actor damage to whoever is in Ctx.LastHitTarget (no-op if not set). */
	void DamageLastHit(FRuneWeaponContext& Ctx, float Damage);

	/** Knock back every actor in a sphere at WorldPos (uses physics impulse where possible). */
	void ApplyKnockback(FRuneWeaponContext& Ctx, FVector WorldPos, float RadiusUnits, float Force);

	/** Single line trace forward from the owner, damage first hit. Returns the hit actor or nullptr. */
	AActor* LineDamage(FRuneWeaponContext& Ctx, float RangeUnits, float Damage);

	/** Count actors inside a sphere at WorldPos (typically used by target.count). */
	int32 CountActorsInSphere(FRuneWeaponContext& Ctx, FVector WorldPos, float RadiusUnits);
}
