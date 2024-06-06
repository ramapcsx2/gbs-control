@ECHO OFF

SET ROOT=%CD%
SET DDIR=%ROOT%\data
SET SDIR=%ROOT%\public\scripts

IF not exist %DDIR% (
    mkdir %DDIR%
    ECHO output directory created: %DDIR%
    ECHO
)

@REM CD %SDIR%
node public\scripts\build.js
%SDIR%\bin\gzip.exe -c9 "%DDIR%\webui.html" > "%DDIR%\__index"
:: cleanup
DEL "%DDIR%\webui.html"

EXIT 0