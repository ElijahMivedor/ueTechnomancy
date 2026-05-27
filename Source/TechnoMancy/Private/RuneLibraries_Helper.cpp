// Copyright TechnoMancy. All rights reserved.
//
// Phase 2: helper library implementations. `math` and `mana` are functional
// (real values), since they have no game-side dependencies. The remaining
// libraries (`target`, `self`, `time`, `status`, `chain`) read whatever is
// already on the FRuneWeaponContext and log otherwise — game effects come
// online in Phase 3.

#include "RuneBuiltinLibraries.h"
#include "RuneEffectHelpers.h"
#include "RuneLibraryRegistry.h"
#include "RuneTypes.h"
#include "TechnoMancyLog.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

namespace
{
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

	void LogCall(const TCHAR* Qualified, const FRuneLibCallArgs& Args)
	{
		UE_LOG(LogTechnoMancy, Log, TEXT("[Spell] %s(%s)"), Qualified, *FormatArgs(Args));
	}

	FRuneLibCallback MakeStub(const TCHAR* QualifiedName)
	{
		const FString Q(QualifiedName);
		return [Q](const FRuneLibCallArgs& Args, FRuneWeaponContext&) -> FRuneValue
		{
			LogCall(*Q, Args);
			return FRuneValue::MakeNil();
		};
	}

