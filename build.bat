set Envs=ESP32_Without_LoRa LILYGO_T3_LoRa32_V1_6_1 LILYGO_T-Beam HELTEC_Wireless_Stick_Lite_V3 HELTEC_WiFi_LoRa_32_V3 HELTEC_WiFi_LoRa_32_V4
set VersionOLD=V1.0.13-a


echo %Version%

for %%n in (%Envs%) do (
	C:\Users\dh1nfj\.platformio\penv\Scripts\platformio.exe run --environment %%n
	C:\Users\dh1nfj\.platformio\penv\Scripts\platformio.exe run --target buildfs --environment %%n
    xcopy ".pio\build\%%n\*.bin" "\\192.168.33.1\web\dh1nfj\rMesh\%%n" /Y /I
    xcopy ".pio\build\%%n\*.bin" "\\192.168.33.1\web\dh1nfj\rMesh\%%n\%VersionOLD%" /Y /I
)

xcopy "changelog.txt" "\\192.168.33.1\web\dh1nfj\rMesh" /Y /I

pause