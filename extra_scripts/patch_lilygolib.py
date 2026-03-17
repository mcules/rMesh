"""
Patches LilyGoLib for rMesh compatibility:
1. Removes ST25R3916 RFAL references (compile fix when USING_ST25R3916 is not defined).
2. Replaces the blocking while(!psramFound()) loop with a one-time warning so the
   device boots normally on boards without PSRAM (e.g. ESP32-S3FN8 variant).

Idempotent: safe to run multiple times.
"""
Import("env")
import os

if env["PIOENV"] != "LILYGO_T-LoraPager":
    Return()

src_dir = os.path.join(env["PROJECT_LIBDEPS_DIR"], env["PIOENV"], "LilyGoLib", "src")

def patch_file(path, replacements):
    if not os.path.exists(path):
        print(f"[patch_lilygolib] WARNING: {path} not found, skipping")
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    changed = False
    for old, new in replacements:
        if old in content:
            content = content.replace(old, new)
            changed = True
    if changed:
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"[patch_lilygolib] Patched {os.path.basename(path)}")

# --- LilyGo_LoRa_Pager.h ---
patch_file(
    os.path.join(src_dir, "LilyGo_LoRa_Pager.h"),
    [
        (
            "extern RfalNfcClass NFCReader;",
            "#ifdef USING_ST25R3916\nextern RfalNfcClass NFCReader;\n#endif",
        ),
    ],
)

# --- LilyGo_LoRa_Pager.cpp ---
patch_file(
    os.path.join(src_dir, "LilyGo_LoRa_Pager.cpp"),
    [
        # Global NFC object instantiation
        (
            "RfalRfST25R3916Class nfc_hw(&SPI, NFC_CS, NFC_INT);\nRfalNfcClass NFCReader(&nfc_hw);",
            "#ifdef USING_ST25R3916\nRfalRfST25R3916Class nfc_hw(&SPI, NFC_CS, NFC_INT);\nRfalNfcClass NFCReader(&nfc_hw);\n#endif",
        ),
        # initNFC() body
        (
            '    bool res = false;\n    log_d("Init NFC");\n    res = NFCReader.rfalNfcInitialize() == ST_ERR_NONE;\n    if (!res) {\n        log_e("Failed to find NFC Reader");\n    } else {\n        log_d("Initializing NFC Reader succeeded");\n        devices_probe |= HW_NFC_ONLINE;\n        // Turn off NFC power\n        powerControl(POWER_NFC, false);\n    }\n    return res;',
            '    bool res = false;\n#ifdef USING_ST25R3916\n    log_d("Init NFC");\n    res = NFCReader.rfalNfcInitialize() == ST_ERR_NONE;\n    if (!res) {\n        log_e("Failed to find NFC Reader");\n    } else {\n        log_d("Initializing NFC Reader succeeded");\n        devices_probe |= HW_NFC_ONLINE;\n        // Turn off NFC power\n        powerControl(POWER_NFC, false);\n    }\n#endif\n    return res;',
        ),
        # Replace blocking while(!psramFound()) with a one-time log warning
        (
            "    while (!psramFound()) {\n        log_d(\"ERROR:PSRAM NOT FOUND!\"); delay(1000);\n    }",
            "    if (!psramFound()) {\n        log_w(\"WARNING: PSRAM NOT FOUND - continuing without PSRAM\");\n    }",
        ),
    ],
)

# --- LilyGoDispInterface.cpp ---
# Fix off-by-2x allocation: draw_buf was sized _width*_height*2 uint16_t elements
# (= 4 bytes/pixel), but uint16_t is already 2 bytes so only _width*_height needed.
# Without PSRAM this caused a std::bad_alloc crash (426 KB requested, ~250 KB available).
patch_file(
    os.path.join(src_dir, "LilyGoDispInterface.cpp"),
    [
        (
            "std::vector<uint16_t> draw_buf(_width * _height * 2, 0x0000);",
            "std::vector<uint16_t> draw_buf(_width * _height, 0x0000);",
        ),
    ],
)
