// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=4354a4d86b2d7393e3db
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).
#include "core/source_origin.hpp"
LUNANX_SOURCE_ORIGIN(u4354a4d86b2d7393e3db, "4354a4d86b2d7393e3db")

// LunaNX — client Amazon Luna non officiel pour Nintendo Switch.
// Copyright (C) 2026 KazaR-RooT — licence GPL-3.0 (voir LICENSE).

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <curl/curl.h>

#include <borealis.hpp>

#include <cstdlib>
#include <exception>
#include <stdexcept>

#include "core/app_paths.hpp"
#include "core/app_version.hpp"
#include "core/i18n.hpp"
#include "core/stream_settings.hpp"
#include "core/runtime_journal.hpp"
#include "core/provenance.hpp"
#include "ui/cover_image_cache.hpp"
#include "ui/library_view.hpp"
#include "ui/theme.hpp"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    lunanx::PrepareAppStorage();
    lunanx::InitializeRuntimeJournal(lunanx::kAppVersion);
    lunanx::LogRuntimeEvent("app", "provenance", lunanx::BuildProvenanceSummary());
#ifdef __SWITCH__
    appletSetWirelessPriorityMode(AppletWirelessPriorityMode_OptimizedForWlan);
#endif

    int exit_code = EXIT_FAILURE;
    try
    {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
            throw std::runtime_error("curl_global_init");
        // Langue choisie dans les réglages (anglais au premier lancement). Borealis n'a ses
        // propres textes (indications des boutons) qu'en anglais et en français.
        lunanx::SetUiLanguage(lunanx::LoadStreamSettings().language);
        brls::Platform::APP_LOCALE_DEFAULT = lunanx::UiLanguage() == "fr" ? "fr" : "en-US";
        if (!brls::Application::init())
            throw std::runtime_error("brls::Application::init");

        brls::Application::createWindow("LunaNX");
        // Polices Nunito et palette LunaNX (après la fenêtre : il faut le contexte NanoVG).
        lunanx::ui::InitializeTheme();
        // Plus sert de bouton Start en jeu : on désactive la sortie globale de Borealis.
        brls::Application::setGlobalQuit(false);
        brls::Application::setFPSStatus(false);
        brls::Application::pushActivity(new lunanx::LibraryActivity(new lunanx::LibraryView()),
                                        brls::TransitionAnimation::FADE);

        while (brls::Application::mainLoop())
        {
        }
        exit_code = EXIT_SUCCESS;
    }
    catch (const std::exception& ex)
    {
        lunanx::LogRuntimeEvent("app", "fatal", ex.what());
    }

    lunanx::ShutdownImageCache();
    lunanx::ShutdownRuntimeJournal();
    curl_global_cleanup();
    return exit_code;
}
