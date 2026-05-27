// Copyright TechnoMancy. All rights reserved.

#include "TechnoMancy.h"
#include "TechnoMancyLog.h"
#include "RuneInterpreter.h"
#include "RuneLibraryRegistry.h"
#include "RuneBuiltinLibraries.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogTechnoMancy);

namespace
{
	static void Cmd_SelfTest()
	{
		FString Log;
		const bool bOk = FRuneInterpreter::SelfTest(Log);
		UE_LOG(LogTechnoMancy, Display, TEXT("=== RuneScript Self-Test: %s ==="), bOk ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogTechnoMancy, Display, TEXT("%s"), *Log);
	}

	static FAutoConsoleCommand GSelfTestCmd(
		TEXT("runescript.test"),
		TEXT("Run the TechnoMancy / RuneScript core self-test"),
		FConsoleCommandDelegate::CreateStatic(&Cmd_SelfTest));
}

void FTechnoMancyModule::StartupModule()
{
	UE_LOG(LogTechnoMancy, Display, TEXT("TechnoMancy module starting up"));

	FRuneLibraryRegistry& Reg = FRuneLibraryRegistry::Get();
	TechnoMancy::BuiltinLibraries::RegisterElemental();
	TechnoMancy::BuiltinLibraries::RegisterHelpers();
	Reg.MarkInitialised();

	UE_LOG(LogTechnoMancy, Display,
		TEXT("TechnoMancy registered %d built-in library functions"),
		Reg.GetAllEntries().Num());
}

void FTechnoMancyModule::ShutdownModule()
{
	FRuneLibraryRegistry::Shutdown();
}

IMPLEMENT_MODULE(FTechnoMancyModule, TechnoMancy)
