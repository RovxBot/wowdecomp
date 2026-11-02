#pragma once
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "Engine/Source/BlizzardCore/Portable/Assets.h"
namespace GlueNative {
Blizzard::Assets::Dbc ReadGlueDbc(const char* path, uint32_t fields);
void LoadRealmData();
void CloseRealmData();
void LoadCharacterServices();
void CloseCharacterServices();
}
