#pragma once

#include "../CommonDef.h"
#include "RawImage.h"
#include "AOTHomologousImage.h"

namespace hybridclr
{
namespace metadata
{

    class Assembly
    {
    public:
        static Il2CppAssembly* LoadFromFile(const char* assemblyFile);
        static Il2CppAssembly* LoadFromBytes(const void* assemblyData, uint64_t length, bool copyData);
        static LoadImageErrorCode LoadMetadataForAOTAssembly(const void* dllBytes, uint32_t dllSize, HomologousImageMode mode);
        static void InitializeDifferentialHybridAssembles();
        static LoadImageErrorCode LoadDifferentialHybridAssembly(const void* dllBytes, uint32_t dllSize, const void* optionData, uint32_t optionCount);
    private:
        static Il2CppAssembly* Create(const byte* assemblyData, uint64_t length, bool copyData);
    };
}
}