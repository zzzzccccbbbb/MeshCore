"""Re-apply W20 LR2021 findChip patch after PlatformIO refreshes RadioLib."""
Import("env")
from pathlib import Path

PATCH_MARK = "accept any non-dead SPI response"
OLD = '''bool LR2021::findChip(void) {
  // this is the only version mentioned in datasheet
  const uint8_t expMajor = 0x01;
  const uint8_t expMinor = 0x18;

  uint8_t i = 0;
  bool flagFound = false;
  uint8_t fwMajor = 0, fwMinor = 0;
  while((i < 10) && !flagFound) {
    // reset the module
    reset();

    // read the version
    int16_t state = getVersion(&fwMajor, &fwMinor);
    RADIOLIB_ASSERT(state);

    if((fwMajor == expMajor) && (fwMinor == expMinor)) {
      RADIOLIB_DEBUG_BASIC_PRINTLN("Found LR2021");
      RADIOLIB_DEBUG_BASIC_PRINTLN("Base FW version: %d.%d", (int)fwMajor, (int)fwMinor);
      flagFound = true;
    } else {
      RADIOLIB_DEBUG_BASIC_PRINTLN("LR2021 not found! (%d of 10 tries) FW version: = %d.%d", (int)i, (int)fwMajor, (int)fwMinor);
      RADIOLIB_DEBUG_BASIC_PRINTLN("Expected: %d.%d", (int)expMajor, (int)expMinor);
      this->mod->hal->delay(10);
      i++;
    }
  }

  return(flagFound);
}'''

NEW = '''bool LR2021::findChip(void) {
  // Datasheet lists 1.24; accept any non-dead SPI response (0x00/0xFF = bus fail).
  // NOTE: do NOT RADIOLIB_ASSERT(state) here — returning int from bool turns errors into true.
  uint8_t i = 0;
  bool flagFound = false;
  uint8_t fwMajor = 0, fwMinor = 0;
  while((i < 10) && !flagFound) {
    reset();

    int16_t state = getVersion(&fwMajor, &fwMinor);
    if(state != RADIOLIB_ERR_NONE) {
      RADIOLIB_DEBUG_BASIC_PRINTLN("LR2021 getVersion err=%d try %d/10", (int)state, (int)i);
      this->mod->hal->delay(10);
      i++;
      continue;
    }

    if((fwMajor != 0x00) && (fwMajor != 0xFF)) {
      RADIOLIB_DEBUG_BASIC_PRINTLN("Found LR2021");
      RADIOLIB_DEBUG_BASIC_PRINTLN("Base FW version: %d.%d", (int)fwMajor, (int)fwMinor);
      flagFound = true;
    } else {
      RADIOLIB_DEBUG_BASIC_PRINTLN("LR2021 not found! (%d of 10 tries) FW version: = %d.%d", (int)i, (int)fwMajor, (int)fwMinor);
      this->mod->hal->delay(10);
      i++;
    }
  }

  return(flagFound);
}'''

def patch_radiolib(source, target, env):
    root = Path(env["PROJECT_LIBDEPS_DIR"]) / env["PIOENV"] / "RadioLib"
    path = root / "src" / "modules" / "LR2021" / "LR2021.cpp"
    if not path.exists():
        print("w20_radiolib_lr2021_patch: RadioLib LR2021.cpp not found, skip")
        return
    text = path.read_text(encoding="utf-8")
    if PATCH_MARK in text:
        print("w20_radiolib_lr2021_patch: already applied")
        return
    if OLD not in text:
        print("w20_radiolib_lr2021_patch: upstream findChip changed, skip")
        return
    path.write_text(text.replace(OLD, NEW, 1), encoding="utf-8")
    print("w20_radiolib_lr2021_patch: patched findChip")

env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", patch_radiolib)
# Also run immediately at script load so compile sees the patch
try:
    class _Fake:
        pass
    patch_radiolib(None, None, env)
except Exception as e:
    print("w20_radiolib_lr2021_patch: immediate apply failed:", e)
