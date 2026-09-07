#!/usr/bin/env python3
"""Turns an image file into one of src/embeds/**'s byte-array headers.

Every icon and banner this app draws is compiled in as a `constexpr std::array<u8,
N>` that CAssetManager decodes with stb_image at startup (see src/asset_manager.cpp) -
there is no runtime asset directory to ship alongside the exe. This script writes one of
those headers in exactly the format the existing ones use, so adding an icon is:

    python tools/embed_image.py path/to/github.png src/embeds/icons/GitHubIcon.hpp github_icon

then one `#include` plus one row in CAssetManager::Load's own entry table.

Any format stb_image can decode works (PNG and JPEG are what this project uses); the bytes
are copied through verbatim, so whatever the file already is, is what gets embedded.
"""

import argparse
import pathlib
import sys

BYTES_PER_LINE = 16
NAMESPACE = "pulsar::embed::icon"

def emit(data: bytes, symbol: str, namespace: str) -> str:
    lines = [
        "#pragma once",
        "#include <array>",
        f"namespace {namespace} {{",
        f"constexpr std::array<u8, {len(data)}> {symbol} = {{",
    ]
    for start in range(0, len(data), BYTES_PER_LINE):
        chunk = data[start : start + BYTES_PER_LINE]
        lines.append("    " + " ".join(f"0x{byte:02X}," for byte in chunk))
    lines.append("};")
    lines.append(f"}} // namespace {namespace}")
    return "\n".join(lines) + "\n"

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", type=pathlib.Path, help="image file to embed (png/jpeg/...)")
    parser.add_argument("output", type=pathlib.Path, help="header to write, e.g. src/embeds/icons/GitHubIcon.hpp")
    parser.add_argument("symbol", help="C++ constant name, e.g. github_icon")
    parser.add_argument(
        "--namespace",
        default=NAMESPACE,
        help=f"namespace to declare it in (default: {NAMESPACE}; banners use pulsar::embed::banner)",
    )
    args = parser.parse_args()

    if not args.source.is_file():
        print(f"no such file: {args.source}", file=sys.stderr)
        return 1

    data = args.source.read_bytes()
    if not data:
        print(f"{args.source} is empty", file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(emit(data, args.symbol, args.namespace), encoding="utf-8", newline="\n")
    print(f"wrote {args.output} ({len(data)} bytes as {args.namespace}::{args.symbol})")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
