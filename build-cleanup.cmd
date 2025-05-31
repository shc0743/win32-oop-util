@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0" || exit /b 1

for /r %%d in (.) do (
    del /q "%%d\*.exe" "%%d\*.ilk" "%%d\*.obj" "%%d\*.pdb" 2>nul
)