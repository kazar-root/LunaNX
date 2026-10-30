#include "luna_sdp.hpp"

#include <cstdio>
#include <random>
#include <sstream>

namespace lunanx::luna
{
namespace
{

std::vector<std::string> SplitLines(const std::string& sdp)
{
    std::vector<std::string> lines;
    std::stringstream stream(sdp);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!line.empty())
            lines.push_back(line);
    }
    return lines;
}

bool StartsWith(const std::string& value, const char* prefix)
{
    return value.rfind(prefix, 0) == 0;
}

std::string Value(const std::string& line, const char* prefix)
{
    return line.substr(std::char_traits<char>::length(prefix));
}

unsigned long RandomSsrc()
{
    static std::mt19937 rng(std::random_device {}());
    return std::uniform_int_distribution<unsigned long>(1u, 0x7fffffffu)(rng);
}

void Append(std::string& sdp, const std::string& line)
{
    sdp += line;
    sdp += "\r\n";
}

void AppendTransport(std::string& sdp, const LocalTransport& t, bool with_candidates)
{
    Append(sdp, "a=ice-ufrag:" + t.ice_ufrag);
    Append(sdp, "a=ice-pwd:" + t.ice_pwd);
    Append(sdp, "a=ice-options:trickle");
    Append(sdp, "a=fingerprint:" + t.fingerprint);
    Append(sdp, "a=setup:actpass");
    if (with_candidates)
        for (const auto& candidate : t.candidates)
            Append(sdp, candidate);
}

} // namespace

LocalTransport ParseLocalTransport(const std::string& libpeer_sdp)
{
    LocalTransport transport;
    for (const auto& line : SplitLines(libpeer_sdp))
    {
        if (StartsWith(line, "a=ice-ufrag:"))
            transport.ice_ufrag = Value(line, "a=ice-ufrag:");
        else if (StartsWith(line, "a=ice-pwd:"))
            transport.ice_pwd = Value(line, "a=ice-pwd:");
        else if (StartsWith(line, "a=fingerprint:"))
            transport.fingerprint = Value(line, "a=fingerprint:");
        else if (StartsWith(line, "a=candidate:"))
            transport.candidates.push_back(line);
    }
    return transport;
}

std::string BuildLunaOffer(const LocalTransport& t)
{
    const unsigned long audio_ssrc = RandomSsrc();
    std::string sdp;
    char origin[96];
    std::snprintf(origin, sizeof(origin), "o=- %lu%06lu 2 IN IP4 127.0.0.1",
                  RandomSsrc(), RandomSsrc() % 1000000ul);
    Append(sdp, "v=0");
    Append(sdp, origin);
    Append(sdp, "s=-");
    Append(sdp, "t=0 0");
    Append(sdp, "a=extmap-allow-mixed");
    Append(sdp, "a=msid-semantic: WMS lunanx");
    Append(sdp, "a=group:BUNDLE 0 1 2");

    // Vidéo : réception seule, H.264 42e01f en mode de paquetisation 1.
    Append(sdp, "m=video 9 UDP/TLS/RTP/SAVPF 96 97");
    Append(sdp, "c=IN IP4 0.0.0.0");
    Append(sdp, "a=rtcp:9 IN IP4 0.0.0.0");
    AppendTransport(sdp, t, true);
    Append(sdp, "a=mid:0");
    Append(sdp, "a=extmap:1 urn:ietf:params:rtp-hdrext:toffset");
    Append(sdp, "a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time");
    Append(sdp, "a=extmap:5 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay");
    Append(sdp, "a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid");
    Append(sdp, "a=recvonly");
    Append(sdp, "a=rtcp-mux");
    Append(sdp, "a=rtcp-rsize");
    Append(sdp, "a=rtpmap:96 H264/90000");
    Append(sdp, "a=rtcp-fb:96 goog-remb");
    Append(sdp, "a=rtcp-fb:96 ccm fir");
    Append(sdp, "a=rtcp-fb:96 nack");
    Append(sdp, "a=rtcp-fb:96 nack pli");
    Append(sdp, "a=fmtp:96 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f");
    Append(sdp, "a=rtpmap:97 rtx/90000");
    Append(sdp, "a=fmtp:97 apt=96");

    // Audio : Opus stéréo. Le serveur ouvre la voie en sendrecv (micro) ; on n'envoie rien.
    Append(sdp, "m=audio 9 UDP/TLS/RTP/SAVPF 111");
    Append(sdp, "c=IN IP4 0.0.0.0");
    Append(sdp, "a=rtcp:9 IN IP4 0.0.0.0");
    AppendTransport(sdp, t, false);
    Append(sdp, "a=mid:1");
    Append(sdp, "a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time");
    Append(sdp, "a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid");
    Append(sdp, "a=sendrecv");
    Append(sdp, "a=msid:lunanx lunanx-audio");
    Append(sdp, "a=rtcp-mux");
    Append(sdp, "a=rtcp-rsize");
    Append(sdp, "a=rtpmap:111 opus/48000/2");
    Append(sdp, "a=rtcp-fb:111 transport-cc");
    Append(sdp, "a=fmtp:111 minptime=10;stereo=1;sprop-stereo=1;useinbandfec=1");
    Append(sdp, "a=ssrc:" + std::to_string(audio_ssrc) + " cname:lunanx");
    Append(sdp, "a=ssrc:" + std::to_string(audio_ssrc) + " msid:lunanx lunanx-audio");

    // Data channels (manette, affichage, contrôle).
    Append(sdp, "m=application 9 UDP/DTLS/SCTP webrtc-datachannel");
    Append(sdp, "c=IN IP4 0.0.0.0");
    AppendTransport(sdp, t, false);
    Append(sdp, "a=mid:2");
    Append(sdp, "a=sctp-port:5000");
    Append(sdp, "a=max-message-size:262144");
    return sdp;
}

std::string NormalizeLunaAnswer(const std::string& answer_sdp)
{
    std::string out;
    for (const auto& line : SplitLines(answer_sdp))
    {
        // libpeer ne sait pas dépaqueter RED/FEC ; on ne les a pas proposés,
        // mais on filtre par sécurité si le serveur les mentionne malgré tout.
        if (line.find(" red/") != std::string::npos ||
            line.find(" ulpfec/") != std::string::npos ||
            line.find(" flexfec") != std::string::npos)
            continue;
        Append(out, line);
    }
    return out;
}

int AnsweredH264PayloadType(const std::string& answer_sdp)
{
    for (const auto& line : SplitLines(answer_sdp))
    {
        int pt = -1;
        if (std::sscanf(line.c_str(), "a=rtpmap:%d H264/90000", &pt) == 1)
            return pt;
    }
    return -1;
}

} // namespace lunanx::luna
