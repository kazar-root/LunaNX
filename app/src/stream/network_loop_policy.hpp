// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=fc1212880f97086c704c
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

namespace lunanx::network
{

constexpr int LoopBackoffMilliseconds(bool ready, int processed_datagrams)
{
    if (processed_datagrams > 0)
        return 0;
    return ready ? 1 : 2;
}

} // namespace lunanx::network
