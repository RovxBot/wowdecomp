#ifndef WOWDECOMP_NATIVE_GLUE_MODELS_H
#define WOWDECOMP_NATIVE_GLUE_MODELS_H
#include <cstdint>
class AnimationDataRec;
namespace GlueNative {
void RegisterModels();
void UnregisterModels();
AnimationDataRec* AnimationRecord(uint32_t id);
}
#endif
