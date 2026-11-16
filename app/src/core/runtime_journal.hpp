// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=4c8bbe9a37eda03ea53d
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

#include <cstdint>
#include <string>

namespace lunanx
{

void InitializeRuntimeJournal(const std::string& app_version);
void ShutdownRuntimeJournal();
void LogRuntimeEvent(
    const std::string& category,
    const std::string& event,
    const std::string& detail = {});

std::uint64_t BeginRuntimeOperation(
    const std::string& category,
    const std::string& operation,
    const std::string& detail = {});
void EndRuntimeOperation(
    std::uint64_t operation_id,
    const std::string& category,
    const std::string& operation,
    const std::string& result,
    const std::string& detail = {});

std::string SanitizeRuntimeUrl(const std::string& url);
const std::string& RuntimeJournalPath();

} // namespace lunanx
