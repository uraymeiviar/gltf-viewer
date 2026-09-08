@ECHO OFF
if not exist "%2" mkdir "%2"
xcopy "%1" "%2" /e /d /i /y /q
@ECHO ON
@if not errorlevel 0 echo "Failed to copy qt files" 
