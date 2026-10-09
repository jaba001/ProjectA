"""Build/check Unreal Compact locres without starting the editor. 에디터 실행 없이 번역 리소스를 생성·검사합니다."""

import argparse
import collections
import json
import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CATALOGS = ROOT / "Config/Localization"
TARGET = ROOT / "Content/User_JeHoon/Localization/Game"
LITERAL = r'"((?:[^"\\]|\\.)*)"'
MACRO = re.compile(r"NSLOCTEXT\s*\(\s*" + LITERAL + r"\s*,\s*" + LITERAL + r"\s*,\s*" + LITERAL + r"\s*\)")


def unescape(value):
    return json.loads('"' + value + '"')


def gather():
    result = {}
    paths = list((ROOT / "Source/ProjectA").rglob("*.cpp")) + list((ROOT / "Source/ProjectA").rglob("*.h")) + list((ROOT / "Config").glob("*.ini"))
    for path in paths:
        for match in MACRO.finditer(path.read_text(encoding="utf-8-sig")):
            namespace, key, source = map(unescape, match.groups())
            identity = (namespace, key)
            if identity in result and result[identity]["source"] != source:
                raise ValueError(f"Conflicting source: {identity}")
            result[identity] = {"namespace": namespace, "key": key, "source": source, "path": path.relative_to(ROOT).as_posix()}
    return result


def load_entries():
    entries = {}
    gathered = gather()
    for path in sorted(CATALOGS.glob("*.json")):
        data = json.loads(path.read_text(encoding="utf-8-sig"))
        for entry in data["entries"]:
            identity = (entry["namespace"], entry["key"])
            if identity in entries:
                raise ValueError(f"Duplicate translation: {identity}")
            if not entry["source"] or not entry["translation"]:
                raise ValueError(f"Empty translation: {identity}")
            if identity in gathered and gathered[identity]["source"] != entry["source"]:
                raise ValueError(f"Stale translation: {identity}")
            # Preserve format arguments across languages. 언어가 바뀌어도 서식 인자를 보존합니다.
            placeholders = lambda text: set(re.findall(r"\{[^{}]+\}", text))
            if placeholders(entry["source"]) != placeholders(entry["translation"]):
                raise ValueError(f"Format argument mismatch: {identity}")
            entries[identity] = entry
    required = set(gathered)
    missing = sorted(required - entries.keys())
    if missing:
        raise ValueError(f"Missing keyed text translations: {missing}")
    return entries


def fstring(value):
    encoded = (value + "\0").encode("utf-16-le")
    return struct.pack("<i", -len(encoded) // 2) + encoded


def source_hash(value):
    # UE FCrc::StrCrc32 uses four bytes per TCHAR, including UTF-16 surrogate units on Windows.
    # UE FCrc::StrCrc32는 Windows UTF-16 서로게이트 단위를 포함해 TCHAR마다 4바이트를 사용합니다.
    units = struct.unpack("<" + "H" * (len(value.encode("utf-16-le")) // 2), value.encode("utf-16-le"))
    return zlib.crc32(b"".join(struct.pack("<I", unit) for unit in units))


def locres(entries, culture):
    namespaces = collections.defaultdict(list)
    strings = []
    for identity, entry in sorted(entries.items()):
        value = entry["source"] if culture == "ko" else entry["translation"]
        if value not in strings:
            strings.append(value)
        namespaces[identity[0]].append((identity[1], source_hash(entry["source"]), strings.index(value)))
    body = struct.pack("<I", len(namespaces))
    for namespace, values in namespaces.items():
        body += fstring(namespace) + struct.pack("<I", len(values))
        for key, checksum, index in values:
            body += fstring(key) + struct.pack("<Ii", checksum, index)
    # Compact version 1 is read by UE 5.8 FTextLocalizationResource::LoadFromArchive.
    # Compact 버전 1은 UE 5.8 FTextLocalizationResource::LoadFromArchive에서 지원합니다.
    header = struct.pack("<4IBq", 0x7574140E, 0xFC034A67, 0x9D90154A, 0x1B7F37C3, 1, 25 + len(body))
    return header + body + struct.pack("<i", len(strings)) + b"".join(map(fstring, strings))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--gather", type=Path)
    args = parser.parse_args()
    if args.gather:
        args.gather.parent.mkdir(parents=True, exist_ok=True)
        args.gather.write_text(json.dumps(list(gather().values()), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"Gathered {len(gather())} text identities")
        return
    entries = load_entries()
    meta = struct.pack("<4IB", 0xA14CEE4F, 0x83554868, 0xBD464C6C, 0x7C50DA70, 1) + fstring("ko") + fstring("ko/Game.locres") + struct.pack("<i", 2) + fstring("ko") + fstring("en")
    outputs = {TARGET / "Game.locmeta": meta, **{TARGET / culture / "Game.locres": locres(entries, culture) for culture in ("ko", "en")}}
    for path, payload in outputs.items():
        if args.check:
            if not path.exists() or path.read_bytes() != payload:
                raise ValueError(f"Generated localization is stale: {path}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)
    print(f"{'Validated' if args.check else 'Built'} {len(entries)} translations, ko/en resources and Korean native fallback")


if __name__ == "__main__":
    main()
