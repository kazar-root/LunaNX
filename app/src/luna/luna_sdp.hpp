#pragma once

#include <string>
#include <vector>

namespace lunanx::luna
{

// Identifiants ICE/DTLS extraits de l'offre générée par libpeer.
struct LocalTransport
{
    std::string ice_ufrag;
    std::string ice_pwd;
    std::string fingerprint;           // « sha-256 AA:BB:... »
    std::vector<std::string> candidates; // lignes « a=candidate:... »
};

LocalTransport ParseLocalTransport(const std::string& libpeer_sdp);

// Offre au format du client web Luna (capture Chrome) réduite à ce que la Switch décode :
// vidéo H.264 Constrained Baseline (PT 96, rtx 97) en réception, Opus stéréo, data channel SCTP.
std::string BuildLunaOffer(const LocalTransport& transport);

// Normalise la réponse du serveur pour libpeer : fins de ligne CRLF et retrait des
// codecs auxiliaires que la Switch ne traite pas (RED/ULPFEC/FlexFEC).
std::string NormalizeLunaAnswer(const std::string& answer_sdp);

// Payload type H.264 choisi par le serveur dans la réponse (‑1 si absent).
int AnsweredH264PayloadType(const std::string& answer_sdp);

} // namespace lunanx::luna
