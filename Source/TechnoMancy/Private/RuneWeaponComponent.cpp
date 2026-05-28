// Copyright TechnoMancy. All rights reserved.

#include "RuneWeaponComponent.h"
#include "RuneInterpreter.h"
#include "RuneSpellAsset.h"
#include "RuneProjectile.h"
#include "TechnoMancyLog.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

URuneWeaponComponent::URuneWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void URuneWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URuneWeaponComponent, CurrentMana);
	DOREPLIFETIME(URuneWeaponComponent, SpellSource);
}

void URuneWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() == ROLE_Authority)
	{
		ApplySpellAssetDefaults();
		CurrentMana = MaxMana;

		if (!SpellSource.IsEmpty())
		{
			TArray<FString> Errors;
			if (!CompileSpellInternal(SpellSource, Errors))
			{
				for (const FString& E : Errors)
				{
					UE_LOG(LogTechnoMancy, Warning, TEXT("[RuneWeapon] compile failed: %s"), *E);
				}
			}
		}
	}

	SetComponentTickEnabled(bAllowTickSpell);
}

void URuneWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwnerRole() != ROLE_Authority) return;
	if (bAllowTickSpell && bReady)
	{
		TArray<FString> Errors;
		InvokeTick(DeltaTime, Errors);
	}
}

void URuneWeaponComponent::ApplySpellAssetDefaults()
{
	// Player-facing asset takes precedence; default-only weapons fall back to DefaultSpellAsset.
	URuneSpellAsset* Source = SpellAsset ? SpellAsset.Get() : DefaultSpellAsset.Get();
	if (!Source) return;

	if (SpellSource.IsEmpty())
	{
		SpellSource = Source->SourceCode;
	}
	if (StorageCapacity == 128 && Source->StorageCapacity > 0)
	{
		StorageCapacity = Source->StorageCapacity;
	}
	if (FMath::IsNearlyEqual(MaxMana, 50.f) && Source->MaxMana > 0.f)
	{
		MaxMana = Source->MaxMana;
	}
}

// =============================================================================
// Compile / request
// =============================================================================

void URuneWeaponComponent::RequestCompile(const FString& NewSource)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		// Server-side caller — compile directly and fire the BP event for symmetry.
		TArray<FString> Errors;
		const bool bOk = CompileSpellInternal(NewSource, Errors);
		OnCompileResult.Broadcast(bOk, Errors);
		return;
	}
	Server_RequestCompile(NewSource);
}

bool URuneWeaponComponent::CompileSpell(const FString& NewSource, TArray<FString>& OutErrors)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		OutErrors.Add(TEXT("CompileSpell can only run on the server — use RequestCompile from clients"));
		return false;
	}
	return CompileSpellInternal(NewSource, OutErrors);
}

bool URuneWeaponComponent::CompileSpellInternal(const FString& NewSource, TArray<FString>& OutErrors)
{
	if (NewSource.Len() > StorageCapacity)
	{
		OutErrors.Add(FString::Printf(
			TEXT("Spell is %d characters — storage limit is %d"),
			NewSource.Len(), StorageCapacity));
		bReady = false;
		return false;
	}

	if (!Interpreter.IsValid())
	{
		Interpreter = MakeShared<FRuneInterpreter>();
	}

	bReady = Interpreter->Compile(NewSource, OutErrors);
	if (bReady)
	{
		SpellSource = NewSource; // replicated to clients
		UE_LOG(LogTechnoMancy, Display, TEXT("[RuneWeapon] compiled %d chars"), NewSource.Len());
	}
	return bReady;
}

bool URuneWeaponComponent::Server_RequestCompile_Validate(const FString& NewSource)
{
	// Bound at 1.5x StorageCapacity to catch sloppy clients while still allowing some slack.
	return NewSource.Len() <= 4096 * 2;
}

void URuneWeaponComponent::Server_RequestCompile_Implementation(const FString& NewSource)
{
	TArray<FString> Errors;
	if (!bAllowPlayerEdits)
	{
		Errors.Add(TEXT("This weapon's spell is locked by the designer"));
		Client_OnCompileResult(false, Errors);
		return;
	}
	const bool bOk = CompileSpellInternal(NewSource, Errors);
	Client_OnCompileResult(bOk, Errors);
}

void URuneWeaponComponent::Client_OnCompileResult_Implementation(bool bSuccess, const TArray<FString>& Errors)
{
	OnCompileResult.Broadcast(bSuccess, Errors);
}

