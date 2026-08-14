param([string]$Dir = "C:\cc1work\suite")

$clang = "C:\Program Files\LLVM\bin\clang.exe"
if (-not (Test-Path $clang)) { "windows-native: clang not found at $clang"; exit 2 }

$pass = 0
$fail = 0

foreach ($src in Get-ChildItem "$Dir\*.c" | Sort-Object Name) {
    $name = $src.BaseName
    $asm = "$Dir\$name.s"
    $harness = "$Dir\$name.harness.asm"
    $exe = "$Dir\$name.exe"

    $m = Select-String -Path $src.FullName -Pattern '^// expect: *(-?\d+)'
    $expect = if ($m) { $m.Matches.Groups[1].Value } else { "" }

    if (-not (Test-Path $asm)) { "FAIL $name - no assembly was relayed for it"; $fail++; continue }

    $inputs = @($asm)
    if (Test-Path $harness) { $inputs += $harness }

    $out = & $clang $inputs -o $exe 2>&1
    if (-not (Test-Path $exe)) {
        "FAIL $name - clang refused what cc1 emitted:"
        ($out | Select-Object -First 4) | ForEach-Object { "       $_" }
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
"x86_64-windows (on Windows)  PASS: $pass   FAIL: $fail"
if ($fail -gt 0) { exit 1 }
exit 0