	FRuneLibCallback MakeReturning0(const TCHAR* QualifiedName)
	{
		const FString Q(QualifiedName);
		return [Q](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
		{
			LogCall(*Q, A);
			return FRuneValue::MakeNumber(0.0);
		};
	}

	// =========================================================================
	// mana — pool manipulation. Helpers do NOT require import.
	// =========================================================================
	void Register_Mana(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("mana"));
		const FName FP(TEXT("mana"));

		Reg.RegisterSimple(NS, FP, TEXT("steal"), false, 0.f,
			TEXT("Gain mana; returns new total"),
			TEXT("gain:float=5"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
			{
				const double Gain = A.GetFloat(TEXT("gain"), 5.0);
				C.Mana = FMath::Min<float>(C.MaxMana, C.Mana + (float)Gain);
				return FRuneValue::MakeNumber(C.Mana);
			});

		Reg.RegisterSimple(NS, FP, TEXT("burst"), false, 0.f,
			TEXT("Spend all mana for power multiplier; returns multiplier"),
			TEXT("dmg_mult:float=1.5"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
			{
				const double Mult = A.GetFloat(TEXT("dmg_mult"), 1.5);
				UE_LOG(LogTechnoMancy, Log, TEXT("[Spell] mana.burst -> burned %.2f mana, x%.2f"), C.Mana, Mult);
				C.Mana = 0.f;
				return FRuneValue::MakeNumber(Mult);
			});

		Reg.RegisterSimple(NS, FP, TEXT("save"), false, 0.f,
			TEXT("Reduce next spell cost by pct (0..1)"),
			TEXT("pct:float=0.2"),
			MakeStub(TEXT("mana.save")));

		Reg.RegisterSimple(NS, FP, TEXT("current"), false, 0.f,
			TEXT("Returns current mana value"), TEXT(""),
			[](const FRuneLibCallArgs&, FRuneWeaponContext& C) -> FRuneValue
			{
				return FRuneValue::MakeNumber(C.Mana);
			});

		Reg.RegisterSimple(NS, FP, TEXT("max"), false, 0.f,
			TEXT("Returns max mana"), TEXT(""),
			[](const FRuneLibCallArgs&, FRuneWeaponContext& C) -> FRuneValue
			{
				return FRuneValue::MakeNumber(C.MaxMana);
			});

		Reg.RegisterSimple(NS, FP, TEXT("pct"), false, 0.f,
			TEXT("Returns current/max (0..1)"), TEXT(""),
			[](const FRuneLibCallArgs&, FRuneWeaponContext& C) -> FRuneValue
			{
				const double Pct = (C.MaxMana > 0.f) ? (C.Mana / C.MaxMana) : 0.0;
				return FRuneValue::MakeNumber(Pct);
			});
	}

	// =========================================================================
	// math — pure utilities (all functional, all 0 mana)
	// =========================================================================
	void Register_Math(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("math"));
		const FName FP(TEXT("math"));

		Reg.RegisterSimple(NS, FP, TEXT("rnd"), false, 0.f,
			TEXT("Random float in range"),
			TEXT("lo:float=0, hi:float=1"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				const double Lo = A.GetFloat(TEXT("lo"), 0.0);
				const double Hi = A.GetFloat(TEXT("hi"), 1.0);
				return FRuneValue::MakeNumber(FMath::FRandRange(Lo, Hi));
			});

		Reg.RegisterSimple(NS, FP, TEXT("rndi"), false, 0.f,
			TEXT("Random integer in range (truncated)"),
			TEXT("lo:float=0, hi:float=10"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				const int32 Lo = A.GetInt(TEXT("lo"), 0);
				const int32 Hi = A.GetInt(TEXT("hi"), 10);
				return FRuneValue::MakeNumber(FMath::RandRange(Lo, Hi));
			});

		Reg.RegisterSimple(NS, FP, TEXT("clamp"), false, 0.f,
			TEXT("Clamp val between lo and hi"),
			TEXT("val:float, lo:float, hi:float"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				const double V  = A.GetFloat(TEXT("val"), 0.0);
				const double Lo = A.GetFloat(TEXT("lo"),  0.0);
				const double Hi = A.GetFloat(TEXT("hi"),  1.0);
				return FRuneValue::MakeNumber(FMath::Clamp(V, Lo, Hi));
			});

		Reg.RegisterSimple(NS, FP, TEXT("lerp"), false, 0.f,
			TEXT("Linear interpolation"),
			TEXT("a:float, b:float, t:float"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				const double Av = A.GetFloat(TEXT("a"), 0.0);
				const double Bv = A.GetFloat(TEXT("b"), 1.0);
				const double T  = A.GetFloat(TEXT("t"), 0.5);
				return FRuneValue::MakeNumber(FMath::Lerp(Av, Bv, T));
			});

		Reg.RegisterSimple(NS, FP, TEXT("abs"), false, 0.f,
			TEXT("Absolute value"), TEXT("val:float"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				return FRuneValue::MakeNumber(FMath::Abs(A.GetFloat(TEXT("val"), 0.0)));
			});

		Reg.RegisterSimple(NS, FP, TEXT("sin"), false, 0.f,
			TEXT("Sine (radians)"), TEXT("val:float"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				return FRuneValue::MakeNumber(FMath::Sin(A.GetFloat(TEXT("val"), 0.0)));
			});

		Reg.RegisterSimple(NS, FP, TEXT("cos"), false, 0.f,
			TEXT("Cosine (radians)"), TEXT("val:float"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				return FRuneValue::MakeNumber(FMath::Cos(A.GetFloat(TEXT("val"), 0.0)));
			});

		Reg.RegisterSimple(NS, FP, TEXT("floor"), false, 0.f,
			TEXT("Floor"), TEXT("val:float"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				return FRuneValue::MakeNumber(FMath::FloorToDouble(A.GetFloat(TEXT("val"), 0.0)));
			});

		Reg.RegisterSimple(NS, FP, TEXT("ceil"), false, 0.f,
			TEXT("Ceiling"), TEXT("val:float"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				return FRuneValue::MakeNumber(FMath::CeilToDouble(A.GetFloat(TEXT("val"), 0.0)));
			});

		Reg.RegisterSimple(NS, FP, TEXT("pow"), false, 0.f,
			TEXT("Power"), TEXT("base:float, exp:float"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				const double B = A.GetFloat(TEXT("base"), 1.0);
				const double E = A.GetFloat(TEXT("exp"),  1.0);
				return FRuneValue::MakeNumber(FMath::Pow(B, E));
			});

		Reg.RegisterSimple(NS, FP, TEXT("min"), false, 0.f,
			TEXT("Minimum"), TEXT("a:float, b:float"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				return FRuneValue::MakeNumber(FMath::Min(A.GetFloat(TEXT("a"), 0.0), A.GetFloat(TEXT("b"), 0.0)));
			});

		Reg.RegisterSimple(NS, FP, TEXT("max"), false, 0.f,
			TEXT("Maximum"), TEXT("a:float, b:float"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				return FRuneValue::MakeNumber(FMath::Max(A.GetFloat(TEXT("a"), 0.0), A.GetFloat(TEXT("b"), 0.0)));
			});
	}

	// =========================================================================
	// target — info about last hit target (Phase 2: stubs; Phase 3 wires real game state)
	// =========================================================================
	void Register_Target(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("target"));
		const FName FP(TEXT("target"));

		Reg.RegisterSimple(NS, FP, TEXT("count"), false, 0.f,
			TEXT("Count enemies within radius of hit pos"),
			TEXT("radius:float=8"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
			{
				const float Radius = (float)A.GetFloat(TEXT("radius"), 8.0);
				const int32 Count = TechnoMancy::Effects::CountActorsInSphere(C, C.LastHitPos, Radius);
				return FRuneValue::MakeNumber(Count);
			});

		Reg.RegisterSimple(NS, FP, TEXT("health_pct"), false, 0.f,
			TEXT("Target HP as 0..1 (0 if no target)"), TEXT(""),
			[](const FRuneLibCallArgs&, FRuneWeaponContext& C) -> FRuneValue
			{
				return FRuneValue::MakeNumber(C.LastHitTarget.IsValid() ? 1.0 : 0.0);
			});

		Reg.RegisterSimple(NS, FP, TEXT("distance"), false, 0.f,
			TEXT("Distance from owner to last hit target"), TEXT(""),
			[](const FRuneLibCallArgs&, FRuneWeaponContext& C) -> FRuneValue
			{
				if (C.WeaponOwner.IsValid() && C.LastHitTarget.IsValid())
				{
					const float D = FVector::Dist(C.WeaponOwner->GetActorLocation(), C.LastHitTarget->GetActorLocation());
					return FRuneValue::MakeNumber(D);
				}
				return FRuneValue::MakeNumber(0.0);
			});

		Reg.RegisterSimple(NS, FP, TEXT("is_frozen"),   false, 0.f, TEXT("1 if frozen"),   TEXT(""), MakeReturning0(TEXT("target.is_frozen")));
		Reg.RegisterSimple(NS, FP, TEXT("is_burning"),  false, 0.f, TEXT("1 if burning"),  TEXT(""), MakeReturning0(TEXT("target.is_burning")));
		Reg.RegisterSimple(NS, FP, TEXT("is_marked"),   false, 0.f, TEXT("1 if void-marked"), TEXT(""), MakeReturning0(TEXT("target.is_marked")));
		Reg.RegisterSimple(NS, FP, TEXT("is_stunned"),  false, 0.f, TEXT("1 if stunned"),  TEXT(""), MakeReturning0(TEXT("target.is_stunned")));
		Reg.RegisterSimple(NS, FP, TEXT("is_poisoned"), false, 0.f, TEXT("1 if poisoned"), TEXT(""), MakeReturning0(TEXT("target.is_poisoned")));

		Reg.RegisterSimple(NS, FP, TEXT("apply_force"), false, 4.f,
			TEXT("Apply physics impulse"),
			TEXT("force:float=400, dir:float=0"),
			MakeStub(TEXT("target.apply_force")));
	}

	// =========================================================================
	// self
	// =========================================================================
	void Register_Self(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("self"));
		const FName FP(TEXT("self"));

		Reg.RegisterSimple(NS, FP, TEXT("health_pct"),    false, 0.f, TEXT("Owner HP as 0..1"), TEXT(""), MakeReturning0(TEXT("self.health_pct")));
		Reg.RegisterSimple(NS, FP, TEXT("mana_pct"),      false, 0.f, TEXT("Same as mana.pct"), TEXT(""),
			[](const FRuneLibCallArgs&, FRuneWeaponContext& C) -> FRuneValue
			{
				const double P = (C.MaxMana > 0.f) ? (C.Mana / C.MaxMana) : 0.0;
				return FRuneValue::MakeNumber(P);
			});
		Reg.RegisterSimple(NS, FP, TEXT("is_moving"),     false, 0.f, TEXT("1 if owner has velocity > 10"), TEXT(""),
			[](const FRuneLibCallArgs&, FRuneWeaponContext& C) -> FRuneValue
			{
				if (C.WeaponOwner.IsValid())
				{
					const float Speed = C.WeaponOwner->GetVelocity().Size();
					return FRuneValue::MakeNumber(Speed > 10.f ? 1.0 : 0.0);
				}
				return FRuneValue::MakeNumber(0.0);
			});
		Reg.RegisterSimple(NS, FP, TEXT("speed"),         false, 0.f, TEXT("Owner current speed (cm/s)"), TEXT(""),
			[](const FRuneLibCallArgs&, FRuneWeaponContext& C) -> FRuneValue
			{
				if (C.WeaponOwner.IsValid()) return FRuneValue::MakeNumber(C.WeaponOwner->GetVelocity().Size());
				return FRuneValue::MakeNumber(0.0);
			});
		Reg.RegisterSimple(NS, FP, TEXT("heal"),          false, 8.f, TEXT("Heal owner; returns new HP"),
			TEXT("amount:float=10"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext&) -> FRuneValue
			{
				LogCall(TEXT("self.heal"), A);
				return FRuneValue::MakeNumber(A.GetFloat(TEXT("amount"), 10.0));
			});
		Reg.RegisterSimple(NS, FP, TEXT("boost_speed"),   false, 8.f, TEXT("Temporary speed buff"),
			TEXT("mult:float=1.5, duration:float=3"), MakeStub(TEXT("self.boost_speed")));
		Reg.RegisterSimple(NS, FP, TEXT("boost_damage"),  false, 10.f, TEXT("Temporary damage buff"),
			TEXT("mult:float=1.5, duration:float=3"), MakeStub(TEXT("self.boost_damage")));
	}

