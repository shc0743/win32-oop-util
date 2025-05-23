@echo off
setlocal enabledelayedexpansion

for /d %%D in (examples\*) do (
    echo ======
    set "compile_success="
    if exist "%%D\compile.cmd" (
        echo Compile %%D\compile.cmd
        pushd "%%D"
        call compile.cmd
        if errorlevel 1 (
            echo Error: %%D\compile.cmd failed.
            pause
            exit /b 1
        ) else (
            set "compile_success=1"
        )
        popd
    ) else if exist "%%D\demo-compile.cmd" (
        echo Compile %%D\demo-compile.cmd
        pushd "%%D"
        call demo-compile.cmd
        if errorlevel 1 (
            echo Error: %%D\demo-compile.cmd failed.
            pause
            exit /b 1
        ) else (
            set "compile_success=1"
        )
        popd
    ) else (
        echo %%D: Compile script not found, skipping.
    )
)

endlocal

timeout 5