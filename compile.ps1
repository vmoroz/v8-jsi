cls
cd E:\GitHub\vmoroz\v8-jsi

& ninja -v -j 32 -C E:\GitHub\vmoroz\v8-jsi\build\v8build\v8\out\win32\x64\Debug v8jsi jsitests
if (!$?) {
    Write-Host "Build failure, check logs for details"
    exit 1
}

e:\GitHub\vmoroz\v8-jsi\build\v8build\v8\out\win32\x64\Debug\jsitests.exe