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
