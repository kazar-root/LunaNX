#pragma once

#include <cstdint>
#include <vector>

namespace lunanx::input
{

// Messages du data channel « clientMessageChannel » (protobuf), relevés sur le client web.
// Le serveur n'envoie pas la vidéo tant qu'il n'a pas reçu la configuration d'affichage.
//
// Envoyés dès l'ouverture, dans cet ordre :
//   30 = ouverture       { 1: fonctionnalité (répété) }
//   40 = affichage       { 1: 1, 3: { 1: {1: largeur, 2: hauteur}, 2: i/s, 3: {} },
//                          4: { 2: 100, 3: 52, 5: 4 }, 5: fonctionnalité,
//                          6: débit min, 7: débit max, 8: débit initial (bit/s), 9: 1 }
//   42 = client          { 1: { 1: navigateur, 2: version, 3: modèle, 4: type, 5: OS, 6: version OS } }
// Puis chaque seconde :
//   20 = ping            { 1: horodatage ms } — le serveur répond { 1, 2, 3 }.
//
// Les réponses du serveur commencent par un en-tête de 16 octets (longueur du protobuf
// en petit-boutiste aux octets 8 à 11), suivi du message protobuf.
struct DisplayRequest
{
    int width = 1920;
    int height = 1080;
    int fps = 60;
    std::uint32_t min_bitrate = 6'000'000;
    std::uint32_t max_bitrate = 30'000'000;
    std::uint32_t start_bitrate = 15'000'000;
};

std::vector<std::uint8_t> EncodeSessionHello();
std::vector<std::uint8_t> EncodeDisplayRequest(const DisplayRequest& request);
std::vector<std::uint8_t> EncodeClientInfo();
std::vector<std::uint8_t> EncodePing(std::uint64_t now_ms);

// Numéro du champ protobuf principal d'un message reçu (0 si illisible).
std::uint32_t ServerMessageField(const std::uint8_t* data, std::size_t size);

// Vibration demandée par le jeu (inputChannel, champ 60 { 2: séquence, 3: moteur lourd,
// 4: moteur léger }, de 0 à 255). Un message sans moteur arrête la vibration.
struct RumbleCommand
{
    std::uint8_t heavy = 0;
    std::uint8_t light = 0;
};
bool ParseRumble(const std::uint8_t* data, std::size_t size, RumbleCommand& out);

} // namespace lunanx::input
