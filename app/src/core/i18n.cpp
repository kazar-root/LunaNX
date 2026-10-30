#include "i18n.hpp"

#include <unordered_map>

namespace lunanx
{
namespace
{

std::string g_language = "en";

const std::unordered_map<std::string, std::string>& EnglishTexts()
{
    static const std::unordered_map<std::string, std::string> texts = {
        // Bibliothèque
        {"Actualiser", "Refresh"},
        {"Réglages", "Settings"},
        {"Quitter", "Quit"},
        {"Connexion…", "Connecting…"},
        {"Chargement du catalogue…", "Loading catalog…"},
        {"Ta session Amazon a expiré ({}). Reconnecte-toi.", "Your Amazon session has expired ({}). Please sign in again."},
        {"Impossible de charger Luna", "Unable to load Luna"},
        {"{}\n\nVérifie la connexion Internet puis appuie sur Y.", "{}\n\nCheck your Internet connection, then press Y."},
        {"Connecté", "Connected"},
        {"Connecté · profil Luna actif", "Connected · Luna profile active"},
        {"Connexion à ton compte Amazon", "Sign in to your Amazon account"},
        {"La page de connexion officielle d'Amazon ({}) va s'ouvrir dans le navigateur de la Switch. "
         "Saisis ton e-mail, ton mot de passe et, si besoin, le code de validation reçu.\n"
         "Ton mot de passe n'est jamais lu ni enregistré par LunaNX : la console est simplement ajoutée "
         "à tes appareils Amazon (« LunaNX »), et tu peux la retirer à tout moment depuis ton compte ou "
         "dans les Réglages.\nCompte d'un autre pays ? Change-le dans les Réglages (X).",
         "The official Amazon sign-in page ({}) will open in the Switch browser. Enter your email, "
         "your password and, if asked, the verification code you receive.\n"
         "Your password is never read or stored by LunaNX: the console is simply added to your Amazon "
         "devices (\"LunaNX\"), and you can remove it at any time from your account or in Settings.\n"
         "Account from another country? Change it in Settings (X)."},
        {"Se connecter avec Amazon", "Sign in with Amazon"},
        {"Solution de secours si la connexion directe échoue : exporte une seule fois la session de ton "
         "navigateur.\n1. Sur ton PC, connecte-toi sur luna.amazon.fr.\n2. Avec l'extension « Get "
         "cookies.txt LOCALLY », clique sur « Export » (fichier cookies.txt).\n3. Copie ce fichier dans "
         "{}/ puis appuie sur Y.\nATTENTION : ce fichier donne un accès complet à ton compte Amazon. Ne "
         "le partage jamais.",
         "Fallback if direct sign-in fails: export your browser session once.\n1. On your PC, sign in "
         "on your Luna website (e.g. luna.amazon.com).\n2. With the \"Get cookies.txt LOCALLY\" "
         "extension, click \"Export\" (cookies.txt file).\n3. Copy this file to {}/ then press Y.\n"
         "WARNING: this file gives full access to your Amazon account. Never share it."},
        {"Connexion annulée.", "Sign-in cancelled."},
        {"Amazon n'a pas renvoyé de code de connexion.", "Amazon did not return a sign-in code."},
        {"Enregistrement de la console sur ton compte…", "Registering the console on your account…"},
        {"Impossible d'enregistrer la session sur la carte SD.", "Unable to save the session to the SD card."},
        {"Aucun jeu trouvé", "No games found"},
        {"Le catalogue Luna n'a renvoyé aucun jeu.\n\nVérifie ton abonnement Luna, puis appuie sur Y pour réessayer.",
         "The Luna catalog returned no games.\n\nCheck your Luna subscription, then press Y to try again."},
        {"{} jeux disponibles", "{} games available"},
        {"Jouer", "Play"},
        {"Reprendre", "Continue playing"},
        {"Tes dernières parties", "Your recent games"},
        {"Les jeux du moment sur Luna", "Trending on Luna"},
        {"Tous les jeux", "All games"},
        {"Afficher {} jeux de plus  ({} restants)", "Show {} more games  ({} left)"},
        {"À LA UNE SUR LUNA", "FEATURED ON LUNA"},

        // Réglages
        {"Retour", "Back"},
        {"Langue", "Language"},
        {"Compte Amazon", "Amazon account"},
        {"Se déconnecter d'Amazon", "Sign out of Amazon"},
        {"Non connecté", "Not signed in"},
        {"Déconnecté", "Signed out"},
        {"Session Amazon supprimée de la console", "Amazon session removed from the console"},
        {"Affichage de la bibliothèque", "Library view"},
        {"Jaquettes", "Covers"},
        {"Liste simple", "Simple list"},
        {"Région du serveur", "Server region"},
        {"Boutons A/B/X/Y", "A/B/X/Y buttons"},
        {"Position Xbox (A Switch = B Xbox)", "Xbox position (Switch A = Xbox B)"},
        {"Étiquettes Switch (A = A)", "Switch labels (A = A)"},
        {"Son", "Sound"},
        {"Tampon audio", "Audio buffer"},
        {"Infos techniques en jeu (L3 + R3)", "In-game technical info (L3 + R3)"},
        {"LunaNX {} · par KazaR-RooT · licence GPL-3.0\nClient non officiel, sans lien avec Amazon ni "
         "Nintendo.\nQuitter une partie : ZL + ZR + Moins.",
         "LunaNX {} · by KazaR-RooT · GPL-3.0 license\nUnofficial client, not affiliated with Amazon or "
         "Nintendo.\nQuit a game: ZL + ZR + Minus."},
        {"France (amazon.fr)", "France (amazon.fr)"},
        {"Allemagne (amazon.de)", "Germany (amazon.de)"},
        {"Royaume-Uni (amazon.co.uk)", "United Kingdom (amazon.co.uk)"},
        {"Espagne (amazon.es)", "Spain (amazon.es)"},
        {"Italie (amazon.it)", "Italy (amazon.it)"},
        {"États-Unis (amazon.com)", "United States (amazon.com)"},

        // Partie
        {"Préparation…", "Preparing…"},
        {"Préparation de « {} »…", "Preparing \"{}\"…"},
        {"Recherche du serveur le plus proche…", "Finding the nearest server…"},
        {"Réservation d'une machine Luna…", "Reserving a Luna machine…"},
        {"En file d'attente…", "In queue…"},
        {"Démarrage de la machine distante…", "Starting the remote machine…"},
        {"Connexion au flux vidéo…", "Connecting to the video stream…"},
        {"En jeu", "Playing"},
        {"La partie est terminée ou la connexion a été perdue.", "The game has ended or the connection was lost."},
        {"Connexion WebRTC impossible ({})", "WebRTC connection failed ({})"},
        {"Impossible de créer la connexion WebRTC", "Unable to create the WebRTC connection"},
        {"Offre WebRTC locale incomplète", "Incomplete local WebRTC offer"},
        {"Le serveur Luna ne répond pas (délai dépassé)", "The Luna server is not responding (timeout)"},
        {"Luna a refusé la session : {}", "Luna refused the session: {}"},
        {"Session Amazon expirée : reconnecte-toi. ({})", "Amazon session expired: please sign in again. ({})"},
        {"B : retour à la bibliothèque", "B: back to library"},
        {"ZL + ZR + Moins : quitter", "ZL + ZR + Minus: quit"},
        {"B : annuler", "B: cancel"},
        {"Région {} · décodeur {} · {}x{} · paquets {} · images {} · erreurs {} · manette {}",
         "Region {} · decoder {} · {}x{} · packets {} · frames {} · errors {} · controller {}"},
        {"prête", "ready"},
        {"en attente", "waiting"},
        {"non initialisé", "not initialized"},
        {"échec d'initialisation du décodeur", "decoder initialization failed"},
        {"NVDEC (matériel)", "NVDEC (hardware)"},
        {"logiciel (mode sans échec)", "software (safe mode)"},
        {"logiciel", "software"},

        // Catalogue et compte Luna
        {"À la une", "Featured"},
        {"Top du moment", "Top right now"},
        {"Avec les abonnements partenaires", "With partner subscriptions"},
        {"Accueil", "Home"},
        {"Soirée jeux", "Game night"},
        {"Ma bibliothèque", "My library"},
        {"Découvrir Luna", "Discover Luna"},
        {"Sélection du profil Luna…", "Selecting Luna profile…"},
        {"Connexion au compte Amazon…", "Signing in to Amazon…"},
        {"Renouvellement de la session Amazon…", "Renewing the Amazon session…"},
        {"Chargement de la configuration Luna…", "Loading Luna configuration…"},
        {"Session Amazon refusée (HTTP {})", "Amazon session refused (HTTP {})"},
        {"/token a répondu HTTP {}", "/token returned HTTP {}"},
        {"/token : réponse illisible", "/token: unreadable response"},
        {"Session Amazon expirée", "Amazon session expired"},
        {"{} : accès refusé (HTTP {})", "{}: access denied (HTTP {})"},
        {"{} a répondu HTTP {}", "{} returned HTTP {}"},
        {"{} : réponse JSON illisible", "{}: unreadable JSON response"},
        {"Impossible d'ouvrir la fiche de « {} »", "Unable to open the page for \"{}\""},
        {"Ce jeu ne peut pas être lancé avec ce compte (non inclus dans l'abonnement ?)",
         "This game cannot be launched with this account (not included in your subscription?)"},
        {"Impossible de préparer l'offre WebRTC", "Unable to prepare the WebRTC offer"},
        {"Impossible de préparer la requête de session", "Unable to prepare the session request"},
        {"Luna n'a pas créé de session de jeu", "Luna did not create a game session"},
        {"Enregistrement de l'appareil refusé : {}", "Device registration refused: {}"},
        {"Réponse d'enregistrement incomplète (aucun jeton)", "Incomplete registration response (no token)"},
        {"L'appareil a été déconnecté du compte Amazon", "The device was removed from the Amazon account"},
        {"Renouvellement de la session impossible : {}", "Unable to renew the session: {}"},
        {"Amazon n'a renvoyé aucun cookie de session", "Amazon returned no session cookie"},
        {"Le navigateur de la Switch n'est pas disponible en mode applet.\nLance LunaNX en maintenant R "
         "au démarrage d'un jeu, puis réessaie.",
         "The Switch browser is not available in applet mode.\nLaunch LunaNX by holding R while starting "
         "a game, then try again."},
        {"Impossible d'ouvrir le navigateur (code {})", "Unable to open the browser (code {})"},
        {"Le navigateur s'est fermé avec une erreur (code {})", "The browser closed with an error (code {})"},
        {"Connexion par navigateur disponible uniquement sur Switch", "Browser sign-in is only available on Switch"},
    };
    return texts;
}

} // namespace

void SetUiLanguage(const std::string& code)
{
    g_language = code == "fr" ? "fr" : "en";
}

const std::string& UiLanguage()
{
    return g_language;
}

std::string Tr(const std::string& french)
{
    if (g_language == "fr")
        return french;
    const auto& texts = EnglishTexts();
    const auto found = texts.find(french);
    return found == texts.end() ? french : found->second;
}

std::string TrFormat(const std::string& french, std::initializer_list<std::string> args)
{
    std::string text = Tr(french);
    size_t position = 0;
    for (const auto& arg : args)
    {
        position = text.find("{}", position);
        if (position == std::string::npos)
            break;
        text.replace(position, 2, arg);
        position += arg.size();
    }
    return text;
}

} // namespace lunanx
