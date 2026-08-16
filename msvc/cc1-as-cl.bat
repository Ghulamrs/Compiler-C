@echo off
rem Stand where cl.exe stands, and hand the C to cc1.
rem
rem MSBuild invokes the C compiler through the CLToolExe property, and what it
rem invokes has to be an executable or a batch file - it cannot be a PowerShell
rem script. So this is the two-line front door, and cc1-as-cl.ps1 beside it is
rem the translator. See the note at the top of that file.
rem
rem "%~dp0" is this batch file's own directory with a trailing backslash, which
rem is how the .ps1 is found whatever the working directory is.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0cc1-as-cl.ps1" %*
exit /b %ERRORLEVEL%
