"""
Fails the build when firmware.bin exceeds the legacy OTA slot size.

Boards migrated to partitions_8MB.csv (2 MB app slots) still have fielded
devices on the old partitions.csv layout (1.75 MB app slots). Those devices
can only be updated over the air, so every firmware image must keep fitting
the OLD slot until the fleet is reflashed. Set `custom_legacy_ota_limit`
in the environment to enforce this; environments without the option are
not checked (PlatformIO already enforces their own partition size).
"""
Import("env")

limit_str = env.GetProjectOption("custom_legacy_ota_limit", "")

if limit_str:
    import os

    limit = int(str(limit_str), 0)

    def check_size(source, target, env):
        fw = os.path.join(env.subst("$BUILD_DIR"), "firmware.bin")
        if not os.path.isfile(fw):
            return
        size = os.path.getsize(fw)
        if size > limit:
            print(f"check_fw_size.py: ERROR firmware.bin is {size} bytes, "
                  f"legacy OTA slot is {limit} bytes - OTA to old-layout "
                  f"devices would fail! Shrink the firmware.")
            env.Exit(1)
        else:
            print(f"check_fw_size.py: firmware.bin {size} bytes, "
                  f"{limit - size} bytes legacy OTA headroom")

    env.AddPostAction("$BUILD_DIR/firmware.bin", check_size)
