#!/usr/bin/env python3
"""Local, read-only copy inspection and deterministic LunaNX build provenance.

Similarity is a review aid, not proof of authorship or an authenticity signature.
No inspected program is executed and nothing is uploaded.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys

SCHEMA = "lunanx-source-provenance/v1"
SOURCE_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".glsl"}
SKIP_DIRECTORIES = {".git", "build", "node_modules", "__pycache__", ".venv", "extern", "vendor"}
MAX_SOURCE_BYTES = 4 * 1024 * 1024
KEYWORDS = set("alignas alignof and asm auto bool break case catch char char8_t char16_t char32_t class "
               "concept const consteval constexpr constinit const_cast continue co_await co_return co_yield "
               "decltype default delete do double dynamic_cast else enum explicit export extern false float "
               "for friend goto if inline int long mutable namespace new noexcept not nullptr operator or "
               "private protected public register reinterpret_cast requires return short signed sizeof "
               "static static_assert static_cast struct switch template this thread_local throw true try "
               "typedef typeid typename union unsigned using virtual void volatile wchar_t while xor".split())
TOKEN = re.compile(
    r'(?P<raw>(?:u8|u|U|L)?R"(?P<delimiter>[^\s()\\]{0,16})\(.*?\)(?P=delimiter)")'
    r'|(?P<string>(?:u8|u|U|L)?"(?:\\.|[^"\\])*")'
    r"|(?P<char>(?:u8|u|U|L)?'(?:\\.|[^'\\])*')"
    r'|(?P<comment>//[^\n]*|/\*.*?\*/)'
    r'|(?P<identifier>[A-Za-z_]\w*)'
    r'|(?P<number>0[xX][0-9a-fA-F]+[uUlL]*|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?[fFuUlL]*)'
    r'|(?P<operator>::|->\*|->|<<=?|>>=?|==|!=|<=|>=|&&|\|\||\+\+|--|[{}()[\];,.?:~!%^&*+=|/<>-])',
    re.DOTALL,
)
UNIT_PATTERN = re.compile(
    rb"LunaNX-Origin/v1\|project=([^|\x00]+)\|creator=([^|\x00]+)\|unit=([0-9a-f]{20})\x00"
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def unit_id(identity: dict, relative: str) -> str:
    return sha256((identity["project_id"] + "\0" + relative).encode())[:20]


def source_paths(root: Path):
    for directory, dirs, names in os.walk(root, followlinks=False):
        dirs[:] = sorted(d for d in dirs if d not in SKIP_DIRECTORIES and not (Path(directory) / d).is_symlink())
        for name in sorted(names):
            path = Path(directory) / name
            if path.suffix in SOURCE_EXTENSIONS and not path.is_symlink() and path.stat().st_size <= MAX_SOURCE_BYTES:
                yield path


def normalized_tokens(text: str) -> list[str]:
    # Attribution and includes are excluded so removal of the tags does not defeat comparison.
    text = re.sub(r"(?m)^[ \t]*LUNANX_SOURCE_ORIGIN\([^\n]*\)[ \t]*;?[ \t]*$", "", text)
    lines, continuation = [], False
    for line in text.splitlines():
        if continuation or line.lstrip().startswith("#"):
            continuation = line.rstrip().endswith("\\")
            continue
        lines.append(line)
    result = []
    for match in TOKEN.finditer("\n".join(lines)):
        kind, value = match.lastgroup, match.group()
        if kind == "comment":
            continue
        result.append("ID" if kind == "identifier" and value not in KEYWORDS else value)
    return result


def fingerprints(tokens: list[str], k: int = 12, window: int = 5) -> set[str]:
    """Rightmost-minimum winnowing of token k-grams; independent of line layout."""
    if len(tokens) < k:
        return set()
    minima, selected = collections.deque(), set()
    count = len(tokens) - k + 1
    window = min(window, count)
    for index in range(count):
        digest = hashlib.blake2b("\x1f".join(tokens[index:index + k]).encode(), digest_size=8).hexdigest()
        while minima and minima[-1][1] >= digest:
            minima.pop()
        minima.append((index, digest))
        while minima[0][0] <= index - window:
            minima.popleft()
        if index >= window - 1:
            selected.add(minima[0][1])
    return selected


def encode_manifest(manifest: dict) -> bytes:
    return (json.dumps(manifest, indent=2, ensure_ascii=True) + "\n").encode()


def create_manifest(root: Path) -> dict:
    identity = json.loads((root / "provenance/project.json").read_text())
    version_match = re.search(r'kAppVersion\s*=\s*"([^"]+)"', (root / "app/src/core/app_version.hpp").read_text())
    if not version_match:
        raise ValueError("Application version is missing")
    files = []
    paths = list(source_paths(root / "app"))
    paths += [root / name for name in ("CMakeLists.txt", "build.sh", "scripts/provenance.py",
                                      "provenance/project.json", "NOTICE", "LICENSE", "THIRD_PARTY_NOTICES.md")]
    paths += [root / name for name in ("resources/branding/LunaNX-V3.png", "resources/branding/Logo-Externe.png",
                                      "resources/img/lunanx_logo.png", "resources/img/lunanx_header.png",
                                      "resources/img/lunanx_cover.png", "resources/icon/icon.jpg")
              if (root / name).is_file()]
    for path in sorted(paths, key=lambda p: p.relative_to(root).as_posix()):
        relative = path.relative_to(root).as_posix()
        data = path.read_bytes()
        code = path.suffix in SOURCE_EXTENSIONS
        tokens = normalized_tokens(data.decode("utf-8")) if code else []
        entry = {"path": relative, "sha256": sha256(data), "bytes": len(data)}
        if code:
            entry.update({"unit": unit_id(identity, relative), "token_count": len(tokens),
                          "normalized_sha256": sha256("\x1f".join(tokens).encode()),
                          "fingerprints": sorted(fingerprints(tokens))[:512]})
        files.append(entry)
    tree = "".join(entry["path"] + "\0" + entry["sha256"] + "\n" for entry in files)
    return {"schema": SCHEMA, "identity": identity, "version": version_match[1],
            "source_tree_sha256": sha256(tree.encode()),
            "fingerprint_method": {"kind": "identifier-normalized-token-winnowing", "k": 12, "window": 5,
                                   "maximum_per_file": 512, "signature": False}, "files": files}


def write_if_changed(path: Path, data: bytes):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def generate(root: Path, manifest_path: Path, header: Path | None = None, symbols: Path | None = None) -> dict:
    manifest = create_manifest(root)
    encoded = encode_manifest(manifest)
    write_if_changed(manifest_path, encoded)
    if header:
        identity = manifest["identity"]
        fields = {"kProjectId": identity["project_id"], "kCreator": identity["creator"],
                  "kRepository": identity["repository"], "kSourceTreeSha256": manifest["source_tree_sha256"],
                  "kManifestSha256": sha256(encoded)}
        code = "// Generated by scripts/provenance.py; deterministic public provenance, no secrets.\n#pragma once\n"
        code += "namespace lunanx::provenance {\n"
        code += "".join("inline constexpr char " + name + "[] = " + json.dumps(value) + ";\n"
                        for name, value in fields.items())
        code += "}\n"
        write_if_changed(header, code.encode())
    if symbols:
        arguments = "".join("--undefined=lunanx_origin_u" + entry["unit"] + "\n"
                            for entry in manifest["files"] if entry["path"].endswith(".cpp"))
        write_if_changed(symbols, arguments.encode())
    return manifest


def inspect_binary(path: Path, reference: dict | None = None) -> dict:
    if path.stat().st_size > 256 * 1024 * 1024:
        raise ValueError("Binary exceeds the 256 MiB inspection limit")
    data = path.read_bytes()
    units = sorted({(p.decode(), c.decode(), u.decode()) for p, c, u in UNIT_PATTERN.findall(data)})
    report = {"file": str(path), "sha256": sha256(data), "origin_marker_count": len(units),
              "origins": sorted({creator for _, creator, _ in units}),
              "project_ids": sorted({project for project, _, _ in units}),
              "units": [unit for _, _, unit in units], "nacp_author": None, "nacp_version": None,
              "embedded_manifest": None}
    if data[0x10:0x14] == b"NRO0":
        size = struct.unpack_from("<I", data, 0x18)[0]
        if data[size:size + 4] != b"ASET" or size + 56 > len(data):
            raise ValueError("NRO asset header is missing or truncated")
        offset, length = struct.unpack_from("<QQ", data, size + 24)
        if length < 0x3070 or size + offset + length > len(data):
            raise ValueError("NRO NACP is missing or truncated")
        nacp = data[size + offset:size + offset + length]
        report["nacp_author"] = nacp[0x200:0x300].split(b"\0")[0].decode("utf-8", "replace")
        report["nacp_version"] = nacp[0x3060:0x3070].split(b"\0")[0].decode("utf-8", "replace")
    marker = ('{\n  "schema": "' + SCHEMA + '",\n').encode()
    position = data.find(marker)
    if position >= 0:
        text = data[position:position + 8 * 1024 * 1024].decode("utf-8", "replace")
        manifest, end = json.JSONDecoder().raw_decode(text)
        encoded = (text[:end] + "\n").encode()
        report["embedded_manifest"] = {"version": manifest["version"], "identity": manifest["identity"],
                                       "source_tree_sha256": manifest["source_tree_sha256"],
                                       "sha256": sha256(encoded), "file_count": len(manifest["files"])}
        report["embedded_digest_present_in_code"] = sha256(encoded).encode() in data
    if reference:
        expected = {e["unit"] for e in reference["files"] if e["path"].endswith(".cpp")}
        present = {u for p, c, u in units if p == reference["identity"]["project_id"] and c == reference["identity"]["creator"]}
        report["missing_compiled_units"] = sorted(expected - present)
        report["reference_manifest_sha256"] = sha256(encode_manifest(reference))
        report["manifest_matches_reference"] = bool(report["embedded_manifest"] and
            report["embedded_manifest"]["sha256"] == report["reference_manifest_sha256"])
    return report


def compare(target: Path, manifest: dict) -> dict:
    references = [entry for entry in manifest["files"] if entry.get("token_count", 0) >= 20]
    frequencies = collections.Counter(fp for entry in references for fp in entry["fingerprints"])
    rare_limit = max(1, len(references) // 20)
    ref_sets = {e["path"]: {fp for fp in e["fingerprints"] if frequencies[fp] <= rare_limit} for e in references}
    findings, scanned = [], 0
    paths = [target] if target.is_file() else source_paths(target)
    for path in paths:
        if path.is_symlink() or path.stat().st_size > MAX_SOURCE_BYTES:
            continue
        tokens = normalized_tokens(path.read_text(encoding="utf-8", errors="replace"))
        scanned += 1
        if len(tokens) < 20:
            continue
        normalized = sha256("\x1f".join(tokens).encode())
        candidate = fingerprints(tokens)
        for entry in references:
            shared = len(candidate & ref_sets[entry["path"]])
            exact = normalized == entry["normalized_sha256"]
            if exact or shared >= 6:
                findings.append({"candidate": str(path), "reference": entry["path"],
                                 "match": "normalized-identical" if exact else "possible-partial-copy",
                                 "shared_uncommon_fingerprints": shared,
                                 "reference_coverage": round(shared / max(1, len(ref_sets[entry["path"]])), 3)})
    findings.sort(key=lambda f: (f["match"] != "normalized-identical", -f["shared_uncommon_fingerprints"]))
    return {"schema": "lunanx-copy-comparison/v1", "creator": manifest["identity"]["creator"],
            "project_id": manifest["identity"]["project_id"], "files_scanned": scanned,
            "findings": findings, "notice": "Similarity leads for human review; not proof of authorship or a signature."}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    default_root = Path(__file__).resolve().parent.parent
    for name in ("generate", "inspect", "compare", "verify", "record-release"):
        command = sub.add_parser(name)
        command.add_argument("--root", type=Path, default=default_root)
        command.add_argument("--manifest", type=Path)
        if name in {"verify", "record-release"}:
            command.add_argument("--release", type=Path)
        if name == "generate":
            command.add_argument("--header", type=Path)
            command.add_argument("--symbols", type=Path)
        else:
            command.add_argument("target", type=Path)
    args = parser.parse_args()
    path = args.manifest or args.root / "provenance/source-manifest.json"
    if args.command == "generate":
        manifest = generate(args.root, path, args.header, args.symbols)
        print(json.dumps({"manifest": str(path), "version": manifest["version"],
                          "source_tree_sha256": manifest["source_tree_sha256"]}, indent=2))
        return 0
    manifest = json.loads(path.read_text())
    if manifest.get("schema") != SCHEMA:
        raise ValueError("Unsupported provenance manifest schema")
    if not args.target.exists():
        raise ValueError("Target does not exist: " + str(args.target))
    if args.command == "compare":
        report = compare(args.target, manifest)
    else:
        report = inspect_binary(args.target, manifest)
    if args.command in {"verify", "record-release"}:
        report["source_matches_reference"] = encode_manifest(create_manifest(args.root)) == encode_manifest(manifest)
        report["provenance_consistent"] = bool(report["source_matches_reference"] and report["manifest_matches_reference"] and
            not report["missing_compiled_units"] and report.get("embedded_digest_present_in_code") and
            report["nacp_author"] == manifest["identity"]["creator"] and report["nacp_version"] == manifest["version"])
        release_path = args.release or args.root / "provenance/releases" / (manifest["version"] + ".json")
        if args.command == "record-release":
            if not report["provenance_consistent"]:
                raise ValueError("Cannot record release: source/binary provenance does not match")
            release = {"schema": "lunanx-release-reference/v1", "identity": manifest["identity"],
                       "version": manifest["version"], "nro_sha256": report["sha256"],
                       "source_tree_sha256": manifest["source_tree_sha256"],
                       "manifest_sha256": sha256(encode_manifest(manifest)), "digitally_signed": False}
            write_if_changed(release_path, (json.dumps(release, indent=2) + "\n").encode())
            print(json.dumps({"release_reference": str(release_path), **release}, indent=2))
            return 0
        release = json.loads(release_path.read_text())
        report["release_hash_matches"] = (release.get("schema") == "lunanx-release-reference/v1" and
            release.get("nro_sha256") == report["sha256"] and release.get("identity") == manifest["identity"] and
            release.get("version") == manifest["version"] and
            release.get("manifest_sha256") == report["reference_manifest_sha256"] and
            release.get("source_tree_sha256") == manifest["source_tree_sha256"])
        report["reference_digitally_signed"] = False
        report["verified"] = report["provenance_consistent"] and report["release_hash_matches"]
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0 if args.command != "verify" or report["verified"] else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, struct.error) as error:
        print("provenance: " + str(error), file=sys.stderr)
        sys.exit(2)
