#pragma once

#include <cstdio>
#include <fstream>
#include <string>

namespace lunanx::storage
{

// FAT-backed libnx filesystems may reject rename() when the destination exists.
// Move the old file aside, install the complete temporary file, then clean up.
inline bool ReplaceWithTemporaryFile(const std::string& temporary_path, const std::string& destination_path)
{
    const std::string backup_path = destination_path + ".bak";
    std::remove(backup_path.c_str());

    if (std::rename(temporary_path.c_str(), destination_path.c_str()) == 0)
        return true;

    if (std::rename(destination_path.c_str(), backup_path.c_str()) != 0)
    {
        std::remove(temporary_path.c_str());
        return false;
    }

    if (std::rename(temporary_path.c_str(), destination_path.c_str()) == 0)
    {
        std::remove(backup_path.c_str());
        return true;
    }

    // Keep the last known-good settings when installing the new file fails.
    std::rename(backup_path.c_str(), destination_path.c_str());
    std::remove(temporary_path.c_str());
    return false;
}

} // namespace lunanx::storage

namespace lunanx
{

inline bool WriteFileAtomically(const std::string& path, const std::string& contents)
{
    const std::string temporary_path = path + ".tmp";
    {
        std::ofstream stream(temporary_path, std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
            return false;
        stream << contents;
        if (!stream.good())
            return false;
    }
    return storage::ReplaceWithTemporaryFile(temporary_path, path);
}

} // namespace lunanx
