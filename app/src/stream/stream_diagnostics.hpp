// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=9f5d0aae66ff065c8747
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

namespace lunanx
{

// One runtime gate for expensive stream diagnostics. The persisted setting is
// applied before a stream starts, so render/audio worker threads can query it.
void SetStreamDiagnosticsEnabled(bool enabled);
bool StreamDiagnosticsEnabled();
void SetSensitiveInputLoggingSuppressed(bool suppressed);
bool SensitiveInputLoggingSuppressed();

} // namespace lunanx