	// =========================================================================
	// time
	// =========================================================================
	void Register_Time(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("time"));
		const FName FP(TEXT("time"));

		Reg.RegisterSimple(NS, FP, TEXT("elapsed"),    false, 0.f, TEXT("Seconds since weapon was drawn"), TEXT(""),
			MakeReturning0(TEXT("time.elapsed"))); // Phase 3: track draw time

		Reg.RegisterSimple(NS, FP, TEXT("game_time"),  false, 0.f, TEXT("Current world time in seconds"), TEXT(""),
			[](const FRuneLibCallArgs&, FRuneWeaponContext& C) -> FRuneValue
			{
				if (C.WeaponOwner.IsValid() && C.WeaponOwner->GetWorld())
				{
					return FRuneValue::MakeNumber(C.WeaponOwner->GetWorld()->GetTimeSeconds());
				}
				return FRuneValue::MakeNumber(0.0);
			});

		Reg.RegisterSimple(NS, FP, TEXT("sin_wave"),   false, 0.f, TEXT("sin(game_time * freq) * amp"),
			TEXT("freq:float=1, amp:float=1"),
			[](const FRuneLibCallArgs& A, FRuneWeaponContext& C) -> FRuneValue
			{
				const double Freq = A.GetFloat(TEXT("freq"), 1.0);
				const double Amp  = A.GetFloat(TEXT("amp"),  1.0);
				double T = 0.0;
				if (C.WeaponOwner.IsValid() && C.WeaponOwner->GetWorld())
				{
					T = C.WeaponOwner->GetWorld()->GetTimeSeconds();
				}
				return FRuneValue::MakeNumber(FMath::Sin(T * Freq) * Amp);
			});

