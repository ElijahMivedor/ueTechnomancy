// Copyright TechnoMancy. All rights reserved.
//
// Phase 3: the 8 elemental libraries.
//   - Projectile-spawning calls (*.shot, *.lance, *.bolt, *.boulder) spawn
//     ARuneProjectile (or the URuneWeaponComponent::ProjectileClass override).
//   - AoE damage calls (*.burst, *.nova, *.shockwave, *.detonate, *.pulse,
//     *.quake) run a sphere overlap and ApplyDamage everything inside.
//   - Beams (*.beam, *.blade) are line traces.
//   - Status / DoT / buff / wall / construct effects stay as logged stubs:
//     they require game-side state the plugin doesn't model. Game developers
//     override these callbacks by re-registering against the same key.
//
// Mana costs and parameter signatures match the PRD library tables verbatim.

#include "RuneBuiltinLibraries.h"
#include "RuneEffectHelpers.h"
#include "RuneLibraryRegistry.h"
#include "RuneTypes.h"
#include "TechnoMancyLog.h"
#include "GameFramework/Actor.h"

namespace
{
	// -------------------------------------------------------------------------
	// Logging helpers
	// -------------------------------------------------------------------------

	FString FormatArgs(const FRuneLibCallArgs& Args)
	{
		TArray<FString> Parts;
		Parts.Reserve(Args.Named.Num());
		for (const TPair<FName, FRuneValue>& Pair : Args.Named)
		{
			FString V;
			switch (Pair.Value.Type)
			{
				case ERuneValueType::Number: V = FString::Printf(TEXT("%.2f"), Pair.Value.Number); break;
				case ERuneValueType::Bool:   V = Pair.Value.Bool ? TEXT("true") : TEXT("false");   break;
				default:                     V = TEXT("nil");                                       break;
			}
			Parts.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key.ToString(), *V));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	FRuneLibCallback MakeStub(const TCHAR* QualifiedName)
	{
		const FString Q(QualifiedName);
		return [Q](const FRuneLibCallArgs& Args, FRuneWeaponContext&) -> FRuneValue
		{
			UE_LOG(LogTechnoMancy, Log, TEXT("[Spell] %s(%s) [stub]"), *Q, *FormatArgs(Args));
			return FRuneValue::MakeNil();
		};
	}

	// -------------------------------------------------------------------------
	// Effect-call helpers — small lambdas reused across libraries
	// -------------------------------------------------------------------------

