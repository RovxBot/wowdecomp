#ifndef WOWDECOMP_NATIVE_FRAME_XML_FACTORIES_H
#define WOWDECOMP_NATIVE_FRAME_XML_FACTORIES_H
#include <functional>
class XMLNode;
class CScriptObject;
class CSimpleFrame;
class CSimpleFontString;
class CSimpleTexture;
struct lua_State;
namespace FrameXML { namespace Native {
// Owned by Frame::Native::Scripts. Retained virtual nodes close before XML pools.
void InstallFactories();
void CloseFactories();
void RequireFactories();
void ValidateCreation(XMLNode*);
void RequireAvailableName(CScriptObject*, const char*);
void RequireTableWrite(lua_State*, int index, const char* key);
CSimpleFontString* CreateFontString(CSimpleFrame*, const char* name, unsigned layer, const char* inherits);
CSimpleTexture* CreateTexture(CSimpleFrame*, const char* name, unsigned layer, const char* inherits);
void ApplyTemplates(XMLNode*, const std::function<void(XMLNode*)>&, bool applyTaint = true);
}}
#endif
