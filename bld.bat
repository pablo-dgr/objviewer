@echo off

set proj=objviewer
set bdir=%cd%\build
set idir=%cd%\libs\include

cls

if not exist %bdir% (mkdir %bdir%)

pushd %bdir%

cl ..\objviewer.cpp -I%idir% -DDEBUG -Fe%proj%.exe -std:c++20 -Zi -FC -GR- -nologo -EHa- -link user32.lib d3d11.lib d3dcompiler.lib

popd