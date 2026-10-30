#include "switch_rumble.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <mutex>

namespace lunanx::input
{

#ifdef __SWITCH__
namespace
{

struct VibrationTarget
{
    HidNpadIdType id;
    u32 style = 0;
    HidVibrationDeviceHandle handles[2] {};
};

std::mutex g_mutex;
VibrationTarget g_targets[] = {{HidNpadIdType_Handheld}, {HidNpadIdType_No1}};

u32 VibrationStyle(u32 styles)
{
    for (u32 style : {static_cast<u32>(HidNpadStyleTag_NpadHandheld), static_cast<u32>(HidNpadStyleTag_NpadFullKey),
                      static_cast<u32>(HidNpadStyleTag_NpadJoyDual), static_cast<u32>(HidNpadStyleTag_NpadJoyLeft),
                      static_cast<u32>(HidNpadStyleTag_NpadJoyRight)})
        if (styles & style)
            return style;
    return 0;
}

} // namespace

void SwitchRumble::Send(std::uint8_t heavy, std::uint8_t light)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    // Chaque actionneur HD reçoit les deux moteurs : basse fréquence (160 Hz) pour le
    // moteur lourd, haute fréquence (320 Hz) pour le léger, comme le font les jeux Switch.
    HidVibrationValue value {};
    value.amp_low = heavy / 255.0f;
    value.freq_low = 160.0f;
    value.amp_high = light / 255.0f;
    value.freq_high = 320.0f;
    const HidVibrationValue values[2] = {value, value};

    for (auto& target : g_targets)
    {
        const u32 style = VibrationStyle(hidGetNpadStyleSet(target.id));
        if (!style)
            continue;
        if (style != target.style)
        {
            const s32 count = style == HidNpadStyleTag_NpadJoyLeft || style == HidNpadStyleTag_NpadJoyRight ? 1 : 2;
            if (R_FAILED(hidInitializeVibrationDevices(target.handles, count, target.id,
                                                       static_cast<HidNpadStyleTag>(style))))
                continue;
            target.style = style;
        }
        const s32 count = style == HidNpadStyleTag_NpadJoyLeft || style == HidNpadStyleTag_NpadJoyRight ? 1 : 2;
        hidSendVibrationValues(target.handles, values, count);
    }
}
#else
void SwitchRumble::Send(std::uint8_t, std::uint8_t) {}
#endif

} // namespace lunanx::input