void URuneWeaponComponent::OnRep_SpellSource()
{
	OnSpellSourceChanged.Broadcast(SpellSource);
}

// =============================================================================
// Preset / default workflow
// =============================================================================

void URuneWeaponComponent::ResetToDefault()
{
	if (GetOwnerRole() != ROLE_Authority) return;
	if (!bAllowPlayerEdits) return;
	if (!DefaultSpellAsset) return;

	TArray<FString> Errors;
	CompileSpellInternal(DefaultSpellAsset->SourceCode, Errors);
}

bool URuneWeaponComponent::HasCustomSpell() const
{
	if (!DefaultSpellAsset) return !SpellSource.IsEmpty();
	return SpellSource != DefaultSpellAsset->SourceCode;
}

FText URuneWeaponComponent::GetDefaultSpellName() const
{
	if (DefaultSpellAsset && !DefaultSpellAsset->DisplayName.IsEmpty())
	{
		return DefaultSpellAsset->DisplayName;
	}
	return FText::FromString(TEXT("None"));
}

// =============================================================================
// Invocation
// =============================================================================

void URuneWeaponComponent::SetLastHit(AActor* HitTarget, FVector HitPos)
{
	Context.LastHitTarget = HitTarget;
	Context.LastHitPos = HitPos;
}

void URuneWeaponComponent::RefreshContext()
{
	Context.WeaponOwner = GetOwner();
	Context.MaxMana = MaxMana;
	Context.ProjectileClass = ProjectileClass.Get() ? ProjectileClass.Get() : ARuneProjectile::StaticClass();
	if (AActor* Owner = GetOwner())
	{
		Context.AimDirection = Owner->GetActorForwardVector();
	}
}

void URuneWeaponComponent::RefillMana()
{
	if (GetOwnerRole() != ROLE_Authority) return;
	CurrentMana = MaxMana;
}

namespace
{
	bool InvokeEntry(TSharedPtr<FRuneInterpreter> Interp,
					 FRuneWeaponContext& Ctx,
					 FName EntryPoint,
					 float& CurrentMana,
					 TArray<FString>& OutErrors)
	{
		if (!Interp.IsValid())
		{
			OutErrors.Add(TEXT("Weapon is not ready (no interpreter)"));
			return false;
		}
		Ctx.Mana = CurrentMana;
		const bool bOk = Interp->Invoke(EntryPoint, Ctx, OutErrors);
		CurrentMana = Ctx.Mana;
		return bOk;
	}
}

bool URuneWeaponComponent::InvokePrimary(TArray<FString>& OutErrors)
{
	if (GetOwnerRole() != ROLE_Authority) return true; // silent no-op on clients
	if (!bReady) { OutErrors.Add(TEXT("Weapon is not ready")); return false; }
	RefreshContext();
	return InvokeEntry(Interpreter, Context, FName(TEXT("Primary")), CurrentMana, OutErrors);
}

bool URuneWeaponComponent::InvokeOnHit(TArray<FString>& OutErrors)
{
	if (GetOwnerRole() != ROLE_Authority) return true;
	if (!bReady) { OutErrors.Add(TEXT("Weapon is not ready")); return false; }
	RefreshContext();
	return InvokeEntry(Interpreter, Context, FName(TEXT("OnHit")), CurrentMana, OutErrors);
}

bool URuneWeaponComponent::InvokeOnKill(TArray<FString>& OutErrors)
{
	if (GetOwnerRole() != ROLE_Authority) return true;
	if (!bReady) { OutErrors.Add(TEXT("Weapon is not ready")); return false; }
	RefreshContext();
	return InvokeEntry(Interpreter, Context, FName(TEXT("OnKill")), CurrentMana, OutErrors);
}

bool URuneWeaponComponent::InvokeTick(float DeltaTime, TArray<FString>& OutErrors)
{
	if (GetOwnerRole() != ROLE_Authority) return true;
	if (!bReady) return true;
	RefreshContext();
	Context.DeltaTime = DeltaTime;
	return InvokeEntry(Interpreter, Context, FName(TEXT("Tick")), CurrentMana, OutErrors);
}

float URuneWeaponComponent::GetManaEstimate(FName EntryPoint) const
{
	return Interpreter.IsValid() ? Interpreter->EstimateManaCost(EntryPoint) : 0.f;
}
