@echo off

if not exist build mkdir build

pushd build

cl /EHsc^
  /Zi^
  /DDEBUG_DIRECTX^
  /Fe"demo-hexagonal-plane.exe"^
  ..\src\main.cpp^
  /link user32.lib d3d12.lib dxgi.lib d3dcompiler.lib

popd
