param([string]$IdfPath = 'C:\Espressif\esp-idf-v6.0.2')
$ErrorActionPreference = 'Stop'
$env:IDF_TOOLS_PATH = Join-Path $env:USERPROFILE '.espressif'
$env:PYTHONUTF8 = '1'
$idfPythonScripts = Join-Path $env:IDF_TOOLS_PATH 'python_env\idf6.0_py3.11_env\Scripts'
$env:Path = $idfPythonScripts + ';' + $env:Path
. (Join-Path $IdfPath 'export.ps1')
Push-Location (Join-Path $PSScriptRoot 'firmware')
try {
    python "$env:IDF_PATH\tools\idf.py" -B build-idf6 -DSDKCONFIG=build-idf6/sdkconfig -DIDF_TARGET=esp32c3 build
    if ($LASTEXITCODE -ne 0) { throw 'Fishing build failed.' }
    python "$env:IDF_PATH\tools\idf.py" -B build-idf6 merge-bin -o nabo-fishing-merged.bin
    if ($LASTEXITCODE -ne 0) { throw 'Firmware merge failed.' }
    python tools/verify_firmware.py build-idf6
    if ($LASTEXITCODE -ne 0) { throw 'Firmware layout verification failed.' }
} finally { Pop-Location }
