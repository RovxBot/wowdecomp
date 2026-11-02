// Native selection of original 12340 character services; no world object manager.
#include "RealmData.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/Glue/CSimpleModelFFX.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/WorldClient/DayNight__DNInfo.h"
#include "Engine/Source/FFX/FFX.h"
#include "Engine/Source/Os/OsFile.h"
#include <stdexcept>
#include "Engine/Source/Images/MippedImg.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Tempest/C2iVector.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
WowClientDB<AreaTableRec> g_areaTableDB;
WowClientDB<CharHairGeosetsRec> g_charHairGeosetsDB;
WowClientDB<CharSectionsRec> g_charSectionsDB;
WowClientDB<CharacterFacialHairStylesRec> g_characterFacialHairStylesDB;
WowClientDB<ChrClassesRec> g_chrClassesDB;
WowClientDB<ChrRacesRec> g_chrRacesDB;
WowClientDB<CreatureDisplayInfoRec> g_creatureDisplayInfoDB;
WowClientDB<CreatureFamilyRec> g_creatureFamilyDB;
WowClientDB<CreatureModelDataRec> g_creatureModelDataDB;
WowClientDB<HelmetGeosetVisDataRec> g_helmetGeosetVisDataDB;
WowClientDB_RLE<ItemDisplayInfoRec> g_itemDisplayInfoDB;
WowClientDB<ItemVisualEffectsRec> g_itemVisualEffectsDB;
WowClientDB<ItemVisualsRec> g_itemVisualsDB;
WowClientDB<LightParamsRec> g_lightParamsDB;
WowClientDB<ParticleColorRec> g_particleColorDB;
WowClientDB<SpellVisualEffectNameRec> g_spellVisualEffectNameDB;
WowClientDB<SpellVisualKitRec> g_spellVisualKitDB;
int32_t g_clientDbPerformCompress = 0; // Original uncompressed runtime record path.
uint32_t CGPlayer_C::s_displayId;
CHARACTER_INFO* CGlueMgr::m_characterInfo;
int32_t CGlueMgr::s_displayedCharacterIndex;
CVar* g_lastCharacterIndexVar;
FFX::Effect* CGlueMgr::m_deathEffect;
namespace GlueNative {
namespace { bool characterOpen = false; }
void LoadCharacterServices() {
    if (characterOpen) return;
    (void)ReadGlueDbc(AreaTableRec::GetFilename(), AreaTableRec::s_numColumns);
    g_areaTableDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(CharHairGeosetsRec::GetFilename(), CharHairGeosetsRec::s_numColumns);
    g_charHairGeosetsDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(CharSectionsRec::GetFilename(), CharSectionsRec::s_numColumns);
    g_charSectionsDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(CharacterFacialHairStylesRec::GetFilename(), CharacterFacialHairStylesRec::s_numColumns);
    g_characterFacialHairStylesDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(ChrClassesRec::GetFilename(), ChrClassesRec::s_numColumns);
    g_chrClassesDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(ChrRacesRec::GetFilename(), ChrRacesRec::s_numColumns);
    g_chrRacesDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(CreatureDisplayInfoRec::GetFilename(), CreatureDisplayInfoRec::s_numColumns);
    g_creatureDisplayInfoDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(CreatureFamilyRec::GetFilename(), CreatureFamilyRec::s_numColumns);
    g_creatureFamilyDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(CreatureModelDataRec::GetFilename(), CreatureModelDataRec::s_numColumns);
    g_creatureModelDataDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(HelmetGeosetVisDataRec::GetFilename(), HelmetGeosetVisDataRec::s_numColumns);
    g_helmetGeosetVisDataDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(ItemDisplayInfoRec::GetFilename(), ItemDisplayInfoRec::s_numColumns);
    g_itemDisplayInfoDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(ItemVisualEffectsRec::GetFilename(), ItemVisualEffectsRec::s_numColumns);
    g_itemVisualEffectsDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(ItemVisualsRec::GetFilename(), ItemVisualsRec::s_numColumns);
    g_itemVisualsDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(LightParamsRec::GetFilename(), LightParamsRec::s_numColumns);
    g_lightParamsDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(ParticleColorRec::GetFilename(), ParticleColorRec::s_numColumns);
    g_particleColorDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(SpellVisualEffectNameRec::GetFilename(), SpellVisualEffectNameRec::s_numColumns);
    g_spellVisualEffectNameDB.Load(__FILE__, __LINE__);
    (void)ReadGlueDbc(SpellVisualKitRec::GetFilename(), SpellVisualKitRec::s_numColumns);
    g_spellVisualKitDB.Load(__FILE__, __LINE__);
    g_lastCharacterIndexVar = CVar::Register("lastCharacterIndex", "Last selected character", 0, "0", nullptr, 0, false, nullptr, false);
    if (!CCharacterComponent::Initialize(GxTex_Rgb565, 8, 0, 0))
        throw std::runtime_error("Original scalar character component initialization failed");
    characterOpen = true;
}
void CloseCharacterServices() {
    if (characterOpen) {
        CCharacterSelection::Shutdown();
        CCharacterComponent::Destroy();
        characterOpen = false;
    }
    CCharacterSelection::m_modelFrame = nullptr;
    if (g_spellVisualKitDB.m_loaded) g_spellVisualKitDB.Free();
    g_spellVisualKitDB.Reset();
    if (g_spellVisualEffectNameDB.m_loaded) g_spellVisualEffectNameDB.Free();
    g_spellVisualEffectNameDB.Reset();
    if (g_particleColorDB.m_loaded) g_particleColorDB.Free();
    g_particleColorDB.Reset();
    if (g_lightParamsDB.m_loaded) g_lightParamsDB.Free();
    g_lightParamsDB.Reset();
    if (g_itemVisualsDB.m_loaded) g_itemVisualsDB.Free();
    g_itemVisualsDB.Reset();
    if (g_itemVisualEffectsDB.m_loaded) g_itemVisualEffectsDB.Free();
    g_itemVisualEffectsDB.Reset();
    if (g_itemDisplayInfoDB.m_loaded) g_itemDisplayInfoDB.Free();
    g_itemDisplayInfoDB.Reset();
    if (g_helmetGeosetVisDataDB.m_loaded) g_helmetGeosetVisDataDB.Free();
    g_helmetGeosetVisDataDB.Reset();
    if (g_creatureModelDataDB.m_loaded) g_creatureModelDataDB.Free();
    g_creatureModelDataDB.Reset();
    if (g_creatureFamilyDB.m_loaded) g_creatureFamilyDB.Free();
    g_creatureFamilyDB.Reset();
    if (g_creatureDisplayInfoDB.m_loaded) g_creatureDisplayInfoDB.Free();
    g_creatureDisplayInfoDB.Reset();
    if (g_chrRacesDB.m_loaded) g_chrRacesDB.Free();
    g_chrRacesDB.Reset();
    if (g_chrClassesDB.m_loaded) g_chrClassesDB.Free();
    g_chrClassesDB.Reset();
    if (g_characterFacialHairStylesDB.m_loaded) g_characterFacialHairStylesDB.Free();
    g_characterFacialHairStylesDB.Reset();
    if (g_charSectionsDB.m_loaded) g_charSectionsDB.Free();
    g_charSectionsDB.Reset();
    if (g_charHairGeosetsDB.m_loaded) g_charHairGeosetsDB.Free();
    g_charHairGeosetsDB.Reset();
    if (g_areaTableDB.m_loaded) g_areaTableDB.Free();
    g_areaTableDB.Reset();
    g_lastCharacterIndexVar = nullptr;
}
}
uint32_t GetDisplayIdFromRaceAndGender(uint32_t race, uint32_t gender) {
    if (gender >= 3) {
        SErrDisplayAppFatalMessage("", "", 0);
    }

    ChrRacesRec* raceRec = g_chrRacesDB.GetRecord(race);

    if (!raceRec) {
        return 0;
    }

    switch (gender) {
        case 0: {
            return raceRec->m_maleDisplayID;
        }

        case 1: {
            return raceRec->m_femaleDisplayID;
        }

        default: {
            return 0;
        }
    }
}
CreatureModelDataRec* GetModelDataRec(uint32_t race, uint32_t gender) {
    if (gender >= 3) {
        SErrDisplayAppFatalMessage("", "", 0);
    }

    uint32_t displayID = GetDisplayIdFromRaceAndGender(race, gender);

    if (!displayID) {
        return nullptr;
    }

    CreatureDisplayInfoRec* displayRec = g_creatureDisplayInfoDB.GetRecord(displayID);

    if (!displayRec) {
        DebugLog("Error, unknown displayInfo %d specified for player race %d sex %d!", displayID, race, gender);
    }

    CreatureModelDataRec* modelRec = g_creatureModelDataDB.GetRecord(displayRec->m_modelID);

    if (!modelRec) {
        DebugLog("Error, unknown model record %d specified for player race %d sex %d!", displayRec->m_modelID, race, gender);
    }

    return modelRec;
}
const char* CGUnit_C::GetDisplayRaceNameFromRecord(const ChrRacesRec* rec, int32_t sex, int32_t* displaySex) {
    if (displaySex) {
        *displaySex = sex;
    }

    if (!rec) {
        return nullptr;
    }

    if (sex == 0) {
        if (*rec->m_nameMale) {
            return rec->m_nameMale;
        }

        if (*rec->m_nameFemale) {
            if (displaySex) {
                *displaySex = 1;
            }

            return rec->m_nameFemale;
        }
    } else if (sex == 1) {
        if (*rec->m_nameFemale) {
            return rec->m_nameFemale;
        }

        if (*rec->m_nameMale) {
            if (displaySex) {
                *displaySex = 0;
            }

            return rec->m_nameMale;
        }
    }

    return rec->m_name;
}
const char* CGUnit_C::GetDisplayClassNameFromRecord(const ChrClassesRec* rec, int32_t sex, int32_t* displaySex) {
    if (displaySex) {
        *displaySex = sex;
    }

    if (!rec) {
        return nullptr;
    }

    if (sex == 0) {
        if (*rec->m_nameMale) {
            return rec->m_nameMale;
        }

        if (*rec->m_nameFemale) {
            if (displaySex) {
                *displaySex = 1;
            }

            return rec->m_nameFemale;
        }
    } else if (sex == 1) {
        if (*rec->m_nameFemale) {
            return rec->m_nameFemale;
        }

        if (*rec->m_nameMale) {
            if (displaySex) {
                *displaySex = 0;
            }

            return rec->m_nameMale;
        }
    }

    return rec->m_name;
}
namespace DayNight { DNInfo g_dnInfo; }
DayNight::DNInfo::DNInfo() {
    this->m_timeOfDay = 0;
    this->m_dayProgression = 0.0f;
    this->m_day = 0.0f;
    this->m_faceAngle = 0.0f;
    this->m_farClip = 0.0f;
    this->m_timeSec = 0.0f;
    this->m_deltaSec = 0.0f;
    this->m_weatherIntensity = 0.0f;
    this->m_flashBlend = 0;
    this->m_overrideLightParamsId = 0;
    this->m_skybox = nullptr;
    this->m_skyboxWeight = 0.0f;

    for (int32_t i = 0; i < 3; i++) {
        this->m_blendSkybox[i] = nullptr;
        this->m_blendSkyboxWeight[i] = 0.0f;
        this->m_blendSkyboxFlags[i] = 0;
    }

    this->m_stormBlend = 0.0f;
    this->m_interiorFogBlend = 0.0f;
    this->m_interiorMapObjDef = nullptr;
    this->m_showSky = 0;
    this->m_sunMoonPath = 0.0f;
    this->m_curve0 = 0.0f;
    this->m_curve1 = 0.0f;
    this->m_nearLightCount = 0;

    for (int32_t i = 0; i < 5; i++) {
        this->m_nearLightBlend[i] = 0.0f;
    }
}
DayNight::DNInfo* DayNight::GetInfo() { return &g_dnInfo; }
CImVector DayNight::LoadLightIntBand(int32_t, const LightParamsRec*, int32_t) {
    throw std::logic_error("Original ghost lighting band evaluation is unavailable");
}
// No streaming mini-layer/world loading layer is admitted by this retail profile.
void LoadingScreenMiniDisable() {}
void LoadingScreenMiniEnable() { throw std::logic_error("Native streaming loading layer is unavailable"); }
void LoadingScreenMiniUpdate(float) { LoadingScreenMiniEnable(); }

