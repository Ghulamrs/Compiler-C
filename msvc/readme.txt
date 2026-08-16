================================================================================
 USING cc1 AS THE C COMPILER INSIDE VISUAL STUDIO
================================================================================

 What this gives you: an ordinary Visual Studio C project that is compiled by
 cc1 instead of cl.exe. You press F7, or run msbuild, and the .obj files come
 out of this compiler. The linker, the debugger and the project system are
 Microsoft's and are untouched.

 Everything here was run on a real machine before being written down.


--------------------------------------------------------------------------------
 0.  WHAT YOU NEED
--------------------------------------------------------------------------------

 * Visual Studio 2022 or later, with "Desktop development with C++".
   The projects are in the 2022 format (ToolsVersion 17.0). They do NOT pin a
   toolset, so a later Visual Studio builds them as well - see step 1c if you
   want to force one.

 * A Developer Command Prompt, or a shell where vcvars64.bat has been run.
   This is not optional and it is needed twice: to build cc1, and to use it.
   ml64.exe and link.exe are on PATH only inside that environment.

     "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

   (Adjust for your edition. vswhere.exe under
    "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer" will find it.)


--------------------------------------------------------------------------------
 1.  BUILD cc1.exe
--------------------------------------------------------------------------------

 From the repository root, in a Developer Command Prompt:

     msbuild msvc\cc1.vcxproj /p:Configuration=Release /p:Platform=x64

 1a. The result lands here, and this path matters later:

     msvc\x64\Release\cc1.exe

 1b. Check it runs. With no arguments it prints its usage:

     msvc\x64\Release\cc1.exe

 1c. If msbuild says

       error MSB8020: The build tools for Visual Studio 2022 (Platform Toolset
       = 'v143') cannot be found

     then something has pinned the toolset. The project does not, so this comes
     from your command line or from a newer/older Visual Studio. Name one you
     actually have:

     msbuild msvc\cc1.vcxproj /p:Configuration=Release /p:Platform=x64 ^
             /p:PlatformToolset=v143

 1d. Nothing in src\ is modified to build here. The one thing MSVC lacks is
     <unistd.h>, and msvc\compat\unistd.h answers that include; the project
     puts that directory first on the include path. If you move this project,
     keep compat\ with it.


--------------------------------------------------------------------------------
 2.  CHECK IT COMPILES SOMETHING
--------------------------------------------------------------------------------

 Before wiring it into a project, prove the compiler works on its own. In a
 Developer Command Prompt:

     msvc\x64\Release\cc1.exe -S msvc\demo\hello.c -o hello.asm
     ml64.exe /nologo /c /Fo hello.obj hello.asm
     link.exe /nologo /subsystem:console /out:hello.exe hello.obj ^
              libcmt.lib libucrt.lib libvcruntime.lib kernel32.lib ^
              legacy_stdio_definitions.lib
     hello.exe

 That is the whole pipeline. cc1 writes MASM; ml64 assembles it; link joins it
 to the C runtime.

 WHY THOSE FIVE LIBRARIES. link.exe driven directly is told nothing, where a
 cl-produced object would have carried /defaultlib directives inside it.
 libcmt brings in mainCRTStartup, the entry point that calls main.
 legacy_stdio_definitions is required by anything that formats into a buffer:
 the UCRT kept printf and fprintf as real exported symbols but turned sprintf,
 the whole v-family and the scanf family into header inlines over
 __stdio_common_*. A compiler that declares them as the ordinary functions C
 says they are - which cc1 does, correctly - has nothing to link against
 without it.


--------------------------------------------------------------------------------
 3.  POINT A PROJECT AT IT
--------------------------------------------------------------------------------

 Two properties do the whole substitution. Add them to any C .vcxproj, in a
 PropertyGroup after the Microsoft.Cpp.props import:

     <CLToolExe>cc1-as-cl.bat</CLToolExe>
     <CLToolPath>C:\path\to\repo\msvc\</CLToolPath>

 MSBuild then runs $(CLToolPath)\$(CLToolExe) wherever it would have run
 cl.exe. The trailing backslash on CLToolPath is required.

 The linker also needs telling, because a cc1 object carries no /defaultlib
 directives:

     <Link>
       <AdditionalDependencies>libcmt.lib;libucrt.lib;libvcruntime.lib;kernel32.lib;legacy_stdio_definitions.lib</AdditionalDependencies>
       <IgnoreAllDefaultLibraries>true</IgnoreAllDefaultLibraries>
     </Link>

 msvc\demo\hello.vcxproj is a complete, working example of exactly this. Copy
 it if that is easier than editing your own.


--------------------------------------------------------------------------------
 4.  BUILD AND RUN THE DEMO
