# Build the existing reference client in GitHub Actions, without game data.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $IsWindows -or $env:GITHUB_ACTIONS -ne 'true') {
    throw 'Client builds must run in GitHub Actions on a Windows runner.'
}

$sourceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $sourceRoot
$evidenceRoot = Join-Path $sourceRoot 'artifacts/windows-baseline'
$binaryRoot = Join-Path $evidenceRoot 'bin'
New-Item -ItemType Directory -Force $binaryRoot | Out-Null

function Invoke-LoggedCommand {
    param([string]$Command, [string[]]$Arguments, [string]$LogName)
    & $Command @Arguments 2>&1 | Tee-Object -FilePath (Join-Path $evidenceRoot $LogName)
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE; see $LogName."
    }
}

$manifest = [ordered]@{
    schema_version = 1
    source_commit = (git rev-parse HEAD).Trim()
    workflow_run = "$env:GITHUB_SERVER_URL/$env:GITHUB_REPOSITORY/actions/runs/$env:GITHUB_RUN_ID"
    runner_os = $env:RUNNER_OS
    runner_arch = $env:RUNNER_ARCH
    runner_image = $env:ImageVersion
    configuration = 'Release'
    generator = 'Visual Studio 17 2022'
    architecture = 'x64'
    signature_checks = 'enabled'
    crash_logger = 'enabled'
    status = 'preflight'
    runtime_validation = 'not run; requires a separate Windows session with game data and a test server'
    dependencies = @()
    binaries = @()
}

try {
    $manifest['cmake_version'] = (cmake --version | Select-Object -First 1)
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $manifest['visual_studio'] = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json)

    foreach ($path in @(
        'ThirdParty/nvapi/amd64/nvapi64.lib',
        'ThirdParty/nvapi/x86/nvapi.lib',
        'ThirdParty/xvidcore/lib/x64/libxvidcore.lib'
    )) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Missing tracked dependency: $path"
        }
        $manifest.dependencies += [ordered]@{
            path = $path
            bytes = (Get-Item -LiteralPath $path).Length
            sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }

    $manifest.status = 'configuring'
    Invoke-LoggedCommand -Command cmake -Arguments @(
        '-S', '.', '-B', 'build/x64', '-G', 'Visual Studio 17 2022', '-A', 'x64',
        '-DWOWDECOMP_DISABLE_SIGNATURE_CHECKS=OFF', '-DWOWCLIENT_CRASH_LOG=ON'
    ) -LogName 'configure.log'

    $manifest.status = 'building'
    Invoke-LoggedCommand -Command cmake -Arguments @(
        '--build', 'build/x64', '--config', 'Release', '--target', 'WowClient', 'Wow', '--parallel', '2'
    ) -LogName 'build.log'

    foreach ($name in @('WowClient64.exe', 'Wow64.exe', 'XvidDecoder.dll')) {
        $path = Join-Path 'bin/Win' $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Build did not produce $path"
        }
        Copy-Item -LiteralPath $path -Destination $binaryRoot
        $manifest.binaries += [ordered]@{
            path = "bin/$name"
            bytes = (Get-Item -LiteralPath $path).Length
            sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    $manifest.status = 'build passed; runtime unverified'
} catch {
    $manifest['failure'] = $_.Exception.Message
    throw
} finally {
    $manifest | ConvertTo-Json -Depth 10 | Set-Content -Encoding utf8 (Join-Path $evidenceRoot 'manifest.json')
}
