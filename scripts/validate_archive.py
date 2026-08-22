#!/usr/bin/env python3
"""Validate the exact runtime contents and BOF headers of an Armory archive."""

from __future__ import annotations

import argparse
import base64
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tarfile
import tempfile


REPO_URL = "https://github.com/sliverarmory/bof_collection"
PACKAGE_NAME = "beune-bof-collection"
COMMANDS = {
    "cve-2019-16098": ("CVE-2019-16098.x64.o", []),
    "beaconinfo": ("beaconinfo.x64.o", []),
    "bof-cat": ("cat.x64.o", [("file", "string")]),
    "disk_exists": (
        "disk_exists.x64.o",
        [("hostname", "string"), ("path", "string")],
    ),
    "hklm_exists": (
        "hklm_exists.x64.o",
        [("hostname", "string"), ("key", "string")],
    ),
    "patchlevel": ("patchlevel.x64.o", [("hostname", "string")]),
    "remotepipelist": ("remotepipelist.x64.o", [("hostname", "string")]),
}
EXPECTED_MEMBERS = {
    "./extension.json",
    "./LICENSE",
    "./NOTICE",
    *(f"./{artifact}" for artifact, _ in COMMANDS.values()),
}
TAG_RE = re.compile(r"^v[0-9]+\.[0-9]+\.[0-9]+$")
MAX_TRUSTED_COMMENT_BYTES = 4000


def fail(message: str) -> None:
    raise ValueError(message)


def inspect_object(data: bytes, name: str, objdump: str) -> None:
    with tempfile.TemporaryDirectory(prefix="armory-bof-inspect-") as directory:
        path = Path(directory) / name
        path.write_bytes(data)
        headers = subprocess.run(
            [objdump, "-f", str(path)],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        if "architecture: i386:x86-64" not in headers:
            fail(f"{name} is not an amd64 COFF object according to objdump")
        symbols = subprocess.run(
            [objdump, "-t", str(path)],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        entrypoint = re.search(
            r"^\[\s*\d+\]\(sec\s+([1-9][0-9]*)\).*?\(scl\s+2\).*?\sgo\s*$",
            symbols,
            re.MULTILINE,
        )
        if entrypoint is None:
            fail(f"{name} does not define the external BOF entrypoint symbol go")
        section = entrypoint.group(1)
        if re.search(
            rf"^\[\s*\d+\]\(sec\s+{re.escape(section)}\).*?\s\.text\s*$",
            symbols,
            re.MULTILINE,
        ) is None:
            fail(f"{name}: go is not defined in the .text code section")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--inspect-objects", action="store_true")
    args = parser.parse_args()

    if not TAG_RE.fullmatch(args.tag):
        parser.error("--tag must have the exact form vMAJOR.MINOR.PATCH")
    if not args.archive.is_file():
        parser.error("--archive must name a readable file")

    objdump = None
    if args.inspect_objects:
        objdump = shutil.which("x86_64-w64-mingw32-objdump") or shutil.which("llvm-objdump")
        if objdump is None:
            parser.error("--inspect-objects requires a COFF-capable objdump")

    with tarfile.open(args.archive, "r:gz") as tar:
        members = tar.getmembers()
        names = [member.name for member in members]
        if len(names) != len(set(names)):
            fail("archive contains duplicate paths")
        if set(names) != EXPECTED_MEMBERS:
            fail(
                "archive allowlist mismatch; expected "
                + repr(sorted(EXPECTED_MEMBERS))
                + ", got "
                + repr(sorted(names))
            )
        for member in members:
            pure = PurePosixPath(member.name)
            if not member.isfile() or member.name.startswith("/") or ".." in pure.parts:
                fail(f"unsafe or non-regular archive member: {member.name}")
            if member.size == 0:
                fail(f"empty archive member: {member.name}")
            if member.mode & 0o6000:
                fail(f"setuid/setgid archive member: {member.name}")

        manifest_member = tar.extractfile("./extension.json")
        if manifest_member is None:
            fail("cannot read ./extension.json")
        manifest_bytes = manifest_member.read()
        if len(base64.b64encode(manifest_bytes)) > MAX_TRUSTED_COMMENT_BYTES:
            fail("extension.json exceeds the safe Minisign trusted-comment limit")
        manifest = json.loads(manifest_bytes.decode("utf-8"))
        expected_top = {
            "name",
            "package_name",
            "version",
            "extension_author",
            "original_author",
            "repo_url",
            "commands",
        }
        if set(manifest) != expected_top:
            fail("extension.json has unexpected or missing top-level fields")
        if manifest["name"] != "beaconinfo":
            fail("manifest name must match an exposed command for Sliver update compatibility")
        if manifest["package_name"] != PACKAGE_NAME:
            fail("unexpected package_name")
        if manifest["version"] != args.tag:
            fail("manifest version does not match tag")
        if manifest["repo_url"] != REPO_URL:
            fail("unexpected repo_url")

        commands = manifest["commands"]
        if not isinstance(commands, list) or len(commands) != len(COMMANDS):
            fail("manifest must contain exactly seven commands")
        observed: set[str] = set()
        for command in commands:
            name = command.get("command_name")
            if name not in COMMANDS or name in observed:
                fail(f"unexpected or duplicate manifest command: {name!r}")
            observed.add(name)
            expected_artifact, expected_args = COMMANDS[name]
            if command.get("entrypoint") != "go":
                fail(f"{name}: entrypoint must be go")
            if command.get("depends_on") != "coff-loader":
                fail(f"{name}: depends_on must be coff-loader")
            if command.get("init") != "":
                fail(f"{name}: init must be empty")
            if not isinstance(command.get("help"), str) or not command["help"].strip():
                fail(f"{name}: help must be nonempty")
            files = command.get("files")
            if files != [
                {"os": "windows", "arch": "amd64", "path": expected_artifact}
            ]:
                fail(f"{name}: unexpected target or artifact mapping")
            arguments = command.get("arguments")
            if not isinstance(arguments, list) or [
                (argument.get("name"), argument.get("type")) for argument in arguments
            ] != expected_args:
                fail(f"{name}: unexpected argument order or type")
            for argument in arguments:
                if argument.get("optional") is not False:
                    fail(f"{name}: every declared argument must be required")
                if not isinstance(argument.get("desc"), str) or not argument["desc"].strip():
                    fail(f"{name}: every argument description must be nonempty")

            if objdump is not None:
                artifact_member = tar.extractfile(f"./{expected_artifact}")
                if artifact_member is None:
                    fail(f"cannot read {expected_artifact}")
                inspect_object(artifact_member.read(), expected_artifact, objdump)

    print(f"OK: {args.archive.name} is the expected seven-command package for {args.tag}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
