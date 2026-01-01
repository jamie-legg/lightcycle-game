# ArmagetronUE5 DevOps Guide

## Quick Reference

| Action | Command | When to Use |
|--------|---------|-------------|
| Build (Development) | `Build-Dev` | Daily coding, testing features |
| Build (Shipping) | `Build-Ship` | Ready to share/release |
| Rebuild All | `Rebuild-All` | Weird bugs, after engine update |
| Clean Build | `Clean-Build` | Broken state, corrupted files |
| Open Editor | `Open-Editor` | Design levels, test in PIE |
| Generate Project Files | `Generate-Project` | After adding new .cpp/.h files |

---

## Prerequisites

### Environment Setup
**When to run:** Once, when setting up your dev machine, or after reinstalling UE5.

```powershell
# Set UE5 installation path (adjust to your installation)
$env:UE5_ROOT = "C:\Program Files\Epic Games\UE_5.7"

# Add to PATH for easy access
$env:PATH += ";$env:UE5_ROOT\Engine\Binaries\Win64"
```

### Verify Installation
**When to run:** If build commands aren't working, or after installing/updating UE5.

```powershell
# Check UnrealBuildTool is accessible
& "$env:UE5_ROOT\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -help
```

---

## Building

### Development Build (Editor)
**When to run:** 
- ✅ After modifying C++ code and you want to test in the Editor
- ✅ When Hot Reload fails or you closed the Editor
- ✅ First thing after pulling from git

```powershell
# Build for Editor (Development)
& "$env:UE5_ROOT\Engine\Build\BatchFiles\Build.bat" ArmagetronUE5Editor Win64 Development "C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject" -WaitMutex
```

### Development Build (Standalone Game)
**When to run:**
- ✅ Testing the game outside the Editor (no PIE overhead)
- ✅ Testing multiplayer/networking scenarios
- ✅ Profiling performance without Editor overhead

```powershell
# Build standalone game (Development)
& "$env:UE5_ROOT\Engine\Build\BatchFiles\Build.bat" ArmagetronUE5 Win64 Development "C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject" -WaitMutex
```

### Shipping Build (Release)
**When to run:**
- ✅ Creating a build to share with testers
- ✅ Final release build
- ✅ Performance benchmarking (most optimized)
- ❌ NOT for debugging - no debug symbols!

```powershell
# Build for release/distribution
& "$env:UE5_ROOT\Engine\Build\BatchFiles\Build.bat" ArmagetronUE5 Win64 Shipping "C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject" -WaitMutex
```

### Debug Build
**When to run:**
- ✅ Tracking down crashes with full stack traces
- ✅ Using Visual Studio debugger with breakpoints
- ✅ Investigating memory corruption or weird behavior
- ❌ NOT for performance testing - very slow!

```powershell
# Build with debug symbols
& "$env:UE5_ROOT\Engine\Build\BatchFiles\Build.bat" ArmagetronUE5 Win64 DebugGame "C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject" -WaitMutex
```

---

## Cleaning

### Clean Intermediate Files
**When to run:**
- ✅ Build is acting weird, incremental compile seems broken
- ✅ After changing Build.cs files
- ✅ Header file changes not being picked up

```powershell
# Remove Intermediate folder
Remove-Item -Recurse -Force "C:\Users\Jamie\ArmagetronUE5\Intermediate" -ErrorAction SilentlyContinue

# Remove Saved folder (optional - includes logs and configs)
Remove-Item -Recurse -Force "C:\Users\Jamie\ArmagetronUE5\Saved" -ErrorAction SilentlyContinue
```

### Clean Binaries
**When to run:**
- ✅ DLL conflicts or "module could not be loaded" errors
- ✅ After changing UPROPERTY/UFUNCTION macros significantly
- ✅ Linker errors that don't make sense

```powershell
# Remove compiled binaries
Remove-Item -Recurse -Force "C:\Users\Jamie\ArmagetronUE5\Binaries" -ErrorAction SilentlyContinue
```

### Full Clean (Nuclear Option)
**When to run:**
- ✅ Nothing else works, build is completely broken
- ✅ After upgrading Unreal Engine version
- ✅ Switching between major branches with different dependencies
- ✅ "I've tried everything" situations
- ⚠️ WARNING: This takes a while to rebuild from scratch!

```powershell
# Complete clean - removes all generated files
$ProjectPath = "C:\Users\Jamie\ArmagetronUE5"
Remove-Item -Recurse -Force "$ProjectPath\Intermediate" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "$ProjectPath\Binaries" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "$ProjectPath\DerivedDataCache" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "$ProjectPath\.vs" -ErrorAction SilentlyContinue
Remove-Item -Force "$ProjectPath\*.sln" -ErrorAction SilentlyContinue
Write-Host "Clean complete. Run Generate-Project to regenerate solution files."
```

---

## Rebuilding

### Rebuild (Clean + Build)
**When to run:**
- ✅ Same scenarios as "Clean Intermediate" but want one command
- ✅ Ensuring a clean slate before committing code
- ✅ CI/CD pipeline builds

