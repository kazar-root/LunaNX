// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=87809b9bf9ea1373d572
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).
#include "core/source_origin.hpp"
LUNANX_SOURCE_ORIGIN(u87809b9bf9ea1373d572, "87809b9bf9ea1373d572")

#include "provenance.hpp"

#include "app_version.hpp"
#include "lunanx_build_provenance.hpp"

namespace lunanx
{

std::string BuildProvenanceSummary()
{
    return std::string("creator=") + provenance::kCreator + " project=" + provenance::kProjectId +
           " sources=" + provenance::kSourceTreeSha256 + " manifest=" + provenance::kManifestSha256;
}

std::string BuildProvenanceDetails()
{
    return std::string("LunaNX ") + kAppVersion + "\n\nKazaR-RooT\n" + provenance::kRepository +
           "\n\nProject ID\n" + provenance::kProjectId + "\n\nSource SHA-256\n" +
           provenance::kSourceTreeSha256 + "\n\nManifest SHA-256\n" + provenance::kManifestSha256 +
           "\n\nGPL-3.0 · THIRD_PARTY_NOTICES.md";
}

} // namespace lunanx
