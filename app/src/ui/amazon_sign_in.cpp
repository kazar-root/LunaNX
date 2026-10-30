#include "amazon_sign_in.hpp"

#include "core/i18n.hpp"
#include "core/runtime_journal.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <stdexcept>
#include <vector>

namespace lunanx
{

std::string OpenAmazonWebSignIn(const luna::PendingSignIn& pending)
{
#ifdef __SWITCH__
    // Le navigateur système n'est disponible qu'en mode application (pas depuis l'Album).
    const AppletType type = appletGetAppletType();
    if (type != AppletType_Application && type != AppletType_SystemApplication)
        throw std::runtime_error(Tr(
            "Le navigateur de la Switch n'est pas disponible en mode applet.\n"
            "Lance LunaNX en maintenant R au démarrage d'un jeu, puis réessaie."));

    WebCommonConfig config;
    Result rc = webPageCreate(&config, pending.sign_in_url.c_str());
    if (R_FAILED(rc))
        throw std::runtime_error(TrFormat("Impossible d'ouvrir le navigateur (code {})", {std::to_string(rc)}));

    // Le navigateur se ferme tout seul dès qu'Amazon redirige vers la page finale.
    webConfigSetCallbackUrl(&config, pending.callback_url.c_str());
    webConfigSetFooter(&config, true);

    WebCommonReply reply;
    rc = webConfigShow(&config, &reply);
    if (R_FAILED(rc))
        throw std::runtime_error(TrFormat("Le navigateur s'est fermé avec une erreur (code {})", {std::to_string(rc)}));

    WebExitReason reason = WebExitReason_ExitButton;
    webReplyGetExitReason(&reply, &reason);
    if (reason != WebExitReason_LastUrl)
    {
        LogRuntimeEvent("auth", "web_closed", "reason=" + std::to_string(static_cast<int>(reason)));
        return {};
    }

    std::vector<char> url(0x1000, '\0');
    size_t size = 0;
    webReplyGetLastUrl(&reply, url.data(), url.size(), &size);
    return std::string(url.data(), size > 0 && url[size - 1] == '\0' ? size - 1 : size);
#else
    (void)pending;
    throw std::runtime_error(Tr("Connexion par navigateur disponible uniquement sur Switch"));
#endif
}

} // namespace lunanx
