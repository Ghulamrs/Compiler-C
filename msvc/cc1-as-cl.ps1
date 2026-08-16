# Stand where cl.exe stands, and hand the C to cc1.
#
# The Mac has tools/cc1-as-clang, which lets Xcode build with this compiler by
# translating a clang command line into a cc1 one. This is the same idea for
# Visual Studio, and the same shape: MSBuild believes it is running cl.exe,
# and gets an .obj back.
#
#   a .c file   -> cc1 -S, then ml64 on the assembly, giving the .obj
#   anything else (.cpp, .rc, a link step) -> the real cl.exe, untouched
#
# Wire it up by setting two properties in the .vcxproj - see demo\hello.vcxproj:
#
#   <CLToolExe>cc1-as-cl.bat</CLToolExe>
#   <CLToolPath>..\path\to\msvc</CLToolPath>
#
# **The response file is the part that is not obvious.** MSBuild does not pass
# sixty flags on the command line the way Xcode does; it writes them to a file
# and runs 'cl.exe @C:\...\file.rsp'. A shim that only reads its arguments sees
# one of them, beginning with '@', and no source file at all. So the first
# thing below is expanding that, and the encoding matters: MSBuild writes the
# file as UTF-16 or UTF-8 depending on version and content, which is why it is
# read by asking .NET to detect from the byte order mark rather than assumed.
#
# **What cc1 does not take, this drops.** /O2, /Zi, /EHsc, /MD and the rest are
# not flags cc1 has, and a C90 compiler with one optimisation level has nothing
# to do with most of them. Only the four that change what the program *means*
# are translated: /I, /D, /U, and the output name. Anything unrecognised is
# ignored rather than refused, because MSBuild sends a great deal that is
# perfectly reasonable for cl and meaningless here.
#
# **The limit worth knowing.** cc1 has no system include path: it searches -I
# and the fifteen headers it ships, so a project that includes <windows.h> will
# not build here. That is the same shield and limit tools/cc1-as-clang carries
# on the Mac, and it is what makes the output comparable across the two.
#
# CC1_AS_CL_VERBOSE=1 prints what it decided, per file.

$ErrorActionPreference = "Continue"

function Say($m) { if ($env:CC1_AS_CL_VERBOSE) { [Console]::Error.WriteLine("cc1-as-cl: $m") } }

# --- expand response files -------------------------------------------------
# One level is enough: MSBuild does not nest them.
$argv = @()
foreach ($a in $args) {
    if ($a -like '@*') {
        $rsp = $a.Substring(1).Trim('"')
        if (-not (Test-Path $rsp)) { [Console]::Error.WriteLine("cc1-as-cl: no response file $rsp"); exit 1 }
        # Detect the encoding from the BOM rather than guessing.
        $text = [System.IO.File]::ReadAllText($rsp)
        # Response files quote any argument containing a space. Split on
        # whitespace that is not inside double quotes.
        foreach ($m in [regex]::Matches($text, '(?:"[^"]*"|\S)+')) {
            $t = $m.Value.Trim()
            if ($t) { $argv += $t }
        }
    } else {
        $argv += $a
    }
}

# --- work out what is being asked for --------------------------------------
$sources   = @()
$passthru  = @()          # -I / -D / -U, in cc1 spelling
$foPath    = ""
$compiling = $false

# cl takes each of these two ways - '/DNAME' and '/D NAME' - and MSBuild uses
# the separated form, which is why this walks by index rather than foreach: a
# flag may claim the argument after it. Reading only the joined form is what
# made the first version of this silently drop every /D, so the demo compiled
# and printed that its macro had not arrived.
for ($i = 0; $i -lt $argv.Count; $i++) {
    $u = $argv[$i].Trim('"')

    # Case-sensitively, all of it. PowerShell matches without regard to case by
    # default and cl's flags are not like that: '/D' defines a macro and
    # '/diagnostics:column' chooses a message format, and a case-blind pattern
    # for '/D(.+)' reads the second as a definition of 'iagnostics:column'.
    # That is exactly what happened, and cc1 answered "unknown option -d".
    if ($u -cmatch '^[/-]([IDU])$' -and $i + 1 -lt $argv.Count) {
        $passthru += "-" + $matches[1]
        $passthru += $argv[++$i].Trim('"')
        continue
    }
    if ($u -cmatch '^[/-]Fo$' -and $i + 1 -lt $argv.Count) {
        $foPath = $argv[++$i].Trim('"')
        continue
    }

    switch -Regex -CaseSensitive ($u) {
        '^[/-]c$'        { $compiling = $true }
        '^[/-]Fo(.+)$'   { $foPath = $matches[1].Trim('"') }
        '^[/-]([IDU])(.+)$' { $passthru += "-" + $matches[1]
                              $passthru += $matches[2].Trim('"') }
        '^[/-]'          { }                               # not ours; drop it
        default {
            if ($u -match '\.(c|cpp|cc|cxx)$') { $sources += $u }
        }
    }
}

# Anything that is not a single C file compiled to an object is cl's business.
$nonC = $sources | Where-Object { $_ -notmatch '\.c$' }
if (-not $compiling -or $sources.Count -eq 0 -or $nonC) {
    Say "not a C compile - handing to cl.exe"
    & cl.exe @args
    exit $LASTEXITCODE
}

# --- compile ---------------------------------------------------------------
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cc1  = if ($env:CC1) { $env:CC1 } else { Join-Path $root "x64\Release\cc1.exe" }
if (-not (Test-Path $cc1)) {
    [Console]::Error.WriteLine("cc1-as-cl: no cc1.exe at $cc1 - build msvc\cc1.vcxproj, or set CC1")
    exit 1
}

$rc = 0
foreach ($src in $sources) {
    # /Fo may name a file or a directory; a trailing backslash means the latter,
    # which is what MSBuild passes for a project with more than one source.
    if ($foPath -eq "") {
        $obj = [System.IO.Path]::ChangeExtension($src, ".obj")
    } elseif ($foPath.EndsWith("\") -or (Test-Path $foPath -PathType Container)) {
        $obj = Join-Path $foPath.TrimEnd('\') `
                         ([System.IO.Path]::GetFileNameWithoutExtension($src) + ".obj")
    } else {
        $obj = $foPath
    }

    $asm = [System.IO.Path]::ChangeExtension($obj, ".asm")
    $dir = Split-Path -Parent $obj
    if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null }

    Say "$src -> $obj via cc1"

    & $cc1 -S @passthru $src -o $asm
    if ($LASTEXITCODE -ne 0) { $rc = $LASTEXITCODE; continue }

    # ml64 is the assembler for what cc1 writes here. cl would have called it
    # too, for a .asm; it simply never sees one in an ordinary C project.
    & ml64.exe /nologo /c /Fo $obj $asm | Out-Null
    if ($LASTEXITCODE -ne 0) {
        [Console]::Error.WriteLine("cc1-as-cl: ml64 refused the assembly for $src")
        $rc = $LASTEXITCODE
    }
}

exit $rc
