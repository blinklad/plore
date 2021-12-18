@echo off

pushd "%~dp0"
IF NOT EXIST .\build mkdir .\build
pushd .\build

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

SET CommonCompilerFlags= -DEBUG:full -nologo -fp:fast -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4146 -wd4201 -wd4100 -wd4189 -wd4505 -wd4244 -wd4456 -wd4100 -wd4013  -wd4996 -wd4204 -wd4221 -wd4116 -DPLORE_INTERNAL=1 -DPLORE_WINDOWS=1 -FC -Z7
SET LinkerFlags= -DEBUG:full -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib opengl32.lib glu32.lib ksuser.lib shlwapi.lib Shell32.lib

del *.pdb > NUL 2> NUL
echo "Waiting for PDB" > lock.tmp
cl %CommonCompilerFlags% ..\code\plore.c -Fmplore.map -LD /link -incremental:no -opt:ref -PDB:plore_%random%.pdb
del lock.tmp

cl %CommonCompilerFlags% ..\code\win32_plore.c -Fmwin32_plore.map /link %LinkerFlags%

popd
