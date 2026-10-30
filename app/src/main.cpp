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
#include "core/runtime_journal.hpp"
#include "ui/cover_image_cache.hpp"
#include "ui/library_view.hpp"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    lunanx::PrepareAppStorage();
    lunanx::InitializeRuntimeJournal(lunanx::kAppVersion);
#ifdef __SWITCH__
    appletSetWirelessPriorityMode(AppletWirelessPriorityMode_OptimizedForWlan);
#endif

    int exit_code = EXIT_FAILURE;
    try
    {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
            throw std::runtime_error("curl_global_init");
        // Interface en anglais pour l'instant (le français reviendra avec un sélecteur fiable).
        lunanx::SetUiLanguage("en");
        brls::Platform::APP_LOCALE_DEFAULT = "en-US";
        if (!brls::Application::init())
            throw std::runtime_error("brls::Application::init");

        brls::Application::createWindow("LunaNX");
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
