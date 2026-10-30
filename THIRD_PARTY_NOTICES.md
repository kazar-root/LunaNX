# Third-party notices

`LunaNX.nro` contains code from the following projects. Each component remains under its own
license and copyright notices; the full license texts are in the [licenses/](licenses/) folder.

| Component | Use in LunaNX | License | License text |
| --- | --- | --- | --- |
| FFmpeg (with averne's NVTEGRA branch) | H.264 decoding (built with `--enable-gpl`) | GPL-2.0-or-later | [FFmpeg-GPL-2.0.txt](licenses/FFmpeg-GPL-2.0.txt), [FFmpeg-LICENSE.md](licenses/FFmpeg-LICENSE.md) |
| Moonlight-Switch (XITRIX) | Basis of the video decoder, renderer and frame queue | GPL-3.0 | [Moonlight-Switch-GPL-3.0.txt](licenses/Moonlight-Switch-GPL-3.0.txt) |
| OpenNOW-Switch (OpenCloudGaming) | Switch adaptations of libpeer, audio pipeline, decode queue policy | MIT | [OpenNOW-Switch-MIT.txt](licenses/OpenNOW-Switch-MIT.txt) |
| Borealis | User interface | Apache-2.0 | [Borealis-Apache-2.0.txt](licenses/Borealis-Apache-2.0.txt), [Borealis-NOTICE.txt](licenses/Borealis-NOTICE.txt) |
| libpeer (sepfy) | WebRTC stack | MIT | [libpeer-MIT.txt](licenses/libpeer-MIT.txt) |
| Mbed TLS | TLS / DTLS | Apache-2.0 | [MbedTLS-Apache-2.0.txt](licenses/MbedTLS-Apache-2.0.txt) |
| libsrtp (Cisco) | SRTP | BSD-3-Clause | [libsrtp-BSD-3-Clause.txt](licenses/libsrtp-BSD-3-Clause.txt) |
| usrsctp | SCTP data channels | BSD-3-Clause | [usrsctp-BSD-3-Clause.txt](licenses/usrsctp-BSD-3-Clause.txt) |
| cJSON | JSON (libpeer) | MIT | [cJSON-MIT.txt](licenses/cJSON-MIT.txt) |
| coreHTTP, coreMQTT | libpeer dependencies | MIT | [coreHTTP-MIT.txt](licenses/coreHTTP-MIT.txt), [coreMQTT-MIT.txt](licenses/coreMQTT-MIT.txt) |
| Yoga | UI layout (Borealis) | MIT | [Yoga-MIT.txt](licenses/Yoga-MIT.txt) |
| NanoVG | Vector drawing (Borealis) | zlib | [NanoVG-zlib.txt](licenses/NanoVG-zlib.txt) |
| fmt | Text formatting (Borealis) | MIT | [fmt-MIT.txt](licenses/fmt-MIT.txt) |
| Tweeny | Animations (Borealis) | MIT | [Tweeny-MIT.txt](licenses/Tweeny-MIT.txt) |
| tinyxml2 | XML (Borealis) | zlib | [tinyxml2-zlib.txt](licenses/tinyxml2-zlib.txt) |
| libromfs | Embedded resources (Borealis) | MIT | [libromfs-MIT.txt](licenses/libromfs-MIT.txt) |
| switch-libpulsar | Audio (Borealis) | MIT | [switch-libpulsar-MIT.txt](licenses/switch-libpulsar-MIT.txt) |
| libnx | Nintendo Switch homebrew library | ISC | [libnx-ISC.txt](licenses/libnx-ISC.txt) |
| deko3d | Graphics API | zlib | [deko3d-zlib.txt](licenses/deko3d-zlib.txt) |
| curl | HTTPS requests | curl license | [curl.txt](licenses/curl.txt) |
| Jansson | JSON | MIT | [Jansson-MIT.txt](licenses/Jansson-MIT.txt) |
| zlib | Compression | zlib | [zlib.txt](licenses/zlib.txt) |
| bzip2 | Compression | bzip2 license | [bzip2.txt](licenses/bzip2.txt) |
| Material Icons (Google) | Icons | Apache-2.0 | [MaterialIcons-Apache-2.0.txt](licenses/MaterialIcons-Apache-2.0.txt) |

Because the binary includes GPL-licensed code (FFmpeg built with `--enable-gpl` and code derived
from Moonlight-Switch), LunaNX as a whole is distributed under the GNU GPL v3.0 (see
[LICENSE](LICENSE)), and its complete corresponding source code must be made available to
everyone who receives the binary.
