// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=bff94a3e4880ee79fb49
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#pragma once

template <typename T> class Singleton {
  public:
    static T& instance() {
        static T instance;
        return instance;
    }

    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton) = delete;

  protected:
    Singleton() {}
};
