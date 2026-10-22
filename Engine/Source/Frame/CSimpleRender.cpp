#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleRender.h"
#include <malloc.h>
#include <cctype>
#include <cstring>
#include "Engine/Source/Base/ConvertUTF.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Frame/CSimpleTexture.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/Frame/CRenderBatch.h"
#include "Engine/Source/Frame/CSimpleEmbeddedTexture.h"
#include "Engine/Source/Gxu/GxuFontString.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDevice/CGxShader.h"
#include "Engine/Source/Gx/CGxVertexPCT.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gxu/GxuFont.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Services/TextBlock.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CScriptRegion.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include <cmath>
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Frame/CSimpleFont.h"
#include "Engine/Source/Frame/CSimpleFontStringAttributes.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/FrameScript/Types.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Tempest/CMath.h"
#include "Engine/Source/Frame/CFramePoint.h"
#include "Engine/Source/Frame/CSimpleRegion.h"
#include "Engine/Source/Services/SysMessage.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "Engine/Source/Tempest/CRect.h"

// VERIFY: f1967c67 auto
// OFFSET: 0x00485AF0
void CSimpleEmbeddedTexture::InitEmbeddedTexture(CSimpleFontString* string, const GXUEMBEDDEDTEXTUREINFO* info) {
    if (string && info) {
        if (!this->m_texture) {
            return;
        }

        char path[256];
        SStrCopy(path, info->m_path, info->m_pathLength + 1);

        if (this->m_texture->SetTexture(path, 0, 0, CSimpleTexture::s_textureFilterMode, ImageMode_Normal)) {
            float top = 0.0f;
            float left = 0.0f;
            float baseLine = 0.0f;
            float right = 0.0f;
            NDCToDDC(info->m_left, info->m_baseLineHeight, &left, &baseLine);
            NDCToDDC(info->m_right, info->m_lineHeight, &right, &top);

            float offsetX = NDCToDDCWidth(info->m_offsetX);
            float offsetY = NDCToDDCHeight(info->m_offsetY);
            float width = NDCToDDCWidth(info->m_width);
            float height = NDCToDDCHeight(info->m_height);

            uint32_t styleFlags = string->m_styleFlags;
            float x = 0.0f;
            float y = 0.0f;
            uint32_t point = FRAMEPOINT_TOPLEFT;

            switch (styleFlags & 0x7) {
                case 0x1:
                    x = left + offsetX;
                    break;

                case 0x2:
                    point = FRAMEPOINT_TOP;
                    x = (right + left) * 0.5f + offsetX;
                    break;

                case 0x4:
                    point = FRAMEPOINT_TOPRIGHT;
                    x = right + offsetX;
                    break;
            }

            switch (styleFlags & 0x38) {
                case 0x8:
                    y = top + offsetY;
                    break;

                case 0x10:
                    point += 3;
                    y = (string->GetStringHeight() * string->m_layoutScale + height) * 0.5f + baseLine + offsetY;
                    break;

                case 0x20:
                    point += 6;
                    y = string->GetStringHeight() + baseLine + offsetY;
                    break;
            }

            this->m_texture->MarkFramePointsUnused();
            this->m_texture->SetPoint(static_cast<FRAMEPOINT>(point), string, static_cast<FRAMEPOINT>(point), x, y, 1);
            this->m_texture->SetWidth(width);
            this->m_texture->SetHeight(height);
            this->m_texture->SetVertexAlpha(string->GetVertexAlpha());

            if (info->m_hasTexCoords) {
                this->m_texture->SetTexCoord(info->m_texCoords);
            } else {
                CRect texRect = { 0.0f, 0.0f, 1.0f, 1.0f };
                this->m_texture->SetTexCoord(texRect);
            }

            this->m_texture->m_shown = 1;
            this->m_texture->ShowThis();
            return;
        }
    }

    if (this->m_texture) {
        this->m_texture->m_shown = 0;
        this->m_texture->HideThis();
    }
}
