// Original Cfg_Categories/Cfg_Configs fixed-width fields via the qualified DBC
// reader and authenticated SFile owner. Names/flags come from unchanged data.
#include "RealmData.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include <deque>
#include <stdexcept>
WowClientDB<Cfg_CategoriesRec> g_cfg_CategoriesDB;
WowClientDB<Cfg_ConfigsRec> g_cfg_ConfigsDB;
namespace GlueNative {
Blizzard::Assets::Dbc ReadGlueDbc(const char* path, uint32_t fields) {
    SFileImpl* file = nullptr;
    if (!SFileOpenFile(path, &file)) throw std::runtime_error(std::string("Missing original Glue database: ") + path);
    struct Close { SFileImpl* file; ~Close() { SFileCloseFile(file); } } close{file};
    const auto size = SFileGetFileSize(file, nullptr);
    if (size < 20 || size > 32 * 1024 * 1024) throw std::runtime_error("Invalid original Glue database extent");
    Blizzard::Assets::Bytes bytes(size);
    uint32_t read = 0;
    if (!SFileReadFile(file, bytes.data(), size, &read, nullptr, 0) || read != size)
        throw std::runtime_error("Incomplete original Glue database read");
    Blizzard::Assets::Dbc result(bytes);
    if (result.Fields() != fields) throw std::runtime_error("Unexpected 12340 Glue database fields");
    return result;
}
void CloseRealmData() {
    if (g_cfg_CategoriesDB.m_loaded) g_cfg_CategoriesDB.Free();
    if (g_cfg_ConfigsDB.m_loaded) g_cfg_ConfigsDB.Free();
    g_cfg_CategoriesDB.Reset(); g_cfg_ConfigsDB.Reset();
}
void LoadRealmData() {
    if (g_cfg_CategoriesDB.m_loaded) return;
    try {
        (void)ReadGlueDbc("DBFilesClient\\Cfg_Categories.dbc", 21);
        (void)ReadGlueDbc("DBFilesClient\\Cfg_Configs.dbc", 4);
        g_cfg_CategoriesDB.Load(__FILE__, __LINE__);
        g_cfg_ConfigsDB.Load(__FILE__, __LINE__);
        if (!g_cfg_CategoriesDB.m_numRecords || !g_cfg_ConfigsDB.m_numRecords)
            throw std::runtime_error("Empty original realm metadata");
    } catch (...) { CloseRealmData(); throw; }
}
}