```powershell
# Full rebuild
& "$env:UE5_ROOT\Engine\Build\BatchFiles\Build.bat" ArmagetronUE5Editor Win64 Development "C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject" -WaitMutex -Clean
```

### Rebuild Specific Module
**When to run:**
- ✅ Only changed code in one module
- ✅ Faster than full rebuild when you know what changed
- ✅ Isolating build issues to a specific module

```powershell
# Rebuild just the ArmagetronUE5 module
& "$env:UE5_ROOT\Engine\Build\BatchFiles\Build.bat" ArmagetronUE5Editor Win64 Development "C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject" -WaitMutex -Module=ArmagetronUE5
```

---

## Project Files Generation

### Generate Visual Studio Solution
**When to run:**
- ✅ After adding NEW .cpp or .h files to the project
- ✅ After modifying Build.cs or Target.cs files
- ✅ IntelliSense is broken or showing wrong errors
- ✅ After cloning the repo for the first time
- ✅ After running "Full Clean"

```powershell
# Generate .sln and project files
& "$env:UE5_ROOT\Engine\Build\BatchFiles\GenerateProjectFiles.bat" "C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject" -Game -Engine
```

### Using UnrealBuildTool Directly
**When to run:** Same as above, alternative method if batch file has issues.

```powershell
& "$env:UE5_ROOT\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -ProjectFiles -Project="C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject" -Game -Engine
```

---

## Running

### Open Editor
**When to run:**
- ✅ Designing levels, placing actors
- ✅ Testing with Play-In-Editor (PIE)
- ✅ Adjusting Blueprint settings
- ✅ Importing/managing assets

```powershell
# Launch Unreal Editor with project
& "$env:UE5_ROOT\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject"
```

### Open Editor with Specific Map
**When to run:**
- ✅ Jumping straight to the map you're working on
- ✅ Automated testing workflows
- ✅ Faster iteration when you know which map you need

```powershell
# Launch directly into a map
& "$env:UE5_ROOT\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject" "/Game/Maps/TestArena"
```

### Run Standalone Game
**When to run:**
- ✅ Testing without Editor overhead
- ✅ Testing game startup/shutdown behavior
- ✅ Checking how the game behaves when packaged
- ✅ Multiplayer testing (can run multiple instances)

```powershell
# Run the compiled game (after building)
& "C:\Users\Jamie\ArmagetronUE5\Binaries\Win64\ArmagetronUE5.exe" -game
```

### Run Game with Log Window
**When to run:**
- ✅ Debugging crashes or issues in standalone game
- ✅ Seeing UE_LOG output in real-time
- ✅ Investigating why something works in Editor but not standalone

```powershell
& "C:\Users\Jamie\ArmagetronUE5\Binaries\Win64\ArmagetronUE5.exe" -game -log
```

---

## Packaging

### Package for Windows
**When to run:**
- ✅ Creating a distributable build
- ✅ Sending to testers/playtesters
- ✅ Uploading to Steam/itch.io/etc
- ✅ Final release preparation
- ⚠️ Takes a long time - go get coffee!

```powershell
# Cook and package for Windows
& "$env:UE5_ROOT\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun `
    -Project="C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject" `
    -NoP4 `
    -Platform=Win64 `
    -ClientConfig=Shipping `
    -Cook `
    -Build `
    -Stage `
    -Pak `
    -Archive `
    -ArchiveDirectory="C:\Users\Jamie\ArmagetronUE5\Packaged"
```

### Cook Content Only
**When to run:**
- ✅ Testing if all assets cook correctly
- ✅ Finding missing asset references
- ✅ Faster than full package when just checking content
- ✅ Before doing a full package to catch errors early

```powershell
# Just cook content (useful for testing)
& "$env:UE5_ROOT\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun `
    -Project="C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject" `
    -NoP4 `
    -Platform=Win64 `
    -ClientConfig=Development `
    -Cook `
    -SkipBuild
```

---

## Automation Scripts

### build.ps1 - Quick Build Script
**When to use:** Save this script once, then use it for all your builds.

Save this as `build.ps1` in your project root:

```powershell
param(
    [string]$Config = "Development",
    [switch]$Clean,
    [switch]$Editor
)

$UE5Root = "C:\Program Files\Epic Games\UE_5.7"
$ProjectPath = "C:\Users\Jamie\ArmagetronUE5\ArmagetronUE5.uproject"
$Target = if ($Editor) { "ArmagetronUE5Editor" } else { "ArmagetronUE5" }

$Args = @($Target, "Win64", $Config, $ProjectPath, "-WaitMutex")
if ($Clean) { $Args += "-Clean" }

Write-Host "Building $Target ($Config)..." -ForegroundColor Cyan
& "$UE5Root\Engine\Build\BatchFiles\Build.bat" @Args

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build succeeded!" -ForegroundColor Green
} else {
    Write-Host "Build failed with code $LASTEXITCODE" -ForegroundColor Red
}
```

