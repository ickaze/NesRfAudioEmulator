param(
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [string]$VisualStudioPath = '',
    [switch]$MSBuildOnly
)
$ErrorActionPreference = 'Stop'
try {
    $projectRoot = $PSScriptRoot
    $candidates = New-Object 'System.Collections.Generic.List[string]'
    function Add-Candidate([string]$path) {
        if ($path -and -not $candidates.Contains($path)) { $candidates.Add($path) }
    }
    # An explicit custom installation takes precedence. No vswhere.exe required.
    if (-not $VisualStudioPath) { $VisualStudioPath = $env:VS2022_ROOT }
    if ($VisualStudioPath) { Add-Candidate $VisualStudioPath }
    else {
        Add-Candidate $env:VSINSTALLDIR
        # The VS Installer keeps the actual installation path here, including
        # installations on a non-system drive.
        if ($env:ProgramData) {
            $instances = Join-Path $env:ProgramData 'Microsoft\VisualStudio\Packages\_Instances'
            if (Test-Path -LiteralPath $instances) {
                foreach ($folder in (Get-ChildItem -LiteralPath $instances -Directory -ErrorAction SilentlyContinue)) {
                    $state = Join-Path $folder.FullName 'state.json'
                    if (Test-Path -LiteralPath $state) {
                        try {
                            $info = Get-Content -LiteralPath $state -Raw | ConvertFrom-Json
                            if ([string]$info.installationVersion -like '17.*') { Add-Candidate ([string]$info.installationPath) }
                        } catch { }
                    }
                }
            }
        }
        foreach ($key in @('HKLM:\SOFTWARE\Microsoft\VisualStudio\SxS\VS7', 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\SxS\VS7')) {
            if (Test-Path $key) { Add-Candidate ((Get-ItemProperty $key -ErrorAction SilentlyContinue).'17.0') }
        }
        foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:ProgramW6432)) {
            if ($base) {
                foreach ($edition in @('Community','Professional','Enterprise','BuildTools')) {
                    Add-Candidate (Join-Path $base "Microsoft Visual Studio\2022\$edition")
                }
            }
        }
    }
    $vsRoot = $null
    $msbuild = $null
    foreach ($candidate in $candidates) {
        $builder = Join-Path $candidate 'MSBuild\Current\Bin\MSBuild.exe'
        $cpp = Join-Path $candidate 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt'
        if ((Test-Path -LiteralPath $builder) -and (Test-Path -LiteralPath $cpp)) {
            $vsRoot = (Resolve-Path -LiteralPath $candidate).Path
            $msbuild = $builder
            break
        }
    }
    if ($VisualStudioPath -and -not $vsRoot) {
        throw "VS2022 C++ tools were not found in: $VisualStudioPath"
    }
    if (-not $msbuild) {
        $command = Get-Command MSBuild.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($command) { $msbuild = $command.Source }
    }
    $cmake = $null
    if (-not $MSBuildOnly) {
        if ($env:CMAKE_EXE) {
            if (-not (Test-Path -LiteralPath $env:CMAKE_EXE -PathType Leaf)) { throw "CMAKE_EXE does not name a file: $env:CMAKE_EXE" }
            $cmake = $env:CMAKE_EXE
        } else {
            $command = Get-Command cmake.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($command) { $cmake = $command.Source }
            if (-not $cmake -and $vsRoot) {
                $bundled = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
                if (Test-Path -LiteralPath $bundled) { $cmake = $bundled }
            }
            if (-not $cmake) {
                foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
                    if ($base) {
                        $standalone = Join-Path $base 'CMake\bin\cmake.exe'
                        if (Test-Path -LiteralPath $standalone) { $cmake = $standalone; break }
                    }
                }
            }
        }
    }
    $buildDir = Join-Path $projectRoot 'build-cmake'
    $outputDir = Join-Path $buildDir $Configuration
    if ($cmake) {
        Write-Host "CMake: $cmake"
        $configureArgs = @('-S', $projectRoot, '-B', $buildDir, '-G', 'Visual Studio 17 2022', '-A', 'x64')
        if ($vsRoot) {
            Write-Host "Visual Studio: $vsRoot"
            $configureArgs += "-DCMAKE_GENERATOR_INSTANCE=$vsRoot"
        }
        & $cmake @configureArgs
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $cmake --build $buildDir --config $Configuration --parallel
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } else {
        if (-not $msbuild) {
            throw 'VS2022 C++ build tools were not found. Run from a VS2022 Developer Command Prompt, or pass the VS2022 installation directory as the second argument. Install the Desktop development with C++ workload if it is missing.'
        }
        if (-not $MSBuildOnly) { Write-Host 'CMake is not installed. Building the included solution with MSBuild.' }
        Write-Host "MSBuild: $msbuild"
        New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
        # Forward slash avoids a trailing backslash before a native quoted argument.
        $outProperty = '/p:OutDir=' + $outputDir.Replace('\','/') + '/'
        & $msbuild (Join-Path $projectRoot 'NesRfAudioEmulator.sln') /m /t:Build "/p:Configuration=$Configuration" /p:Platform=x64 $outProperty /nologo
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    $exe = Join-Path $outputDir 'NesRfAudioEmulator.exe'
    if (-not (Test-Path -LiteralPath $exe)) { throw "The build finished but the executable was not found: $exe" }
    Write-Host "Built: $exe"
    exit 0
} catch {
    Write-Host ('Build error: ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
