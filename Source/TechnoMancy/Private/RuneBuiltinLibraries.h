// Copyright TechnoMancy. All rights reserved.
//
// Forward declarations for the built-in library registration entry points,
// called from FTechnoMancyModule::StartupModule. Implementations live in
// RuneLibraries_Elemental.cpp and RuneLibraries_Helper.cpp.

#pragma once

namespace TechnoMancy::BuiltinLibraries
{
	void RegisterElemental();
	void RegisterHelpers();
}
