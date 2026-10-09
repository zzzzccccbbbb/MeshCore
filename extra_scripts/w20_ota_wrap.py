"""Post-build: prepend W20 HTTP OTA magic byte -> w20-firmware-ota.bin"""

Import("env")

W20_OTA_MAGIC = 0x57  # 'W', must match W20_WIFI_OTA_FILE_MAGIC in w20_wifi_ota.h


def wrap_ota_firmware(source, target, env):
    import os

    build_dir = env.subst("$BUILD_DIR")
    src = os.path.join(build_dir, "firmware.bin")
    dst = os.path.join(build_dir, "w20-firmware-ota.bin")
    if not os.path.isfile(src):
        return
    with open(src, "rb") as f:
        payload = f.read()
    with open(dst, "wb") as f:
        f.write(bytes([W20_OTA_MAGIC]))
        f.write(payload)
    print("W20 OTA: wrote %s (%u bytes, magic 0x%02x)" % (dst, len(payload) + 1, W20_OTA_MAGIC))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", wrap_ota_firmware)
