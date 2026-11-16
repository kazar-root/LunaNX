# LunaNX attribution and source comparison

Project creator: **KazaR-RooT**. Original repository:
[kazar-root/LunaNX](https://github.com/kazar-root/LunaNX).

Stable public project identifier: `urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed`.
This identifies the project, not a console, player or Amazon account.

## Included information

- `NOTICE` and `provenance/project.json` identify the project and creator.
- Application source files carry stable origin comments. C++ units also contain compiled
  origin strings that are retained during linker garbage collection.
- The NRO embeds `romfs:/provenance/source-manifest.json` and `NOTICE`. The manifest records
  source SHA-256 digests and normalized code fingerprints. Paths are relative.
- The original V3/external logos and their application/NRO variants are included in the manifest.
- **Settings → Creator and origin** displays only **KazaR-RooT**. Offline tools expose the
  detailed public information. This mechanism does not send telemetry or identify users.
- `provenance/source-manifest.json` describes the current source tree;
  `provenance/releases/3.0.json` records the complete NRO digest outside the binary.

Third-party authors and licenses remain listed in `THIRD_PARTY_NOTICES.md`. A match involving
a dependency does not attribute that dependency to KazaR-RooT.

## Verify the release

From the source directory, with the released NRO at `build/LunaNX.nro`:

```sh
python3 scripts/provenance.py verify build/LunaNX.nro
```

`"verified": true` means that the current sources, embedded manifest, compiled origin markers,
NACP creator/version and complete NRO digest match the local references. Use reference files
from the original repository or your own trusted copy.

Inspect an NRO without running it:

```sh
python3 scripts/provenance.py inspect /path/to/copy.nro > nro-report.json
```

`inspect` reports available attribution and metadata; `verify` checks integrity against the
external release reference.

## Compare source copies

```sh
python3 scripts/provenance.py compare /path/to/another-project > source-report.json
python3 scripts/provenance.py compare /path/to/renamed-file.cpp
```

Comparison ignores origin tags, comments, includes and whitespace. It normalizes identifiers
and compares token-sequence fingerprints, so a renamed file or copied function can remain
recognizable after visible notices are removed.

`normalized-identical` indicates equivalence under that normalization. `possible-partial-copy`
flags shared uncommon token sequences for human review. These are similarity indicators,
not independent proof of authorship. Common structures can resemble each other, and substantial
rewrites can evade comparison. Files shorter than 20 tokens do not receive a similarity conclusion.

The tool does not execute inspected code, alter copies or contact a server. Dependency/build
directories, symlinks and source files larger than 4 MiB are excluded.

## Regenerate references after changing the source

```sh
bash build.sh
python3 scripts/provenance.py generate --manifest provenance/source-manifest.json
python3 scripts/provenance.py record-release build/LunaNX.nro
python3 scripts/provenance.py verify build/LunaNX.nro
```

The build generates its own files in `build/provenance/`. Keep the exact source, NRO, manifest
and external release reference together. Manifest generation is deterministic and contains no
personal directory path or timestamp.

These public markers are editable. They are not digital signatures or certified timestamps,
do not prevent forks or modifications, and do not add restrictions to the GPL license.
