// Clean-room reconstruction from static analysis of Resources_DE_2024-01-25.
// Not original NextMU source.

#ifdef _WIN32
#include <windows.h>
#include <string>

namespace NextMUReference
{
    struct LoadedBackendModule
    {
        HMODULE module = nullptr;
        FARPROC factory = nullptr;
        std::string moduleName;
    };

    static const char* ArchitectureSuffix()
    {
    #if defined(_WIN64)
        return "_64r";
    #else
        return "_32r";
    #endif
    }

    LoadedBackendModule LoadBackendModule(const char* engineBaseName,
                                          const char* factoryName)
    {
        LoadedBackendModule out;
        out.moduleName = std::string{engineBaseName} + ArchitectureSuffix() + ".dll";

        out.module = ::LoadLibraryA(out.moduleName.c_str());
        if (!out.module)
            return out;

        out.factory = ::GetProcAddress(out.module, factoryName);
        if (!out.factory)
        {
            ::FreeLibrary(out.module);
            out.module = nullptr;
        }

        return out;
    }
}
#endif
