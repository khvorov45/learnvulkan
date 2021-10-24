pushd build
cl -Od -Z7 -nologo -TC -I%VK_SDK_PATH%/include ../code/main.c -link User32.lib Gdi32.lib
popd
echo done