Usage:
```powershell
# Development editor build - daily workflow
.\build.ps1 -Editor

# Shipping standalone build - for testers
.\build.ps1 -Config Shipping

# Clean rebuild - when things are broken
.\build.ps1 -Editor -Clean
```

---

## Troubleshooting

### Common Issues

#### "Could not find NetFxSDK"
**When you see this:** Build fails early with .NET SDK error.

```powershell
# Install .NET SDK components via Visual Studio Installer
# Or set environment variable:
$env:NETFXSDK_DIR = "C:\Program Files (x86)\Windows Kits\NETFXSDK\4.8"
```

#### Build Fails with Linker Errors
**When you see this:** LNK2001, LNK2019, unresolved external symbol errors.

```powershell
# Clean and regenerate
Remove-Item -Recurse -Force "Intermediate" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "Binaries" -ErrorAction SilentlyContinue
& "$env:UE5_ROOT\Engine\Build\BatchFiles\GenerateProjectFiles.bat" "$ProjectPath"
```

#### Hot Reload Not Working
**When you see this:** Changes in Editor don't reflect code changes, or Hot Reload button is greyed out.

```powershell
# Force full rebuild of module
& "$env:UE5_ROOT\Engine\Build\BatchFiles\Build.bat" ArmagetronUE5Editor Win64 Development "$ProjectPath" -WaitMutex -ForceHotReloadFromIDE
```

#### Shader Compilation Issues
**When you see this:** Pink/missing materials, "compiling shaders" never finishes, shader compile errors.

```powershell
# Clear shader cache
Remove-Item -Recurse -Force "Saved\ShaderDebugInfo" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "DerivedDataCache" -ErrorAction SilentlyContinue
```

---

## Useful Log Locations

| Log | Path | When to Check |
|-----|------|---------------|
| Build Log | `Saved\Logs\ArmagetronUE5.log` | After crashes, to see what happened |
| Cook Log | `Saved\Cooked\*\Metadata\CookerOpenOrder.log` | Packaging fails with asset errors |
| Crash Dumps | `Saved\Crashes\` | After a hard crash, for bug reports |

### View Recent Build Log
**When to run:** After a crash to see what went wrong.

```powershell
Get-Content "Saved\Logs\ArmagetronUE5.log" -Tail 100
```

### Watch Log in Real-Time
**When to run:** While the game is running to see live output.

```powershell
Get-Content "Saved\Logs\ArmagetronUE5.log" -Wait -Tail 20
```

---

## VS Code / Cursor Integration

### tasks.json
**When to set up:** Once, to enable Ctrl+Shift+B build shortcuts.

Add to `.vscode/tasks.json`:
```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Build Editor (Development)",
            "type": "shell",
            "command": "${env:UE5_ROOT}/Engine/Build/BatchFiles/Build.bat",
            "args": [
                "ArmagetronUE5Editor",
                "Win64", 
                "Development",
                "${workspaceFolder}/ArmagetronUE5.uproject",
                "-WaitMutex"
            ],
            "group": "build",
            "problemMatcher": "$msCompile"
        },
        {
            "label": "Clean Build",
            "type": "shell", 
            "command": "Remove-Item",
            "args": ["-Recurse", "-Force", "Intermediate", "-ErrorAction", "SilentlyContinue"],
            "group": "build"
        }
    ]
}
```

---

## Quick Command Aliases

**When to set up:** Once, to make your life easier with short commands.

Add to your PowerShell profile (`$PROFILE`):

```powershell
# ArmagetronUE5 aliases
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
```

---

## Build Configurations Reference

| Configuration | When to Use | Optimization | Debug Info |
|--------------|-------------|--------------|------------|
| Debug | Debugging engine internals, deep issues | None | Full |
| DebugGame | Debugging your game code with breakpoints | Engine optimized | Game debug |
| Development | Daily coding and testing | Partial | Yes |
| Shipping | Release builds, sharing with others | Full | Stripped |
| Test | QA testing with some debug features | Full | Minimal |

---

## Typical Workflows

### Daily Development
1. Pull latest from git
2. Run `Build-Arma` or `.\build.ps1 -Editor`
3. Open Editor with `Open-Arma`
4. Make changes, use Hot Reload (Ctrl+Alt+F11 in Editor)
5. If Hot Reload fails → Close Editor → Build → Reopen

### Debugging a Crash
1. Build with Debug: `.\build.ps1 -Editor -Config DebugGame`
2. Open in Visual Studio
3. Attach debugger or run with F5
4. Check `Saved\Logs\` and `Saved\Crashes\` for clues

### Creating a Release
1. Clean build: `.\build.ps1 -Clean`
2. Test in Editor thoroughly
3. Run "Cook Content Only" to catch asset issues
4. Run full "Package for Windows"
5. Test the packaged build before sharing

### Everything is Broken
1. Close Editor and Visual Studio
2. Run "Full Clean (Nuclear Option)"
3. Run "Generate Visual Studio Solution"
4. Build from scratch
5. If still broken, check git status for accidental changes