--------------------------------------------------------------------------------

     msbuild msvc\demo\hello.vcxproj /p:Configuration=Release /p:Platform=x64
     msvc\demo\x64\Release\hello.exe

 Expected output:

     built by cc1 inside MSBuild
     DEMO reached the compiler: 1
     data model: int=4 long=4 ptr=8 long double=8
     struct: a=42 d=2.5 s=ok
     third=0.3333333333
     longjmp gave 5

 "DEMO reached the compiler" is worth reading rather than skipping: it proves
 the /D from the .vcxproj travelled through the response file MSBuild writes,
 through the shim, and into cc1. If it says "DEMO did not arrive", the shim is
 parsing arguments wrongly - see step 6.

 To watch what the shim decides, per file:

     set CC1_AS_CL_VERBOSE=1


--------------------------------------------------------------------------------
 5.  WHAT YOUR CODE HAS TO KEEP TO
--------------------------------------------------------------------------------

 This is a C90 compiler and the limits are real ones, not teething trouble.

 * C90. No // comments are fine (they are accepted), but no declarations after
   statements in the strict sense, no long long that you rely on being C99, and
   nothing from C11 or later. The compiler accepts a handful of extensions -
   see docs/STATUS.md, "What it accepts and C90 does not".

 * THE FIFTEEN SHIPPED HEADERS ONLY. cc1 has no system include path. It
   searches -I and lib\*.h, which are the fifteen headers C90 defines. So
   #include <stdio.h> finds this project's own, and #include <windows.h> will
   not build and never will.

   That is a shield as much as a limit: it is what stops the compiler meeting
   the 3,997 lines of __declspec and __attribute__ in a real platform header.

 * x64 only. There is no 32-bit backend.

 * C++ is passed straight to cl.exe. The shim only takes .c files; a .cpp in
   the same project compiles normally with Microsoft's compiler. So a mixed
   project works, with each language going to the compiler that can read it.


--------------------------------------------------------------------------------
 6.  WHEN SOMETHING GOES WRONG
--------------------------------------------------------------------------------

 "cc1-as-cl.bat exited with code 1" and above it "unknown option -d"
     The shim mistook a cl flag for one of cc1's. It matches /D, /I and /U
     case-sensitively for exactly this reason - /diagnostics:column begins with
     /d - so if you see this, a new flag has appeared that needs handling in
     cc1-as-cl.ps1. The fix goes in the argument loop there.

 "no cc1.exe at ..."
     Step 1 has not been done, or the shim is looking in the wrong place. It
     expects msvc\x64\Release\cc1.exe. Override with the CC1 environment
     variable:

         set CC1=D:\somewhere\cc1.exe

 "ml64 is not recognised" / "link is not recognised"
     You are not in a Developer Command Prompt. See step 0. This bites at BUILD
     time from a plain cmd window, and the message names the tool.

 unresolved external symbol mainCRTStartup, or sprintf
     The five libraries in step 3 are missing from the project's
     AdditionalDependencies, or IgnoreAllDefaultLibraries is not set.

 "fatal error: cannot open <windows.h>" or any platform header
     Expected, and permanent. See step 5.

 The build succeeds but no .exe appears
     The source files are probably not members of the target - a file can sit
     in the project tree without being in the Sources build phase, and MSBuild
     then compiles nothing, reports success and writes no program. Check that
     each .c appears in an <ItemGroup> as <ClCompile Include="..." />.


--------------------------------------------------------------------------------
 7.  TURNING IT OFF
--------------------------------------------------------------------------------

 Delete the two properties from step 3, or set them empty:

     <CLToolExe></CLToolExe>
     <CLToolPath></CLToolPath>

 The project builds with cl.exe again. Nothing is installed, no toolchain is
 modified, and no registry key is written - the substitution is two lines in a
 project file and lasts exactly as long as they do.


--------------------------------------------------------------------------------
 8.  WHAT IS IN THIS DIRECTORY
--------------------------------------------------------------------------------

 cc1.vcxproj        builds cc1.exe from ..\src with MSVC
 cc1.sln            solution wrapper, if you would rather open it in the IDE
 compat\unistd.h    the one header MSVC lacks; see the note inside it
 cc1-as-cl.bat      what MSBuild invokes in place of cl.exe
 cc1-as-cl.ps1      the translator: cl's command line into cc1's
 demo\hello.vcxproj a complete project wired up as in step 3
 demo\hello.c       C90 exercising setjmp, long double, structs and printf
 readme.txt         this file

 The equivalent for Xcode on macOS is tools\cc1-as-clang, and the two are the
 same idea: let the IDE believe it is running the compiler it expects.
