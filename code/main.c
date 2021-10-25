#include "stdint.h"

#include "stdlib.h"
#include "string.h"

#include "windows.h"

#include "vulkan/vulkan.h"
#include "vulkan/vulkan_win32.h"

#define true 1
#define false 0

#define assert(expr) if (!(expr)) { *((int*)0) = 0; }

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

    VkInstance vulkanInstance;
    {
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

        VkResult result = vkCreateInstance(&createInfo, 0, &vulkanInstance);
        assert(result == VK_SUCCESS);
    }

    VkSurfaceKHR surface;
    {
        VkWin32SurfaceCreateInfoKHR createInfo;
        ZeroMemory(&createInfo, sizeof(VkWin32SurfaceCreateInfoKHR));
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = window;
        createInfo.hinstance = hInstance;
        VkResult result = vkCreateWin32SurfaceKHR(vulkanInstance, &createInfo, 0, &surface);
        assert(result == VK_SUCCESS);
    }

    VkPhysicalDevice physicalDevice;
    u32 graphicsQueueFamilyIndex = 0;
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    {
        uint32_t deviceCount = 1;
        VkResult enum_devices_result = vkEnumeratePhysicalDevices(vulkanInstance, &deviceCount, &physicalDevice);
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
        uint32_t queueFamilyCount = 1;
        VkQueueFamilyProperties queueFamilyProperties;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, &queueFamilyProperties);
        assert(queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT);
        b32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsQueueFamilyIndex, surface, &presentSupport);
        assert(presentSupport);

        b32 swapChainSupported = false;
        u32 extensionCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, 0, &extensionCount, 0);
        VkExtensionProperties* extensions = malloc(extensionCount * sizeof(VkExtensionProperties));
        for (u32 extensionIndex = 0; extensionIndex < extensionCount; ++extensionIndex) {
            VkExtensionProperties* extension = extensions + extensionIndex;
            if (strcmp(extension->extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
                swapChainSupported = true;
                break;
            }
        }
        assert(swapChainSupported);
        free(extensions);

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
        assert(surfaceCapabilities.currentExtent.width != UINT32_MAX);

        u32 formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, 0);
        VkSurfaceFormatKHR* formats = malloc(formatCount * sizeof(VkSurfaceFormatKHR));
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats);
        assert(formatCount > 0);
        b32 formatFound = false;
        for (u32 formatIndex = 0; formatIndex < formatCount; formatIndex++) {
            VkSurfaceFormatKHR format = formats[formatIndex];
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surfaceFormat = format;
                formatFound = true;
                break;
            }
        }
        assert(formatFound);

        u32 presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, 0);
        VkPresentModeKHR* presentModes = malloc(presentModeCount * sizeof(VkPresentModeKHR));
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes);
        assert(presentModeCount > 0);
        b32 presentModeFound = false;
        for (u32 presentModeIndex = 0; presentModeIndex < presentModeCount; presentModeIndex++) {
            VkPresentModeKHR thisPresentMode = presentModes[presentModeIndex];
            if (thisPresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = thisPresentMode;
                presentModeFound = true;
                break;
            }
        }
        assert(presentModeFound);
    }

    VkDevice device;
    {
        VkDeviceQueueCreateInfo queueCreateInfo;
        ZeroMemory(&queueCreateInfo, sizeof(VkDeviceQueueCreateInfo));
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        f32 queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures;
        ZeroMemory(&deviceFeatures, sizeof(VkPhysicalDeviceFeatures));

        VkDeviceCreateInfo createInfo;
        ZeroMemory(&createInfo, sizeof(VkDeviceCreateInfo));
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = 1;
        char* extensions[1];
        extensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        createInfo.ppEnabledExtensionNames = extensions;

        VkResult result = vkCreateDevice(physicalDevice, &createInfo, 0, &device);
        assert(result == VK_SUCCESS);
    }

    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);

    VkSwapchainKHR swapChain;
    {
        u32 imageCount = surfaceCapabilities.minImageCount + 1;
        assert(imageCount <= surfaceCapabilities.maxImageCount);

        VkSwapchainCreateInfoKHR createInfo;
        ZeroMemory(&createInfo, sizeof(VkSwapchainCreateInfoKHR));
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = surfaceCapabilities.currentExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = surfaceCapabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VkResult result = vkCreateSwapchainKHR(device, &createInfo, 0, &swapChain);
        assert(result == VK_SUCCESS);
    }

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
