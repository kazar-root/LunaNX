// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=5e9566f3403234ce60a9
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

// Public origin metadata. This does not enforce restrictions or modify application behavior.
// Third-party components retain their original authors and licenses.
#define LUNANX_ORIGIN_PROJECT_ID "urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed"
#define LUNANX_ORIGIN_CREATOR "KazaR-RooT"

#if defined(__GNUC__) && defined(__ELF__)
#define LUNANX_ORIGIN_ATTRIBUTES __attribute__((used, section(".rodata.lunanx_origin")))
#elif defined(__GNUC__)
#define LUNANX_ORIGIN_ATTRIBUTES __attribute__((used))
#else
#define LUNANX_ORIGIN_ATTRIBUTES [[maybe_unused]]
#endif

// The generated linker response file explicitly retains each external symbol through
// --gc-sections, without a runtime call or a change to the Switch memory layout.
#define LUNANX_SOURCE_ORIGIN(symbol, unit) \
    extern "C" { LUNANX_ORIGIN_ATTRIBUTES extern const char lunanx_origin_##symbol[] = \
        "LunaNX-Origin/v1|project=" LUNANX_ORIGIN_PROJECT_ID "|creator=" LUNANX_ORIGIN_CREATOR "|unit=" unit; }
