@echo off

set proj=objviewer
set bdir=%cd%\build

cls

if not exist %bdir% (mkdir %bdir%)

pushd %bdir%

cl ..\objviewer.cpp -DDEBUG -Fe%proj%.exe -std:c++20 -Zi -FC -GR- -nologo -EHa- -link user32.lib

popd