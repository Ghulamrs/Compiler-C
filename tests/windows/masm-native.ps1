param([string]$Dir = "C:\cc1work\masm")

# Assemble with ml64 and link with link.exe: the Microsoft toolchain end to
# end, over the MASM that cc1 now writes for this target by default.
#
# vcvars64.bat is what puts ml64 and link on PATH and sets LIB, and there is no
# way to ask a .bat to export into this process - so it is run inside cmd and
# its environment read back out. Doing it once here rather than per case is
# worth about a second a file.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { "masm-native: vswhere not found"; exit 2 }

$vsRoot = & $vswhere -latest -products * -property installationPath
if (-not $vsRoot) { "masm-native: no Visual Studio installation found"; exit 2 }

$vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { "masm-native: vcvars64.bat not found under $vsRoot"; exit 2 }

cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        Set-Item -Path ("env:" + $matches[1]) -Value $matches[2] -ErrorAction SilentlyContinue
    }
}

foreach ($tool in @("ml64.exe", "link.exe")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        "masm-native: $tool is not on PATH after vcvars64"; exit 2
    }
}

# The static CRT, named outright. clang passes these itself through
# -defaultlib directives in the object it writes; link.exe driven directly is
# told nothing, so mainCRTStartup - the entry point that calls main - has to be
# brought in by naming libcmt.
#
# legacy_stdio_definitions, and it is not optional for anything that formats
# into a buffer. Microsoft's UCRT kept printf, fprintf, puts and fputs as real
# exported symbols but made sprintf, the whole v-family and the scanf family
# inline wrappers over __stdio_common_* in <stdio.h>. A compiler that declares
# them as the ordinary functions C says they are - which is what cc1 does,
# correctly - has nothing to link against without this.
$libs = @("libcmt.lib", "libucrt.lib", "libvcruntime.lib", "kernel32.lib",
          "legacy_stdio_definitions.lib")

$pass = 0
$fail = 0

foreach ($src in Get-ChildItem "$Dir\*.c" | Sort-Object Name) {
    $name = $src.BaseName
    $asm = "$Dir\$name.asm"
    $harness = "$Dir\$name.harness.asm"
    $exe = "$Dir\$name.exe"

    $m = Select-String -Path $src.FullName -Pattern '^// expect: *(-?\d+)'
    $expect = if ($m) { $m.Matches.Groups[1].Value } else { "" }

    if (-not (Test-Path $asm)) { "FAIL $name - no assembly was relayed for it"; $fail++; continue }

    $objs = @()
    $bad = $false
    foreach ($input in @($asm) + @(if (Test-Path $harness) { $harness })) {
        $obj = [System.IO.Path]::ChangeExtension($input, ".obj")
        $out = & ml64.exe /nologo /c /Fo $obj $input 2>&1
        if ($LASTEXITCODE -ne 0) {
            "FAIL $name - ml64 refused what cc1 emitted:"
            ($out | Select-String "error" | Select-Object -First 4) |
                ForEach-Object { "       $_" }
            $bad = $true
            break
        }
        $objs += $obj
    }
    if ($bad) { $fail++; continue }

    $out = & link.exe /nologo /subsystem:console /out:$exe $objs $libs 2>&1
    if ($LASTEXITCODE -ne 0) {
        "FAIL $name - link.exe refused the objects:"
        ($out | Select-String "error" | Select-Object -First 4) |
            ForEach-Object { "       $_" }
        $fail++
        continue
    }

    $progOut = (& $exe 2>&1 | Out-String).TrimEnd()
    $rc = $LASTEXITCODE

    if ($expect -ne "" -and "$rc" -ne "$expect") {
        "FAIL $name - Windows gave $rc, the case expects $expect"
        if ($progOut) { "       output: $progOut" }
        $fail++
        continue
    }
    $pass++
}

""
"x86_64-windows (MASM, ml64 + link)  PASS: $pass   FAIL: $fail"
if ($fail -gt 0) { exit 1 }
exit 0
