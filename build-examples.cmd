@echo off
setlocal enabledelayedexpansion

for /d %%D in (examples\*) do (
    if exist "%%D\compile.cmd" (
        echo Compile %%D\compile.cmd
        pushd "%%D"
        call compile.cmd
        popd
    ) else if exist "%%D\demo-compile.cmd" (
        echo Compile %%D\demo-compile.cmd
        pushd "%%D"
        call demo-compile.cmd
        popd
    ) else (
        echo %%D: Compile script not found, skipping.
    )
)

endlocal

timeout 5