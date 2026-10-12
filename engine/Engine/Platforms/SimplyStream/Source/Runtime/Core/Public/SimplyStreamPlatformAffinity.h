// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
LuminAffinity.h: Lumin affinity profile masks definitions.
==============================================================================================*/

#pragma once

#include "GenericPlatform/GenericPlatformAffinity.h"
// wndr 5.8: stock now declares `class FPlatformAffinity : public FPlatformAffinityBase` and includes
// the platform header for the base type (was `FPlatformAffinity` directly in 5.7).
typedef FGenericPlatformAffinity FPlatformAffinityBase;
