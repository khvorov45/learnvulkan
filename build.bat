pushd build
cl -Od -Z7 -nologo -TC -I%VK_SDK_PATH%/include ../code/main.c -link -LIBPATH:%VK_SDK_PATH%/Lib User32.lib Gdi32.lib vulkan-1.lib
popd
echo done