	FRuneLibCallback MakeProjectile(FName Element,
									float DefaultSpeed,
									float DefaultDamage,
									int32 DefaultPierce = 0,
									float DefaultSize = 1.f)
	{
		const FName ElementCapture = Element;
		const float DefSpd = DefaultSpeed;
		const float DefDmg = DefaultDamage;
		const int32 DefPierce = DefaultPierce;
		const float DefSize = DefaultSize;
		return [ElementCapture, DefSpd, DefDmg, DefPierce, DefSize]
			(const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
		{
			const float Spd  = (float)A.GetFloat(TEXT("spd"),  DefSpd);
			const float Dmg  = (float)A.GetFloat(TEXT("dmg"),  DefDmg);
			const float Sz   = (float)A.GetFloat(TEXT("size"), DefSize);
			int32 Pierce = DefPierce;
			if (A.Named.Contains(TEXT("pierce_count")))
			{
				Pierce = A.GetInt(TEXT("pierce_count"), DefPierce);
			}
			else if (A.Named.Contains(TEXT("pierce")))
			{
				Pierce = A.GetBool(TEXT("pierce"), false) ? 1 : DefPierce;
			}
			TechnoMancy::Effects::SpawnProjectile(C, ElementCapture, Spd, Dmg, Pierce, Sz);
			return FRuneValue::MakeNil();
		};
	}

	FRuneLibCallback MakeBurstAtLastHit(float DefaultDmg, float DefaultRadius)
	{
		const float DefDmg = DefaultDmg;
		const float DefRadius = DefaultRadius;
		return [DefDmg, DefRadius](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
		{
			const float Dmg = (float)A.GetFloat(TEXT("dmg"),    DefDmg);
			const float R   = (float)A.GetFloat(TEXT("radius"), DefRadius);
			TechnoMancy::Effects::DamageSphere(C, C.LastHitPos, R, Dmg);
			return FRuneValue::MakeNil();
		};
	}

	FRuneLibCallback MakeNovaAroundOwner(float DefaultDmg, float DefaultRadius)
	{
		const float DefDmg = DefaultDmg;
		const float DefRadius = DefaultRadius;
		return [DefDmg, DefRadius](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
		{
			const float Dmg = (float)A.GetFloat(TEXT("dmg"),    DefDmg);
			const float R   = (float)A.GetFloat(TEXT("radius"), DefRadius);
			if (C.WeaponOwner.IsValid())
			{
				TechnoMancy::Effects::DamageSphere(C, C.WeaponOwner->GetActorLocation(), R, Dmg);
			}
			return FRuneValue::MakeNil();
		};
	}

	FRuneLibCallback MakeBeamForward(float DefaultRange, float DefaultDmg)
	{
		const float DefRange = DefaultRange;
		const float DefDmg = DefaultDmg;
		return [DefRange, DefDmg](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
		{
			const float Range = (float)A.GetFloat(TEXT("range"), DefRange);
			const float Dmg   = (float)A.GetFloat(TEXT("dmg"),   DefDmg);
			TechnoMancy::Effects::LineDamage(C, Range, Dmg);
			return FRuneValue::MakeNil();
		};
	}

	// =========================================================================
	// runes.fire
	// =========================================================================
	void Register_Fire(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("fire"));
		const FName FP(TEXT("runes.fire"));
		Reg.RegisterSimple(NS, FP, TEXT("shot"),   true,  8.f, TEXT("Spawns a fire projectile"),
			TEXT("spd:float=10, dmg:float=8, pierce:bool=false, size:float=1"),
			MakeProjectile(TEXT("fire"), 10.f, 8.f));
		Reg.RegisterSimple(NS, FP, TEXT("burst"),  true, 15.f, TEXT("AoE explosion at last hit position"),
			TEXT("dmg:float=15, radius:float=3, falloff:float=0.5"),
			MakeBurstAtLastHit(15.f, 3.f));
		Reg.RegisterSimple(NS, FP, TEXT("dot"),    true,  6.f, TEXT("Apply burn DoT to last hit target"),
			TEXT("dmg_per_sec:float=5, duration:float=3"),                         MakeStub(TEXT("fire.dot")));
		Reg.RegisterSimple(NS, FP, TEXT("wall"),   true, 20.f, TEXT("Spawn fire wall in front of owner"),
			TEXT("width:float=4, height:float=3, duration:float=5"),               MakeStub(TEXT("fire.wall")));
		Reg.RegisterSimple(NS, FP, TEXT("nova"),   true, 18.f, TEXT("360° fire explosion from owner"),
			TEXT("dmg:float=12, radius:float=5"),
			MakeNovaAroundOwner(12.f, 5.f));
		Reg.RegisterSimple(NS, FP, TEXT("trail"),  true, 10.f, TEXT("Leave damage fire trail on ground"),
			TEXT("dmg:float=3, duration:float=4"),                                 MakeStub(TEXT("fire.trail")));
		Reg.RegisterSimple(NS, FP, TEXT("homing"), true, 14.f, TEXT("Self-guided homing fireball"),
			TEXT("spd:float=8, dmg:float=12, turn_rate:float=3"),
			MakeProjectile(TEXT("fire"), 8.f, 12.f));
		Reg.RegisterSimple(NS, FP, TEXT("pillar"), true, 16.f, TEXT("Summon fire pillar at target location"),
			TEXT("dmg:float=20, duration:float=3"),
			MakeBurstAtLastHit(20.f, 2.f));
	}

	// =========================================================================
	// runes.ice
	// =========================================================================
	void Register_Ice(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("ice"));
		const FName FP(TEXT("runes.ice"));
		Reg.RegisterSimple(NS, FP, TEXT("shot"),    true,  7.f, TEXT("Slowing ice projectile"),
			TEXT("spd:float=9, dmg:float=6, slow_pct:float=0.3, slow_dur:float=2"),
			MakeProjectile(TEXT("ice"), 9.f, 6.f));
		Reg.RegisterSimple(NS, FP, TEXT("freeze"),  true, 12.f, TEXT("Freeze last hit target solid"),
			TEXT("duration:float=2"),                                              MakeStub(TEXT("ice.freeze")));
		Reg.RegisterSimple(NS, FP, TEXT("shatter"), true, 10.f, TEXT("Shatter frozen target (3x dmg if frozen)"),
			TEXT("dmg:float=20"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
			{
				const float Dmg = (float)A.GetFloat(TEXT("dmg"), 20.0);
				TechnoMancy::Effects::DamageLastHit(C, Dmg);
				return FRuneValue::MakeNil();
			});
		Reg.RegisterSimple(NS, FP, TEXT("nova"),    true, 16.f, TEXT("Radial ice slow burst"),
			TEXT("radius:float=4, slow_pct:float=0.4, slow_dur:float=2"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
			{
				const float R = (float)A.GetFloat(TEXT("radius"), 4.0);
				if (C.WeaponOwner.IsValid())
				{
					TechnoMancy::Effects::DamageSphere(C, C.WeaponOwner->GetActorLocation(), R, 0.f);
				}
				return FRuneValue::MakeNil();
			});
		Reg.RegisterSimple(NS, FP, TEXT("wall"),    true, 18.f, TEXT("Solid ice barrier"),
			TEXT("width:float=4, height:float=3, duration:float=6"),               MakeStub(TEXT("ice.wall")));
		Reg.RegisterSimple(NS, FP, TEXT("spikes"),  true, 14.f, TEXT("Spawn ice spikes from ground at target"),
			TEXT("count:int=3, dmg:float=10"),
			MakeBurstAtLastHit(10.f, 2.f));
		Reg.RegisterSimple(NS, FP, TEXT("armor"),   true, 10.f, TEXT("Ice armor buff on self"),
			TEXT("absorption:float=20, duration:float=6"),                         MakeStub(TEXT("ice.armor")));
		Reg.RegisterSimple(NS, FP, TEXT("lance"),   true, 12.f, TEXT("Piercing ice lance"),
			TEXT("spd:float=12, dmg:float=14, pierce_count:float=2"),
			MakeProjectile(TEXT("ice"), 12.f, 14.f, 2));
	}

	// =========================================================================
	// runes.lightning
	// =========================================================================
	void Register_Lightning(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("lightning"));
		const FName FP(TEXT("runes.lightning"));
		Reg.RegisterSimple(NS, FP, TEXT("bolt"),     true,  9.f, TEXT("Fast lightning bolt"),
			TEXT("spd:float=20, dmg:float=10"),
			MakeProjectile(TEXT("lightning"), 20.f, 10.f));
		Reg.RegisterSimple(NS, FP, TEXT("chain"),    true, 14.f, TEXT("Chain lightning between nearby enemies"),
			TEXT("jumps:float=3, dmg:float=12, falloff:float=0.7"),                MakeStub(TEXT("lightning.chain")));
		Reg.RegisterSimple(NS, FP, TEXT("stun"),     true,  8.f, TEXT("Stun last hit target"),
			TEXT("duration:float=1.5"),                                            MakeStub(TEXT("lightning.stun")));
		Reg.RegisterSimple(NS, FP, TEXT("storm"),    true, 25.f, TEXT("Lightning storm AoE"),
			TEXT("radius:float=8, strikes:float=5, dmg:float=8"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
			{
				const float R = (float)A.GetFloat(TEXT("radius"),  8.0);
				const float D = (float)A.GetFloat(TEXT("dmg"),     8.0);
				const int32 N = FMath::Max(1, A.GetInt(TEXT("strikes"), 5));
				const FVector Center = C.WeaponOwner.IsValid()
					? C.LastHitPos
					: C.LastHitPos;
				for (int32 i = 0; i < N; ++i)
				{
					const FVector Offset = FMath::VRand() * R * TechnoMancy::Effects::UnitScale * 0.5f;
					TechnoMancy::Effects::DamageSphere(C, Center + Offset, R * 0.3f, D);
				}
				return FRuneValue::MakeNil();
			});
		Reg.RegisterSimple(NS, FP, TEXT("arc"),      true, 12.f, TEXT("Cone lightning arc"),
			TEXT("angle:float=45, range:float=6, dmg:float=10"),
			MakeBeamForward(6.f, 10.f));
		Reg.RegisterSimple(NS, FP, TEXT("overload"), true, 20.f, TEXT("Buff next hit's damage"),
			TEXT("dmg_mult:float=2, duration:float=3"),                            MakeStub(TEXT("lightning.overload")));
		Reg.RegisterSimple(NS, FP, TEXT("static"),   true, 10.f, TEXT("Apply static charges to target"),
			TEXT("charges:float=3, dmg:float=6"),                                  MakeStub(TEXT("lightning.static")));
		Reg.RegisterSimple(NS, FP, TEXT("pulse"),    true, 16.f, TEXT("EMP radial pulse"),
			TEXT("radius:float=4, dmg:float=8, stun_dur:float=0.5"),
			MakeNovaAroundOwner(8.f, 4.f));
	}

	// =========================================================================
	// runes.void
	// =========================================================================
	void Register_Void(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("void"));
		const FName FP(TEXT("runes.void"));
		Reg.RegisterSimple(NS, FP, TEXT("shot"),     true,  8.f, TEXT("Void projectile"),
			TEXT("spd:float=9, dmg:float=7, pierce:bool=false"),
			MakeProjectile(TEXT("void"), 9.f, 7.f));
		Reg.RegisterSimple(NS, FP, TEXT("phase"),    true, 12.f, TEXT("Brief owner invulnerability"),
			TEXT("duration:float=0.5"),                                            MakeStub(TEXT("void.phase")));
		Reg.RegisterSimple(NS, FP, TEXT("mark"),     true,  5.f, TEXT("Mark target; amplifies damage received"),
			TEXT("duration:float=5, amp:float=1.5"),                               MakeStub(TEXT("void.mark")));
		Reg.RegisterSimple(NS, FP, TEXT("slow"),     true,  6.f, TEXT("Slow last hit target"),
			TEXT("amt:float=0.4, duration:float=2"),                               MakeStub(TEXT("void.slow")));
		Reg.RegisterSimple(NS, FP, TEXT("echo"),     true,  8.f, TEXT("Delayed repeat of last shot"),
			TEXT("delay:float=0.2, dmg_mult:float=0.8"),                           MakeStub(TEXT("void.echo")));
		Reg.RegisterSimple(NS, FP, TEXT("siphon"),   true,  0.f, TEXT("Deal dmg, return mana gained"),
			TEXT("dmg:float=10, mana_gain:float=8"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
			{
				const float Dmg  = (float)A.GetFloat(TEXT("dmg"),       10.0);
				const float Gain = (float)A.GetFloat(TEXT("mana_gain"),  8.0);
				TechnoMancy::Effects::DamageLastHit(C, Dmg);
				C.Mana = FMath::Min(C.MaxMana, C.Mana + Gain);
				return FRuneValue::MakeNumber(Gain);
			});
		Reg.RegisterSimple(NS, FP, TEXT("rift"),     true, 22.f, TEXT("Gravity rift pulling enemies"),
			TEXT("radius:float=5, duration:float=4"),                              MakeStub(TEXT("void.rift")));
		Reg.RegisterSimple(NS, FP, TEXT("detonate"), true, 18.f, TEXT("Void explosion at last hit pos"),
			TEXT("radius:float=4, dmg:float=18"),
			MakeBurstAtLastHit(18.f, 4.f));
	}

	// =========================================================================
	// runes.light
	// =========================================================================
	void Register_Light(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("light"));
		const FName FP(TEXT("runes.light"));
		Reg.RegisterSimple(NS, FP, TEXT("beam"),      true, 10.f, TEXT("Continuous damage beam (one frame)"),
			TEXT("range:float=12, dmg:float=8, width:float=0.5"),
			MakeBeamForward(12.f, 8.f));
		Reg.RegisterSimple(NS, FP, TEXT("construct"), true, 16.f, TEXT("0=shield wall, 1=platform, 2=cage"),
			TEXT("type:float=0, duration:float=6"),                                MakeStub(TEXT("light.construct")));
		Reg.RegisterSimple(NS, FP, TEXT("burst"),     true, 12.f, TEXT("Flash burst — damage and blind"),
			TEXT("dmg:float=10, radius:float=4, blind_dur:float=1"),
			MakeBurstAtLastHit(10.f, 4.f));
		Reg.RegisterSimple(NS, FP, TEXT("shield"),    true,  8.f, TEXT("Personal hardlight shield"),
			TEXT("absorption:float=15, duration:float=4"),                         MakeStub(TEXT("light.shield")));
		Reg.RegisterSimple(NS, FP, TEXT("lance"),     true, 11.f, TEXT("Fast piercing hardlight lance"),
			TEXT("spd:float=15, dmg:float=12"),
			MakeProjectile(TEXT("light"), 15.f, 12.f, 1));
		Reg.RegisterSimple(NS, FP, TEXT("prism"),     true, 18.f, TEXT("Light shot that bounces between enemies"),
			TEXT("bounces:float=3, dmg:float=8"),
			MakeProjectile(TEXT("light"), 12.f, 8.f));
		Reg.RegisterSimple(NS, FP, TEXT("beacon"),    true, 14.f, TEXT("Pulsing damage beacon at target pos"),
			TEXT("duration:float=5, pulse_dmg:float=5"),                           MakeStub(TEXT("light.beacon")));
		Reg.RegisterSimple(NS, FP, TEXT("blade"),     true, 12.f, TEXT("Melee hardlight slash"),
			TEXT("dmg:float=15, range:float=2, arc:float=90"),
			MakeBeamForward(2.f, 15.f));
	}

	// =========================================================================
	// runes.earth
	// =========================================================================
	void Register_Earth(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("earth"));
		const FName FP(TEXT("runes.earth"));
		Reg.RegisterSimple(NS, FP, TEXT("spike"),     true, 10.f, TEXT("Stone spike from ground at target"),
			TEXT("dmg:float=12, height:float=3"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
			{
				const float Dmg = (float)A.GetFloat(TEXT("dmg"), 12.0);
				TechnoMancy::Effects::DamageSphere(C, C.LastHitPos, 1.5f, Dmg);
				return FRuneValue::MakeNil();
			});
		Reg.RegisterSimple(NS, FP, TEXT("wall"),      true, 18.f, TEXT("Stone barrier wall"),
			TEXT("width:float=5, height:float=4, duration:float=8"),               MakeStub(TEXT("earth.wall")));
		Reg.RegisterSimple(NS, FP, TEXT("shockwave"), true, 15.f, TEXT("Ground shockwave radial"),
			TEXT("range:float=6, dmg:float=8, knockback:float=500"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
			{
				const float R    = (float)A.GetFloat(TEXT("range"),     6.0);
				const float Dmg  = (float)A.GetFloat(TEXT("dmg"),       8.0);
				const float Push = (float)A.GetFloat(TEXT("knockback"), 500.0);
				if (C.WeaponOwner.IsValid())
				{
					const FVector Origin = C.WeaponOwner->GetActorLocation();
					TechnoMancy::Effects::DamageSphere(C, Origin, R, Dmg);
					TechnoMancy::Effects::ApplyKnockback(C, Origin, R, Push);
				}
				return FRuneValue::MakeNil();
			});
		Reg.RegisterSimple(NS, FP, TEXT("boulder"),   true, 12.f, TEXT("Rolling boulder projectile"),
			TEXT("spd:float=7, dmg:float=20, size:float=1"),
			MakeProjectile(TEXT("earth"), 7.f, 20.f, 0, 1.5f));
		Reg.RegisterSimple(NS, FP, TEXT("quake"),     true, 22.f, TEXT("Sustained earthquake AoE"),
			TEXT("radius:float=8, dmg:float=6, duration:float=3"),
			MakeNovaAroundOwner(6.f, 8.f));
		Reg.RegisterSimple(NS, FP, TEXT("armor"),     true, 10.f, TEXT("Stone armor buff on self"),
			TEXT("absorption:float=25, duration:float=8"),                         MakeStub(TEXT("earth.armor")));
		Reg.RegisterSimple(NS, FP, TEXT("pull"),      true,  8.f, TEXT("Gravitational pull on last hit target"),
			TEXT("force:float=600"),                                               MakeStub(TEXT("earth.pull")));
		Reg.RegisterSimple(NS, FP, TEXT("raise"),     true, 20.f, TEXT("Raise terrain obstacles around owner"),
			TEXT("count:float=3, duration:float=6"),                               MakeStub(TEXT("earth.raise")));
	}

	// =========================================================================
	// runes.wind
	// =========================================================================
	void Register_Wind(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("wind"));
		const FName FP(TEXT("runes.wind"));
		Reg.RegisterSimple(NS, FP, TEXT("blast"),   true,  8.f, TEXT("Knockback blast in aim direction"),
			TEXT("range:float=5, force:float=600, dmg:float=5"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
			{
				const float R     = (float)A.GetFloat(TEXT("range"), 5.0);
				const float Force = (float)A.GetFloat(TEXT("force"), 600.0);
				const float Dmg   = (float)A.GetFloat(TEXT("dmg"),   5.0);
				if (C.WeaponOwner.IsValid())
				{
					const FVector Origin = C.WeaponOwner->GetActorLocation() + C.AimDirection.GetSafeNormal() * (R * 0.5f * TechnoMancy::Effects::UnitScale);
					TechnoMancy::Effects::DamageSphere (C, Origin, R, Dmg);
					TechnoMancy::Effects::ApplyKnockback(C, Origin, R, Force);
				}
				return FRuneValue::MakeNil();
			});
		Reg.RegisterSimple(NS, FP, TEXT("dash"),    true, 10.f, TEXT("Owner dash (0=fwd, 1=back, 2=left, 3=right)"),
			TEXT("dist:float=500, dir_x:float=0, dir_z:float=0"),                  MakeStub(TEXT("wind.dash")));
		Reg.RegisterSimple(NS, FP, TEXT("vortex"),  true, 16.f, TEXT("Wind vortex trapping enemies"),
			TEXT("radius:float=4, duration:float=4"),                              MakeStub(TEXT("wind.vortex")));
		Reg.RegisterSimple(NS, FP, TEXT("gust"),    true,  6.f, TEXT("Push single last hit target"),
			TEXT("force:float=400"),                                               MakeStub(TEXT("wind.gust")));
		Reg.RegisterSimple(NS, FP, TEXT("shield"),  true, 12.f, TEXT("Projectile deflection shield"),
			TEXT("deflect_pct:float=0.3, duration:float=4"),                       MakeStub(TEXT("wind.shield")));
		Reg.RegisterSimple(NS, FP, TEXT("haste"),   true,  8.f, TEXT("Owner movement speed boost"),
			TEXT("speed_mult:float=1.5, duration:float=4"),                        MakeStub(TEXT("wind.haste")));
		Reg.RegisterSimple(NS, FP, TEXT("tornado"), true, 24.f, TEXT("Sustained tornado at target pos"),
			TEXT("radius:float=5, dmg:float=4, duration:float=6"),
			MakeBurstAtLastHit(4.f, 5.f));
		Reg.RegisterSimple(NS, FP, TEXT("shot"),    true,  7.f, TEXT("Knockback wind projectile"),
			TEXT("spd:float=14, dmg:float=6, knockback:float=300"),
			MakeProjectile(TEXT("wind"), 14.f, 6.f));
	}

	// =========================================================================
	// runes.bio
	// =========================================================================
	void Register_Bio(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("bio"));
		const FName FP(TEXT("runes.bio"));
		Reg.RegisterSimple(NS, FP, TEXT("poison"),     true,  8.f, TEXT("Poison DoT on last hit target"),
			TEXT("dmg_per_sec:float=4, duration:float=5"),                         MakeStub(TEXT("bio.poison")));
		Reg.RegisterSimple(NS, FP, TEXT("drain"),      true, 14.f, TEXT("Life drain; returns health gained"),
			TEXT("dmg:float=10, health_gain:float=5"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
			{
				const float Dmg = (float)A.GetFloat(TEXT("dmg"),         10.0);
				const float Gain = (float)A.GetFloat(TEXT("health_gain"), 5.0);
				TechnoMancy::Effects::DamageLastHit(C, Dmg);
				return FRuneValue::MakeNumber(Gain);
			});
		Reg.RegisterSimple(NS, FP, TEXT("spores"),     true, 12.f, TEXT("Spore cloud AoE"),
			TEXT("radius:float=4, duration:float=5, slow_pct:float=0.3"),
			MakeBurstAtLastHit(0.f, 4.f));
		Reg.RegisterSimple(NS, FP, TEXT("regenerate"), true, 10.f, TEXT("Self-heal over time"),
			TEXT("hp_per_sec:float=5, duration:float=5"),                          MakeStub(TEXT("bio.regenerate")));
		Reg.RegisterSimple(NS, FP, TEXT("mutate"),     true, 16.f, TEXT("Weaken target, amplifying damage it takes"),
			TEXT("dmg_amp:float=1.25, duration:float=4"),                          MakeStub(TEXT("bio.mutate")));
		Reg.RegisterSimple(NS, FP, TEXT("plague"),     true, 20.f, TEXT("Spreading plague to nearby enemies"),
			TEXT("spread_radius:float=6, duration:float=6"),                       MakeStub(TEXT("bio.plague")));
		Reg.RegisterSimple(NS, FP, TEXT("cocoon"),     true, 18.f, TEXT("Stun self but regenerate rapidly"),
			TEXT("duration:float=3, hp_per_sec:float=10"),                         MakeStub(TEXT("bio.cocoon")));
		Reg.RegisterSimple(NS, FP, TEXT("sap"),        true,  8.f, TEXT("Weaken and slow last hit target"),
			TEXT("slow_pct:float=0.3, duration:float=3"),                          MakeStub(TEXT("bio.sap")));
	}
}

void TechnoMancy::BuiltinLibraries::RegisterElemental()
{
	FRuneLibraryRegistry& Reg = FRuneLibraryRegistry::Get();
	Register_Fire(Reg);
	Register_Ice(Reg);
	Register_Lightning(Reg);
	Register_Void(Reg);
	Register_Light(Reg);
	Register_Earth(Reg);
	Register_Wind(Reg);
	Register_Bio(Reg);
}
