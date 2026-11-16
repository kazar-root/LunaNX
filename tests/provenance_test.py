"""Offline regression tests: no user files, network, or execution of inspected code."""
import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
SCRIPT = Path(__file__).resolve().parents[1] / "scripts/provenance.py"
SPEC = importlib.util.spec_from_file_location("provenance", SCRIPT)
P = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(P)

FIRST = '''
unsigned MixPacket(const unsigned char* bytes, int length) {
    unsigned result = 2166136261u;
    for (int cursor = 0; cursor < length; ++cursor) {
        result ^= bytes[cursor];
        result *= 16777619u;
        if ((result & 7u) == 3u) result = (result << 5) | (result >> 27);
        else result += 137u;
    }
    return result ^ (result >> 16);
}
'''
SECOND = '''
int OtherFunction(int left, int right) {
    switch (left) {
        case 42: return right * 39;
        case 93: return right / 17;
        default: return (left % 43) - (right % 91);
    }
}
'''


class ProvenanceTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.identity = {"schema": "lunanx-origin/v1", "project": "LunaNX", "creator": "KazaR-RooT",
                         "project_id": "urn:uuid:test-project", "repository": "https://example.test/LunaNX",
                         "license": "GPL-3.0"}
        self.write("provenance/project.json", json.dumps(self.identity))
        self.write("app/src/core/app_version.hpp", 'constexpr auto kAppVersion = "9.8.7";')
        self.write("app/src/algorithm.cpp", FIRST + SECOND)
        for name in ("CMakeLists.txt", "build.sh", "scripts/provenance.py", "NOTICE", "LICENSE", "THIRD_PARTY_NOTICES.md"):
            self.write(name, "fixture: " + name)
        self.manifest_path = self.root / "provenance/source-manifest.json"
        self.manifest = P.generate(self.root, self.manifest_path, self.root / "generated.hpp")

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def fake_nro(self):
        marker = "LunaNX-Origin/v1|project=urn:uuid:test-project|creator=KazaR-RooT|unit="
        unit = next(e["unit"] for e in self.manifest["files"] if e["path"].endswith(".cpp"))
        core = bytearray(0x100)
        core[0x10:0x14] = b"NRO0"
        core.extend((marker + unit + "\0" + P.sha256(P.encode_manifest(self.manifest))).encode())
        struct.pack_into("<I", core, 0x18, len(core))
        nacp = bytearray(0x4000)
        nacp[0x200:0x200 + len(b"KazaR-RooT")] = b"KazaR-RooT"
        nacp[0x3060:0x3060 + len(b"9.8.7")] = b"9.8.7"
        assets = struct.pack("<4sIQQQQQQ", b"ASET", 0, 0, 0, 56, len(nacp), 0, 0)
        target = self.root / "release.nro"
        target.write_bytes(core + assets + nacp + P.encode_manifest(self.manifest))
        return target

    def cli(self, *args):
        return subprocess.run([sys.executable, str(SCRIPT), *args, "--root", str(self.root)],
                              capture_output=True, text=True, check=False)

    def test_generation_is_deterministic_and_excludes_private_files(self):
        before = self.manifest_path.read_bytes()
        self.write("amazon_device.json", '"private-test-secret"')
        self.write("app/src/cookies.json", '"private-test-secret"')
        P.generate(self.root, self.manifest_path)
        self.assertEqual(before, self.manifest_path.read_bytes())
        self.assertNotIn(b"private-test-secret", before)
        self.assertNotIn(str(self.root).encode(), before)

    def test_branding_assets_are_included_in_the_release_fingerprint(self):
        before = self.manifest["source_tree_sha256"]
        asset = self.root / "resources/img/lunanx_logo.png"
        asset.parent.mkdir(parents=True)
        asset.write_bytes(b"\x89PNG\x00\xffsynthetic-logo")
        manifest = P.create_manifest(self.root)
        self.assertNotEqual(before, manifest["source_tree_sha256"])
        entry = next(e for e in manifest["files"] if e["path"] == "resources/img/lunanx_logo.png")
        self.assertEqual(entry["sha256"], P.sha256(asset.read_bytes()))

    def test_comments_and_identifiers_do_not_hide_a_full_copy(self):
        changed = (FIRST + SECOND).replace("result", "state").replace("cursor", "position").replace("bytes", "payload")
        changed = "// removed original attribution\n#include <unrelated>\n" + changed.replace(";", ";\n")
        target = self.write("renamed/copied.cpp", changed)
        before = target.read_bytes()
        matches = P.compare(target, self.manifest)["findings"]
        self.assertTrue(any(m["match"] == "normalized-identical" and m["reference"] == "app/src/algorithm.cpp" for m in matches))
        self.assertEqual(before, target.read_bytes())

    def test_copied_function_is_detected_inside_unrelated_code(self):
        target = self.write("snippet.cpp", "class Wrapper {};\n" + FIRST.replace("result", "checksum") + "void Finish() {}")
        matches = P.compare(target, self.manifest)["findings"]
        self.assertTrue(any(m["match"] == "possible-partial-copy" and m["shared_uncommon_fingerprints"] >= 6 for m in matches))

    def test_unrelated_code_is_not_a_match(self):
        target = self.write("unrelated.cpp", 'class Window { public: virtual ~Window() = default; void Display() { Draw("Hello"); } };')
        self.assertFalse(P.compare(target, self.manifest)["findings"])

    def test_strings_keep_comment_like_characters(self):
        tokens = P.normalized_tokens('auto url = "https://example.test/a"; // comment\nauto x = R"tag(a // b /* c */)tag";')
        self.assertIn('"https://example.test/a"', tokens)
        self.assertIn('R"tag(a // b /* c */)tag"', tokens)
        self.assertNotIn("comment", tokens)

    def test_markers_are_excluded_from_function_fingerprints(self):
        tagged = '#include "core/source_origin.hpp"\nLUNANX_SOURCE_ORIGIN("0123456789abcdef0123")\n' + FIRST
        self.assertEqual(P.normalized_tokens(FIRST), P.normalized_tokens(tagged))

    def test_binary_inspection_reads_embedded_origin_and_manifest(self):
        report = P.inspect_binary(self.fake_nro(), self.manifest)
        self.assertEqual(report["nacp_author"], "KazaR-RooT")
        self.assertEqual(report["nacp_version"], "9.8.7")
        self.assertEqual(report["origin_marker_count"], 1)
        self.assertTrue(report["manifest_matches_reference"])
        self.assertTrue(report["embedded_digest_present_in_code"])
        self.assertFalse(report["missing_compiled_units"])

    def test_verify_uses_external_binary_hash_and_detects_tampering(self):
        target = self.fake_nro()
        self.assertEqual(self.cli("record-release", str(target)).returncode, 0)
        self.assertEqual(self.cli("verify", str(target)).returncode, 0)
        changed = bytearray(target.read_bytes())
        changed[0x80] ^= 1  # Code changes even while all public origin tags remain untouched.
        target.write_bytes(changed)
        result = self.cli("verify", str(target))
        self.assertEqual(result.returncode, 1, result.stderr)
        report = json.loads(result.stdout)
        self.assertTrue(report["provenance_consistent"])
        self.assertFalse(report["release_hash_matches"])

    def test_verify_detects_modified_source(self):
        target = self.fake_nro()
        self.assertEqual(self.cli("record-release", str(target)).returncode, 0)
        self.write("app/src/algorithm.cpp", FIRST.replace("137u", "139u") + SECOND)
        result = self.cli("verify", str(target))
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertFalse(json.loads(result.stdout)["source_matches_reference"])

    def test_missing_units_and_truncated_assets_are_reported(self):
        target = self.fake_nro()
        data = target.read_bytes().replace(b"LunaNX-Origin/v1", b"RemovedOrigin/v1")
        target.write_bytes(data)
        self.assertTrue(P.inspect_binary(target, self.manifest)["missing_compiled_units"])
        target.write_bytes(data[:0x100])
        with self.assertRaises((ValueError, struct.error)):
            P.inspect_binary(target, self.manifest)


if __name__ == "__main__":
    unittest.main()
