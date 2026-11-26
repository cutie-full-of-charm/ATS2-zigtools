"""Regenerate the prepatched ATS2 .tgz for `zig build`."""
from __future__ import annotations

import gzip
import hashlib
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ORIGINAL = ROOT / "ATS2-Postiats-0.4.2.tgz"
OUTPUT = ROOT / "ATS2-Postiats-0.4.2-zigtools.tgz"
PATCHES = [
    "prelude-function-prototypes.patch",
    "windows-compat-1.patch",
    "windows-compat-2.patch",
    "stdlib-win-strerror-r.patch",
    *(f"windows-compat-{number}.patch" for number in range(3, 10)),
    "windows-legacy-sockets.patch",
    "windows-absolute-paths.patch",
    "windows-relative-paths.patch",
    "prelude-bsearch-macro.patch",
]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="ats2-repack-") as tmpdir:
        root = Path(tmpdir)
        with tarfile.open(ORIGINAL, "r:gz") as src:
            src.extractall(root, filter="data")
        upstream_dirs = [path for path in root.iterdir() if path.is_dir()]
        if len(upstream_dirs) != 1:
            raise RuntimeError("Expected exactly one upstream ATS2 top-level directory")
        ats = upstream_dirs[0]

        for name in PATCHES:
            subprocess.run(
                [
                    "patch", "--batch", "--fuzz=0", "--no-backup-if-mismatch",
                    "-p1", "-d", str(ats), "-i", str(ROOT / "patches" / name),
                ],
                check=True,
            )

        symlinks = sorted(path for path in ats.rglob("*") if path.is_symlink())
        for path in symlinks:
            target = path.resolve()
            path.unlink()
            if not target.exists():
                print(f"warning: dangling link omitted: {path.relative_to(ats)}")
            elif not target.is_relative_to(ats):
                print(f"warning: external link omitted: {path.relative_to(ats)}")
            elif target.is_dir():
                shutil.copytree(target, path, symlinks=False)
            elif target.is_file():
                shutil.copyfile(target, path)

        with OUTPUT.open("wb") as output_file:
            with gzip.GzipFile(fileobj=output_file, mode="wb", filename="", mtime=0, compresslevel=9) as gz:
                with tarfile.open(fileobj=gz, mode="w", format=tarfile.GNU_FORMAT) as dst:
                    for path in sorted(ats.rglob("*")):
                        if path.is_symlink() or path.name.endswith((".orig", ".rej")):
                            continue
                        info = dst.gettarinfo(str(path), arcname=path.relative_to(root).as_posix())
                        info.mtime = 0
                        info.uid = info.gid = 0
                        info.uname = info.gname = ""
                        if info.isfile():
                            with path.open("rb") as stream:
                                dst.addfile(info, stream)
                        else:
                            dst.addfile(info)

    digest = hashlib.sha256(OUTPUT.read_bytes()).hexdigest()
    original_digest = hashlib.sha256(ORIGINAL.read_bytes()).hexdigest()
    (ROOT / "SHA256SUMS").write_text(f"{original_digest}  {ORIGINAL.name}\n{digest}  {OUTPUT.name}\n", encoding="ascii")
    print(f"{OUTPUT.name}: {OUTPUT.stat().st_size} bytes; sha256 {digest}")


if __name__ == "__main__":
    main()
