#pragma once
#include <cstdint>
class XMLNode;
class CStatus;
namespace RovRetailUI {
// Production FrameXML admission. Locale is the existing SFile locale tag.
bool LoadFontFamily(XMLNode* node, uint32_t locale, CStatus* status);
}
