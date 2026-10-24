# Post-build packaging for Meshnology W13 (nRF52840 + Adafruit Feather UF2 bootloader).
#
# Outputs in $BUILD_DIR:
#   firmware.uf2
#   meshnology-w13.uf2        (LCD line)  or  meshnology-w13-eink.uf2
#   meshnology-w13.dfu        (LCD line)  or  meshnology-w13-eink.dfu
#     (.dfu is an adafruit-nrfutil DFU zip, renamed for MeshCore web / tooling)
#
# SoftDevice requirement: S140 7.3.0 (FWID 0x0123), matching boards/meshnology_w13.json.
# UF2 family: 0x239A0029 (Adafruit Feather nRF52840), NOT the generic 0xADA52840.
Import("env")

import os
import shutil
import subprocess
import sys
from os.path import isfile, join


W13_UF2_FAMILY = "0x239A0029"
W13_SD_REQ = "0x0123"


def _board_slug(pioenv):
    """Map PIOENV to product file stem."""
    if pioenv.startswith("meshnology_w13_eink"):
        return "meshnology-w13-eink"
    if pioenv.startswith("meshnology_w13"):
        return "meshnology-w13"
    return pioenv.lower().replace("_", "-")


def _nrfutil_cmd():
    pkg = env.subst("$PROJECT_PACKAGES_DIR")
    candidates = [
        join(pkg, "tool-adafruit-nrfutil", "adafruit-nrfutil.py"),
        join(pkg, "framework-arduinoadafruitnrf52", "tools", "adafruit-nrfutil", "adafruit-nrfutil.py"),
    ]
    for c in candidates:
        if isfile(c):
            return [env.subst("$PYTHONEXE"), c]
    return ["adafruit-nrfutil"]


def gen_w13_pkgs(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    progname = env.subst("${PROGNAME}")
    pioenv = env.subst("$PIOENV")
    slug = _board_slug(pioenv)
    hex_path = join(build_dir, progname + ".hex")
    if not isfile(hex_path):
        print("gen_w13_pkgs: skip, missing", hex_path)
        return

    uf2_path = join(build_dir, progname + ".uf2")
    zip_tmp = join(build_dir, progname + ".zip")
    uf2conv = join(project_dir, "bin", "uf2conv", "uf2conv.py")

    uf2_cmd = [
        env.subst("$PYTHONEXE"),
        uf2conv,
        "-f",
        W13_UF2_FAMILY,
        "-c",
        hex_path,
        "-o",
        uf2_path,
    ]
    print("gen_w13_pkgs: UF2", " ".join(uf2_cmd))
    r = subprocess.run(uf2_cmd, cwd=project_dir)
    if r.returncode != 0:
        sys.stderr.write("gen_w13_pkgs: uf2conv failed\n")
        env.Exit(r.returncode)

    named_uf2 = join(build_dir, slug + ".uf2")
    shutil.copy2(uf2_path, named_uf2)
    print("gen_w13_pkgs: wrote", uf2_path)
    print("gen_w13_pkgs: wrote", named_uf2)

    nrfutil = _nrfutil_cmd()
    genpkg_cmd = nrfutil + [
        "dfu",
        "genpkg",
        "--dev-type",
        "0x0052",
        "--dev-revision",
        "0xADAF",
        "--application",
        hex_path,
        "--application-version",
        "0xFFFFFFFF",
        "--sd-req",
        W13_SD_REQ,
        zip_tmp,
    ]
    print("gen_w13_pkgs: DFU", " ".join(genpkg_cmd))
    env_py = os.environ.copy()
    tool_root = join(env.subst("$PROJECT_PACKAGES_DIR"), "tool-adafruit-nrfutil")
    site = join(tool_root, "site-packages")
    if os.path.isdir(site):
        env_py["PYTHONPATH"] = site + os.pathsep + env_py.get("PYTHONPATH", "")

    r = subprocess.run(genpkg_cmd, cwd=project_dir, env=env_py)
    if r.returncode != 0:
        sys.stderr.write(
            "gen_w13_pkgs: adafruit-nrfutil failed (UF2 was still generated)\n"
        )
        return

    dfu_path = join(build_dir, slug + ".dfu")
    if isfile(dfu_path):
        os.remove(dfu_path)
    shutil.move(zip_tmp, dfu_path)
    print("gen_w13_pkgs: wrote", dfu_path)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", gen_w13_pkgs)
