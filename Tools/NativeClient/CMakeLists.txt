# Integrated startup, using the already qualified native ABI and services.
add_library(WowNativeStartup STATIC
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleHTML.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleHTML_Scripts.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleHyperlinkedFrame.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleScrollFrame.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleScrollFrame_Scripts.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleSlider.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleSlider_Scripts.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/Portable/LoginWidgets.cpp
    ${PROJECT_SOURCE_DIR}/WoW/Source/Glue/GlueMgr.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/ConsoleBase/ConsoleVar.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/ConsoleBase/ConsoleCommand.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/ConsoleBase/Portable/Console.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Event/Portable/Loop.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/FrameXML/Portable/Load.cpp
    ${PROJECT_SOURCE_DIR}/WoW/Source/Glue/Portable/Startup.cpp)
target_sources(WowNativeStartup PRIVATE
    ${PROJECT_SOURCE_DIR}/WoW/Source/Glue/Portable/CharacterSelection.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Services/Portable/LegacyFiles.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Os/Portable/ModelMemory.cpp)
target_link_libraries(WowNativeStartup PUBLIC WowNativeFrameRegions)
add_library(WowNativeModels STATIC
    ${PROJECT_SOURCE_DIR}/Engine/Source/Model2/M2Cache.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Model2/M2Data.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Model2/M2Light.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Model2/M2Model.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Model2/M2Scene.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Model2/M2Shared.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Model2/CM2SceneRender.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Services/Camera.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Services/ParticleSystem2.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Services/RibbonEmitter.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/ShaderEffect/ShaderEffect.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/ShaderEffect/ShaderEffectManager.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/ShaderEffect/ShaderEffectParser.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/ShaderEffect/ShadowCache.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/FFX/FFX.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/FFX/FFX_CVar.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/FFX/EffectGlow.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/FFX/PassBox4.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/FFX/PassGauss4.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/FFX/PassGlow.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleModel.cpp
    ${PROJECT_SOURCE_DIR}/WoW/Source/Glue/CSimpleModelFFX.cpp)
target_link_libraries(WowNativeModels PUBLIC WowNativeStartup)
# Diagnostic spellings only, scoped to this Linux source group.
target_compile_definitions(WowNativeModels PRIVATE raw_name=name _alloca=alloca)
target_compile_options(WowNativeModels PRIVATE -include alloca.h -fms-extensions)
if(WOWDECOMP_CORE_SANITIZERS)
    target_compile_options(WowNativeModels PRIVATE -g1)
endif()
add_executable(WowNativeClient main.cpp)
target_link_libraries(WowNativeClient PRIVATE WowNativeStartup)
add_dependencies(WowNativeClient WowNativeModels)
add_executable(NativeStartupCheck startup_check.cpp model_io_check.cpp)
target_link_libraries(NativeStartupCheck PRIVATE WowNativeStartup)
if(BUILD_TESTING)
    add_test(NAME native-integrated-startup-contracts COMMAND NativeStartupCheck)
endif()
if(WOWDECOMP_CORE_SANITIZERS)
    target_compile_options(WowNativeStartup PRIVATE -g1)
    target_compile_options(WowNativeClient PRIVATE -g1)
endif()