DayNight::CurrentLight::CurrentLight() {
    this->m_skyboxes[0].m_weight = 0.0f;
    this->m_skyboxes[1].m_weight = 0.0f;
    this->m_skyboxes[0].m_id = 0;
    this->m_skyboxes[2].m_weight = 0.0f;
    this->m_skyboxes[1].m_id = 0;
    this->m_skyboxes[2].m_id = 0;
    this->m_cloudWeight = 0.0f;
    this->m_cloudTypeId = 0;
    this->m_fogEnd = 0.0f;
    this->m_fogStartMul = 0.0f;
    this->m_fogRate = 0.0f;
    this->m_highlightSky = 0.0f;
    this->m_glow = 0.0f;
    this->m_floatBand2 = 0.0f;
    this->m_cloudDensity = 0.0f;
    this->m_floatBand4 = 0.0f;
    this->m_floatBand5 = 0.0f;
    this->m_waterShallowAlpha = 0.0f;
    this->m_waterDeepAlpha = 0.0f;
    this->m_oceanShallowAlpha = 0.0f;
    this->m_oceanDeepAlpha = 0.0f;
}

// Original 0x004B5510 mip count and uncached 0x004B7220/0x004B7300
// allocation path. Native ownership avoids the legacy global mip-cache lifetime.
int32_t TextureCalcMipCount(uint32_t width, uint32_t height) {
    if (!width || !height || width>16384 || height>16384)
        throw std::out_of_range("Invalid native character texture extent");
    int32_t count=1;
    while (width>1 || height>1) { width=width>1 ? width>>1 : 1; height=height>1 ? height>>1 : 1; ++count; }
    return count;
}
MipBits* TextureAllocMippedImg(PIXEL_FORMAT format, uint32_t width, uint32_t height) {
    (void)TextureCalcMipCount(width,height);
    if ((format!=PIXEL_ARGB8888 && format!=PIXEL_DXT1) || width>512 || height>512)
        throw std::invalid_argument("Unavailable native component mip allocation");
    return MippedImgAllocA(format,width,height,__FILE__,__LINE__);
}
void TextureFreeMippedImg(MipBits* bits, PIXEL_FORMAT, uint32_t, uint32_t) {
    SMemFree(bits,__FILE__,__LINE__,0);
}
