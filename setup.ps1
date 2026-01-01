$ArmaProject = "C:\Users\Jamie\ArmagetronUE5"
$UE5Root = "C:\Program Files\Epic Games\UE_5.7"

function Build-Arma { 
    & "$UE5Root\Engine\Build\BatchFiles\Build.bat" ArmagetronUE5Editor Win64 Development "$ArmaProject\ArmagetronUE5.uproject" -WaitMutex 
}

function Clean-Arma {
    Remove-Item -Recurse -Force "$ArmaProject\Intermediate" -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force "$ArmaProject\Binaries" -ErrorAction SilentlyContinue
    Write-Host "Cleaned!" -ForegroundColor Green
}

function Open-Arma {
    & "$UE5Root\Engine\Binaries\Win64\UnrealEditor.exe" "$ArmaProject\ArmagetronUE5.uproject"
}

function Rebuild-Arma {
    Clean-Arma
    Build-Arma
}