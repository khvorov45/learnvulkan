pushd build
glslc ../code/shader.vert -o vert.spv
glslc ../code/shader.frag -o frag.spv
cl -Od -Z7 -nologo -TC -W3 -I%VK_SDK_PATH%/include ../code/main.c -link -LIBPATH:%VK_SDK_PATH%/Lib User32.lib Gdi32.lib vulkan-1.lib
popd
echo done
