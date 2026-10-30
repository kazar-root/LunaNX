#include "settings_view.hpp"

#include "core/app_paths.hpp"
#include "core/app_version.hpp"
#include "core/i18n.hpp"
#include "luna/amazon_auth.hpp"

#include <cstdio>

#include <algorithm>

namespace lunanx
{
namespace
{

int IndexOf(const std::vector<std::string>& values, const std::string& value)
{
    const auto found = std::find(values.begin(), values.end(), value);
    return found == values.end() ? 0 : static_cast<int>(found - values.begin());
}

const std::vector<std::string> kRegionCodes = {
    "Auto", "eu-west-3", "eu-west-2", "eu-central-1", "eu-central-2", "eu-south-1",
    "eu-south-2", "eu-north-1", "us-east-1", "us-east-2", "us-west-1", "us-west-2",
};
const std::vector<std::string> kVolumeValues = {"400", "800", "1200", "1600"};
const std::vector<std::string> kBufferValues = {"30", "40", "60", "100"};
const std::vector<std::string> kLayoutCodes = {"Switch", "Xbox"};
const std::vector<std::string> kLibraryCodes = {"Covers", "List"};

} // namespace

SettingsView::SettingsView()
    : brls::Box(brls::Axis::COLUMN)
    , settings_(LoadStreamSettings())
{
    setGrow(1.0f);
    setBackgroundColor(nvgRGB(12, 11, 18));
    setPadding(26, 60, 20, 60);

    auto* header = new brls::Box(brls::Axis::ROW);
    header->setAlignItems(brls::AlignItems::CENTER);
    header->setMarginBottom(16);
    auto* title = new brls::Label();
    title->setText(Tr("Réglages"));
    title->setFontSize(30);
    title->setTextColor(nvgRGB(176, 150, 255));
    title->setGrow(1.0f);
    header->addView(title);
    auto* back = new brls::Button();
    back->setText(Tr("Retour"));
    back->setStyle(&brls::BUTTONSTYLE_BORDERED);
    back->setHeight(44);
    back->registerClickAction([](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
    header->addView(back);
    addView(header);

    auto* scroller = new brls::ScrollingFrame();
    scroller->setGrow(1.0f);
    auto* list = new brls::Box(brls::Axis::COLUMN);
    scroller->setContentView(list);
    addView(scroller);

    // Compte Amazon : pays de la boutique et déconnexion de la console.
    std::vector<std::string> market_labels;
    std::vector<std::string> market_codes;
    for (const auto& market : luna::AmazonMarkets())
    {
        market_labels.push_back(Tr(market.label));
        market_codes.push_back(market.code);
    }
    auto* market = new brls::SelectorCell();
    market->init(Tr("Compte Amazon"), market_labels, IndexOf(market_codes, settings_.amazon_market),
                 [this, market_codes](int index) {
                     settings_.amazon_market = market_codes[index];
                     Save();
                 });
    list->addView(market);

    luna::DeviceRegistration registration;
    const bool registered = luna::AmazonAuth::Load(registration);
    auto* logout = new brls::DetailCell();
    logout->setText(Tr("Se déconnecter d'Amazon"));
    logout->setDetailText(registered ? (registration.customer_name.empty() ? Tr("Connecté")
                                                                           : registration.customer_name)
                                     : Tr("Non connecté"));
    logout->registerClickAction([logout](brls::View*) {
        luna::DeviceRegistration current;
        if (luna::AmazonAuth::Load(current))
        {
            // Retire la console des appareils du compte, puis efface la session locale.
            try { luna::AmazonAuth::Deregister(current); } catch (...) {}
        }
        luna::AmazonAuth::Erase();
        std::remove((AppHomePath() + "/cookies.txt").c_str());
        std::remove((AppHomePath() + "/cookies.json").c_str());
        logout->setDetailText(Tr("Déconnecté"));
        brls::Application::notify(Tr("Session Amazon supprimée de la console"));
        return true;
    });
    list->addView(logout);

    auto* library = new brls::SelectorCell();
    library->init(Tr("Affichage de la bibliothèque"), {Tr("Jaquettes"), Tr("Liste simple")},
                  IndexOf(kLibraryCodes, settings_.library_layout), [this](int index) {
                      settings_.library_layout = kLibraryCodes[index];
                      Save();
                  });
    list->addView(library);

    auto* region = new brls::SelectorCell();
    region->init(Tr("Région du serveur"), kRegionCodes, IndexOf(kRegionCodes, settings_.region),
                 [this](int index) {
                     settings_.region = kRegionCodes[index];
                     Save();
                 });
    list->addView(region);

    auto* layout = new brls::SelectorCell();
    layout->init(Tr("Boutons A/B/X/Y"),
                 {Tr("Étiquettes Switch (A = A)"), Tr("Position Xbox (A Switch = B Xbox)")},
                 IndexOf(kLayoutCodes, settings_.controller_layout), [this](int index) {
                     settings_.controller_layout = kLayoutCodes[index];
                     Save();
                 });
    list->addView(layout);

    auto* audio = new brls::BooleanCell();
    audio->init(Tr("Son"), settings_.audio_enabled, [this](bool on) {
        settings_.audio_enabled = on;
        Save();
    });
    list->addView(audio);

    auto* volume = new brls::SelectorCell();
    volume->init("Volume", {"x4", "x8", "x12", "x16"},
                 IndexOf(kVolumeValues, std::to_string(settings_.audio_volume)), [this](int index) {
                     settings_.audio_volume = std::stoi(kVolumeValues[index]);
                     Save();
                 });
    list->addView(volume);

    auto* buffer = new brls::SelectorCell();
    buffer->init(Tr("Tampon audio"), {"30 ms", "40 ms", "60 ms", "100 ms"},
                 IndexOf(kBufferValues, std::to_string(settings_.audio_buffer_ms)), [this](int index) {
                     settings_.audio_buffer_ms = std::stoi(kBufferValues[index]);
                     Save();
                 });
    list->addView(buffer);

    auto* debug = new brls::BooleanCell();
    debug->init(Tr("Infos techniques en jeu (L3 + R3)"), settings_.debug_diagnostics, [this](bool on) {
        settings_.debug_diagnostics = on;
        Save();
    });
    list->addView(debug);

    auto* about = new brls::Label();
    about->setText(TrFormat("LunaNX {} · par KazaR-RooT · licence GPL-3.0\nClient non officiel, sans lien "
                            "avec Amazon ni Nintendo.\nQuitter une partie : ZL + ZR + Moins.",
                            {kAppVersion}));
    about->setFontSize(15);
    about->setTextColor(nvgRGB(130, 134, 145));
    about->setMarginTop(20);
    list->addView(about);

    registerAction(Tr("Retour"), brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
}

void SettingsView::Save()
{
    SaveStreamSettings(settings_);
}

} // namespace lunanx
