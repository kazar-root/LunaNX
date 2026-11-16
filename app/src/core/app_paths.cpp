// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=23c239828debbf16bc21
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).
#include "core/source_origin.hpp"
LUNANX_SOURCE_ORIGIN(u23c239828debbf16bc21, "23c239828debbf16bc21")

#include "app_paths.hpp"

#include <sys/stat.h>

namespace lunanx
{

const std::string& AppHomePath()
{
#ifdef __SWITCH__
    static const std::string path = "sdmc:/switch/LunaNX";
#else
    static const std::string path = "./LunaNX";
#endif
    return path;
}

void PrepareAppStorage()
{
#ifdef __SWITCH__
    mkdir("sdmc:/switch", 0777);
#endif
    mkdir(AppHomePath().c_str(), 0777);
}

} // namespace lunanx
