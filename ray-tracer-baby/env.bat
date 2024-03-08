
SET VS2022INSTALLDIR=D:\Program Files\Microsoft Visual Studio\2022\Community
CALL "C:\Program Files (x86)\Intel\oneAPI\compiler\latest\env\vars.bat"

SET "VCPKG_PATH=D:\dev\vcpkg\installed\x64-windows"
SET "EMBREE_PATH=D:\embree-3.13.5"

SET "INCLUDE=%VCPKG_PATH%\include;%EMBREE_PATH%\include;%INCLUDE%"
SET "LIB=%VCPKG_PATH%\lib;%EMBREE_PATH%\lib;C:\Program Files (x86)\Intel\oneAPI\ipp\2021.10\lib;%LIB%"
