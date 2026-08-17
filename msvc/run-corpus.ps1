param(
    [string]$Root  = "C:\cc1work\verify",
    [string]$Cases = "",
    [string]$Filter = "*"
)

# The whole single-file corpus, compiled by the MSVC-built cc1 and run on
# Windows, with cl.exe as the reference beside it.
#
# This is the Windows twin of tests/run.sh, and the same argument holds: an
# expectation is an opinion about C, and the platform's own compiler sitting on
# the same disk is the thing worth disagreeing with. Where cc1 and cl agree the
# case is settled whatever the '// expect:' line says; where they differ, one
# of them is wrong and the case is worth reading.
#
# It is NOT the same question tests/windows.sh asks. That one takes 18 cases
# chosen to survive being run under a foreign convention on Linux. This takes
# all of them, natively, and the interesting output is the list at the end.
#
# Some disagreement is expected and correct: the corpus was written against
# LP64, where long is 8 bytes, and Windows is LLP64 where it is 4. A case that
# prints sizeof(long) or packs 64 bits into a long is not a compiler bug when
# it prints something else here - which is why cl is run too, and why a case
# both compilers get "wrong" together is a fact about the data model.
#
#   .\msvc\run-corpus.ps1
#   .\msvc\run-corpus.ps1 -Filter "ld_*"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsRoot  = & $vswhere -latest -products * -property installationPath
$vcvars  = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        Set-Item -Path ("env:" + $matches[1]) -Value $matches[2] -ErrorAction SilentlyContinue
    }
}

if ($Cases -eq "") { $Cases = Join-Path $Root "tests\cases" }
$cc1  = Join-Path $Root "msvc\x64\Release\cc1.exe"
$work = "C:\cc1work\corpus-run"

if (-not (Test-Path $cc1))  { "run-corpus: no cc1.exe at $cc1"; exit 2 }
if (-not (Test-Path $Cases)){ "run-corpus: no cases at $Cases"; exit 2 }

Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $work | Out-Null

$libs = @("libcmt.lib","libucrt.lib","libvcruntime.lib","kernel32.lib",
          "legacy_stdio_definitions.lib")

# Cases whose C is undefined under LLP64, where no answer can be compared
# against any reference because the standard names none. Excluded by name and
# with the reason attached, and reported below rather than dropped: a case that
# stops being printed is a case that stops being thought about, which is how two
# bugs spent a day inside 'cc1 refused: 2'.
#
# The bar for this table is that the C is undefined on this data model, not that
# the case is inconvenient or that the two compilers happen to differ. A case
# that merely reads sizeof(long) is still comparable and stays in the run.
$undefinedHere = [ordered]@{
    'ce_unsigned_long_div' =
        'shifts right by 60 where unsigned long is 32 bits, so the count reaches the width of the type'
}

$refused = @(); $asmFail = @(); $linkFail = @()
$agree = 0; $differ = @(); $clFail = @(); $excluded = @()
$total = 0

Push-Location $work
foreach ($src in Get-ChildItem "$Cases\$Filter.c" | Sort-Object Name) {
    $n = $src.BaseName
    $total++

    if ($undefinedHere.Contains($n)) { $excluded += $n; continue }

    # --- cc1 -------------------------------------------------------------
    & $cc1 -S $src.FullName -o "$work\$n.asm" 2>$null
    if ($LASTEXITCODE -ne 0) { $refused += $n; continue }

    & ml64.exe /nologo /c /Fo "$work\$n.obj" "$work\$n.asm" 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { $asmFail += $n; continue }

    & link.exe /nologo /subsystem:console /out:"$work\$n.exe" "$work\$n.obj" $libs 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { $linkFail += $n; continue }

    # stdout only. Capturing stderr as well merges two streams whose relative
    # order is a property of the C library's buffering rather than of the
    # compiler, and file_streams.c writes to both - so the two builds printed
    # the same lines in a different order and were reported as disagreeing.
    $ourOut = (& "$work\$n.exe" 2>$null | Out-String)
    $ourRc  = $LASTEXITCODE

    # --- cl, the reference ------------------------------------------------
    & cl.exe /nologo /w /Fe"$work\$n.cl.exe" /Fo"$work\$n.cl.obj" $src.FullName 2>&1 | Out-Null
    if (-not (Test-Path "$work\$n.cl.exe")) { $clFail += $n; continue }
    $refOut = (& "$work\$n.cl.exe" 2>$null | Out-String)
    $refRc  = $LASTEXITCODE

    if ($ourOut -eq $refOut -and $ourRc -eq $refRc) {
        $agree++
    } else {
        $differ += [PSCustomObject]@{
            Name = $n; OurRc = $ourRc; RefRc = $refRc
            OurOut = $ourOut.Trim(); RefOut = $refOut.Trim()
        }
    }
}
Pop-Location

"================ corpus on Windows, cc1 (MSVC build) vs cl ================"
"cases:                  $total"
"agree with cl:          $agree"
"disagree with cl:       $($differ.Count)"
"cc1 refused:            $($refused.Count)"
"ml64 refused:           $($asmFail.Count)"
"link failed:            $($linkFail.Count)"
"cl could not build:     $($clFail.Count)"
"excluded, undefined C:  $($excluded.Count)"

if ($refused.Count)  { "`n--- cc1 refused ---";  $refused  | ForEach-Object { "  $_" } }
if ($asmFail.Count)  { "`n--- ml64 refused ---"; $asmFail  | ForEach-Object { "  $_" } }
if ($linkFail.Count) { "`n--- link failed ---";  $linkFail | ForEach-Object { "  $_" } }
if ($clFail.Count)   { "`n--- cl could not build (so not comparable) ---"
                       $clFail | ForEach-Object { "  $_" } }

if ($excluded.Count) { "`n--- excluded: undefined C on this data model ---"
                       $excluded | ForEach-Object { "  ${_}: $($undefinedHere[$_])" } }

if ($differ.Count) {
    "`n--- disagreed with cl ---"
    foreach ($d in $differ) {
        "  $($d.Name): cc1 rc=$($d.OurRc) cl rc=$($d.RefRc)"
        if ($d.OurOut -ne $d.RefOut) {
            "      cc1: $($d.OurOut -replace "`r?`n",' | ')"
            "      cl : $($d.RefOut -replace "`r?`n",' | ')"
        }
    }
}
