param([string]$Dir = "C:\cc1work\cc1lab")

# The Windows half of on-windows.sh: assemble what was relayed, link it against
# the static CRT, and run it. Driven from the Mac over ssh; see the note at the
# top of the shell script beside this one.

# vcvars64.bat is what puts ml64 and link on PATH and sets LIB, and a .bat
# cannot export into this process - so it is run inside cmd and its environment
# is read back out.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { "cc1lab: vswhere not found"; exit 2 }

$vsRoot = & $vswhere -latest -products * -property installationPath
if (-not $vsRoot) { "cc1lab: no Visual Studio installation found"; exit 2 }

$vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { "cc1lab: vcvars64.bat not found under $vsRoot"; exit 2 }

cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        Set-Item -Path ("env:" + $matches[1]) -Value $matches[2] -ErrorAction SilentlyContinue
    }
}

foreach ($tool in @("ml64.exe", "link.exe")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        "cc1lab: $tool is not on PATH after vcvars64"; exit 2
    }
}

# link.exe driven directly is told nothing, where a compiler driver would have
# embedded -defaultlib directives in the object. libcmt brings in
# mainCRTStartup; legacy_stdio_definitions is required by anything that formats
# into a buffer, because the UCRT made sprintf and the v-family header inlines
# over __stdio_common_* rather than real exported symbols.
$libs = @("libcmt.lib", "libucrt.lib", "libvcruntime.lib", "kernel32.lib",
          "legacy_stdio_definitions.lib")

$objs = @()
foreach ($asm in Get-ChildItem "$Dir\*.asm" | Sort-Object Name) {
    $obj = Join-Path $Dir "$($asm.BaseName).obj"
    & ml64.exe /nologo /c /Fo $obj $asm.FullName 2>&1 | ForEach-Object { "  $_" }
    if ($LASTEXITCODE -ne 0) { "cc1lab: ml64 refused $($asm.Name)"; exit 1 }
    $objs += $obj
}

$exe = Join-Path $Dir "CC1Lab.exe"
& link.exe /nologo /subsystem:console /out:$exe $objs $libs 2>&1 |
    ForEach-Object { "  $_" }
if ($LASTEXITCODE -ne 0) { "cc1lab: link refused it"; exit 1 }

"=== CC1Lab, x86_64-windows, cc1 + ml64 + link.exe ==="
& $exe
$rc = $LASTEXITCODE
""
"exit code: $rc"
exit $rc
