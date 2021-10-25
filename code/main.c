#include "stdint.h"

#include "windows.h"

#include "vulkan/vulkan.h"
#include "vulkan/vulkan_win32.h"

#define true 1
#define false 0

typedef uint32_t u32;
typedef int32_t i32;
typedef size_t usize;
typedef intptr_t isize;
typedef int32_t b32;
typedef float f32;

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nShowCmd
) {
    wchar_t* applicationName = L"LearnVulkan";

    WNDCLASSEXW windowClass;
    ZeroMemory(&windowClass, sizeof(WNDCLASSEXW));
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursorW(0, (LPWSTR)IDC_ARROW);;
    windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    windowClass.lpszClassName = applicationName;

    RegisterClassExW(&windowClass);

    i32 windowWidth = 1280;
    i32 windowHeight = 720;

    HWND window = CreateWindowExW(
        0,
        applicationName,
        applicationName,
        WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowWidth,
        windowHeight,
        0,
        0,
        hInstance,
        0
    );

    //
    //
    //

    VkApplicationInfo appInfo;
    ZeroMemory(&appInfo, sizeof(VkApplicationInfo));
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo;
    ZeroMemory(&createInfo, sizeof(VkInstanceCreateInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    char* extensionNames[2];
    extensionNames[0] = VK_KHR_SURFACE_EXTENSION_NAME;
    extensionNames[1] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
    createInfo.ppEnabledExtensionNames = extensionNames;
    createInfo.enabledLayerCount = 1;
    char* layerNames[1];
    layerNames[0] = "VK_LAYER_KHRONOS_validation";
    createInfo.ppEnabledLayerNames = layerNames;

    VkInstance vulkanInstance;
    VkResult createInstanceResult = vkCreateInstance(&createInfo, 0, &vulkanInstance);

    uint32_t deviceCount = 1;
    VkPhysicalDevice physicalDevice;
    VkResult enum_devices_result = vkEnumeratePhysicalDevices(vulkanInstance, &deviceCount, &physicalDevice);
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
    uint32_t queueFamilyCount = 1;
    VkQueueFamilyProperties queueFamilyProperties;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, &queueFamilyProperties);

    //
    //
    //

    ShowWindow(window, SW_SHOWMINIMIZED);
    ShowWindow(window, SW_SHOWNORMAL);

    for (;;) {
        MSG message;
        while (PeekMessageW(&message, window, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    return 0;
}