		Reg.RegisterSimple(NS, FP, TEXT("slow_field"), false, 20.f, TEXT("World-space time dilation field"),
			TEXT("radius:float=4, factor:float=0.5, duration:float=3"),
			MakeStub(TEXT("time.slow_field")));
	}

	// =========================================================================
	// status — generic status effect management
	// =========================================================================
	void Register_Status(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("status"));
		const FName FP(TEXT("status"));

		Reg.RegisterSimple(NS, FP, TEXT("apply"),  false, 5.f,
			TEXT("Apply status. Types: 0=burn,1=freeze,2=stun,3=slow,4=poison,5=mark,6=bleed"),
			TEXT("type:float, duration:float, power:float=1"),
			MakeStub(TEXT("status.apply")));
		Reg.RegisterSimple(NS, FP, TEXT("remove"), false, 3.f, TEXT("Remove status type from target"),
			TEXT("type:float"), MakeStub(TEXT("status.remove")));
		Reg.RegisterSimple(NS, FP, TEXT("has"),    false, 0.f, TEXT("1 if target has status, 0 otherwise"),
			TEXT("type:float"), MakeReturning0(TEXT("status.has")));
		Reg.RegisterSimple(NS, FP, TEXT("count"),  false, 0.f, TEXT("Number of active statuses on target"),
			TEXT(""), MakeReturning0(TEXT("status.count")));
	}

	// =========================================================================
	// chain — projectile modifier flags (set BEFORE firing projectile)
	// =========================================================================
	void Register_Chain(FRuneLibraryRegistry& Reg)
	{
		const FName NS(TEXT("chain"));
		const FName FP(TEXT("chain"));

		Reg.RegisterSimple(NS, FP, TEXT("bounce"), false, 5.f, TEXT("Next fired projectile bounces N times"),
			TEXT("count:float=2, dmg_falloff:float=0.7"), MakeStub(TEXT("chain.bounce")));
		Reg.RegisterSimple(NS, FP, TEXT("pierce"), false, 4.f, TEXT("Next fired projectile pierces N targets"),
			TEXT("count:float=2"), MakeStub(TEXT("chain.pierce")));
		Reg.RegisterSimple(NS, FP, TEXT("split"),  false, 8.f, TEXT("Next fired projectile splits on first hit"),
			TEXT("count:float=2, angle:float=15"), MakeStub(TEXT("chain.split")));
	}
}

void TechnoMancy::BuiltinLibraries::RegisterHelpers()
{
	FRuneLibraryRegistry& Reg = FRuneLibraryRegistry::Get();
	Register_Mana(Reg);
	Register_Math(Reg);
	Register_Target(Reg);
	Register_Self(Reg);
	Register_Time(Reg);
	Register_Status(Reg);
	Register_Chain(Reg);
}
