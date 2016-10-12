@rem
@rem If necessary, do setlocal.
@rem Then start the build.
@rem

@if "%build_level%"=="" setlocal
@call start_build %1 %2 %3


@rem
@rem Choose debug or release build
@rem
if "%build_type%"=="debug" goto build_debug
goto build_release


:build_release

%MSDEV% dlls/hl.dsp %CONFIG%"hl - Win32 Release HL2" %build_target%
if errorlevel 1 set BUILD_ERROR=1

goto done


:build_debug

%MSDEV% dlls/hl.dsp %CONFIG%"hl - Win32 Debug HL2" %build_target%
if errorlevel 1 set BUILD_ERROR=1

goto done


@rem
@rem All done
@rem
:done
@rem copy ..\hl2\bin\server.dll ..\testmod\bin\
call end_build