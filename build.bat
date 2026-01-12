@echo off
setlocal 
cd /d "%~dp0"

if not exist build mkdir build
pushd build

set COMMON_DEFINE=-D_CRT_SECURE_NO_WARNINGS -D__DEBUG=1 -D__MSVC=1
set WARNING_FLAGS=-wd4100 -wd4505 -wd4701 -wd4201
set VENDOR_DIR=..\src\vendor
set CCFLAGS=-std:c++17 -nologo -arch:AVX2 -Z7 -W4 %WARNING_FLAGS% -I%VENDOR_DIR%

:: Windows ::
set WINDOWS_LIB=user32.lib gdi32.lib
call cl %CCFLAGS% %COMMON_DEFINE% -Od ..\src\win32.cpp -Fe:vk.exe /link %WINDOWS_LIB%

:: Vulkan ::
set VK_INCLUDE_DIR=C:\\VulkanSDK\\1.4.335.0\\Include
set VK_LIB_DIR=C:\VulkanSDK\1.4.335.0\Lib
set VK_LIB=vulkan-1.lib user32.lib
set VK_EXPORT=-EXPORT:win32_load_renderer -EXPORT:win32_end_frame
call cl %CCFLAGS% %COMMON_DEFINE% -I%VK_INCLUDE_DIR% -Od -LD ..\src\win32_vulkan.cpp -Fe:vulkan.dll /link -libpath:%VK_LIB_DIR% %VK_LIB% %VK_EXPORT% 

popd
