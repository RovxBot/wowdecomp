#ifndef WOW_SOURCE_WOWSERVICES_CHARACTERINFO_H
#define WOW_SOURCE_WOWSERVICES_CHARACTERINFO_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Object/WGUID.h"

// Original record, separated from socket/client implementation dependencies.
class CHARACTER_INFO {
    public:
    /* 0000 */ WOWGUID m_guid;
    /* 0008 */ char m_name [48];
    /* 0038 */ uint32_t m_mapID;
    /* 003C */ uint32_t m_zoneID;
    /* 0040 */ uint32_t m_guildID;
    /* 0044 */ C3Vector m_position;
    /* 0050 */ uint32_t m_inventoryItemDisplayID [23];
    /* 00AC */ uint32_t m_inventoryItemType [23];
    /* 0108 */ uint32_t m_inventoryItemVisualID [23];
    /* 0164 */ uint32_t m_petDisplayInfoID;
    /* 0168 */ uint32_t m_petExperienceLevel;
    /* 016C */ uint32_t m_petCreatureFamilyID;
    /* 0170 */ uint32_t m_flags;
    /* 0174 */ uint32_t m_customizeFlags;
    /* 0178 */ uint8_t m_raceID;
    /* 0179 */ uint8_t m_classID;
    /* 017A */ uint8_t m_sexID;
    /* 017B */ uint8_t m_skinColorID;
    /* 017C */ uint8_t m_faceID;
    /* 017D */ uint8_t m_hairStyleID;
    /* 017E */ uint8_t m_hairColorID;
    /* 017F */ uint8_t m_facialHairStyleID;
    /* 0180 */ uint8_t m_experienceLevel;
    /* 0181 */ uint8_t m_firstLogin;
    /* 0182 */ uint8_t unused_0182[6];
};

#endif
