#!/usr/bin/env python3
"""Export kept XCTest screenshot attachments with Xcode 14 xcresulttool."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess


def value(node, default=""):
    return node.get("_value", default) if isinstance(node, dict) else default


def export_screenshots(result, destination):
    environment = dict(os.environ)
    environment.setdefault("DEVELOPER_DIR", "/Applications/Xcode.app/Contents/Developer")

    def xcresult(*arguments):
        return subprocess.check_output(
            ["xcrun", "xcresulttool", *arguments], env=environment, text=True
        )

    pending = [None]
    visited = set()
    payloads = set()
    screenshots = []
    destination.mkdir(parents=True, exist_ok=True)

    def walk(node):
        if isinstance(node, list):
            for child in node:
                walk(child)
            return
        if not isinstance(node, dict):
            return
        attachment_type = value(node.get("uniformTypeIdentifier"))
        payload = value(node.get("payloadRef", {}).get("id"))
        if payload and attachment_type in ("public.png", "public.jpeg") and payload not in payloads:
            payloads.add(payload)
            name = value(node.get("name"), "screenshot")
            safe_name = re.sub(r"[^\w.-]+", "-", name).strip("-.") or "screenshot"
            extension = ".png" if attachment_type == "public.png" else ".jpg"
            if not safe_name.lower().endswith(extension):
                safe_name += extension
            path = destination / ("%03d-%s" % (len(screenshots) + 1, safe_name))
            xcresult("export", "--path", str(result), "--id", payload,
                     "--type", "file", "--output-path", str(path))
            screenshots.append({"name": name, "file": path.name, "uti": attachment_type})
            print(path)
        for key, child in node.items():
            if key in ("testsRef", "summaryRef") and isinstance(child, dict):
                reference = value(child.get("id"))
                if reference and reference not in visited:
                    pending.append(reference)
            walk(child)

    while pending:
        reference = pending.pop(0)
        if reference in visited:
            continue
        visited.add(reference)
        arguments = ["get", "--path", str(result), "--format", "json"]
        if reference:
            arguments.extend(["--id", reference])
        walk(json.loads(xcresult(*arguments)))

    (destination / "manifest.json").write_text(
        json.dumps({"result": str(result), "screenshots": screenshots}, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    if not screenshots:
        raise SystemExit("No PNG/JPEG screenshot attachments were found; inspect the xcresult manually.")
    print("Exported %d screenshots." % len(screenshots))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("result", type=Path, help="Completed .xcresult bundle")
    parser.add_argument("destination", type=Path, help="Directory for exported images and manifest")
    args = parser.parse_args()
    export_screenshots(args.result.resolve(), args.destination.resolve())
