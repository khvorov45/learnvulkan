#include "stdint.h"

#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "windows.h"

#include "vulkan/vulkan.h"
#include "vulkan/vulkan_win32.h"

#define true 1
#define false 0

#define assert(expr) if (!(expr)) { *((int*)0) = 0; }

#define zero(x) ZeroMemory(&x, sizeof(x))

typedef uint32_t u32;
typedef int32_t i32;
typedef size_t usize;
typedef intptr_t isize;
typedef int32_t b32;
typedef float f32;

typedef struct SwapChain {
    VkSwapchainKHR swapChain;
    u32 imageCount;
    VkCommandBuffer* commandBuffers;
} SwapChain;

static globalRunning = true;

LRESULT windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE: case WM_DESTROY: case WM_QUIT: {
        globalRunning = false;
    } break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

VkShaderModule
createShaderModule(char* filename, VkDevice device) {
    FILE* file = fopen(filename, "rb");
    fseek(file, 0, SEEK_END);
    i32 fileSize = ftell(file);
    void* contents = malloc(fileSize);
    fseek(file, 0, SEEK_SET);
    u32 read = fread(contents, fileSize, 1, file);
    assert(read == 1);
    fclose(file);
    VkShaderModuleCreateInfo createInfo;
    ZeroMemory(&createInfo, sizeof(VkShaderModuleCreateInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = (u32*)(contents);
    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(device, &createInfo, 0, &shaderModule);
    assert(result == VK_SUCCESS);
    return shaderModule;
}

SwapChain
createSwapChain(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkSurfaceKHR surface,
    VkPresentModeKHR presentMode,
    VkPipelineShaderStageCreateInfo* shaderStages,
    VkPipelineVertexInputStateCreateInfo* vertexInputInfo,
    VkPipelineInputAssemblyStateCreateInfo* inputAssembly,
    VkPipelineRasterizationStateCreateInfo* rasterizer,
    VkPipelineMultisampleStateCreateInfo* multisampling,
    VkPipelineColorBlendStateCreateInfo* colorBlending,
    VkPipelineLayout pipelineLayout,
    u32 graphicsQueueFamilyIndex,
    VkCommandPool commandPool
) {

    SwapChain swapChain;
    zero(swapChain);

    VkSurfaceFormatKHR surfaceFormat;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    {
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
    }

    {
        swapChain.imageCount = surfaceCapabilities.minImageCount + 1;
        assert(swapChain.imageCount <= surfaceCapabilities.maxImageCount);

        VkSwapchainCreateInfoKHR createInfo;
        ZeroMemory(&createInfo, sizeof(VkSwapchainCreateInfoKHR));
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = swapChain.imageCount;
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

        VkResult result = vkCreateSwapchainKHR(device, &createInfo, 0, &swapChain.swapChain);
        assert(result == VK_SUCCESS);
    }

    vkGetSwapchainImagesKHR(device, swapChain.swapChain, &swapChain.imageCount, 0);
    VkImage* swapChainImages = malloc(sizeof(VkImage) * swapChain.imageCount);
    vkGetSwapchainImagesKHR(device, swapChain.swapChain, &swapChain.imageCount, swapChainImages);
    VkImageView* swapChainImageViews = malloc(sizeof(VkImageView) * swapChain.imageCount);
    for (u32 imageIndex = 0; imageIndex < swapChain.imageCount; ++imageIndex) {
        VkImageViewCreateInfo createInfo;
        ZeroMemory(&createInfo, sizeof(VkImageViewCreateInfo));
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[imageIndex];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = surfaceFormat.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(device, &createInfo, 0, swapChainImageViews + imageIndex);
        assert(result == VK_SUCCESS);
    }

    VkViewport viewport;
    zero(viewport);
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (f32)surfaceCapabilities.currentExtent.width;
    viewport.height = (f32)surfaceCapabilities.currentExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    zero(scissor);
    scissor.extent = surfaceCapabilities.currentExtent;

    VkPipelineViewportStateCreateInfo viewportState;
    zero(viewportState);
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkAttachmentDescription colorAttachment;
    zero(colorAttachment);
    colorAttachment.format = surfaceFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef;
    zero(colorAttachmentRef);
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass;
    zero(subpass);
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPass renderPass;
    {
        VkSubpassDependency dependency;
        zero(dependency);
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo;
        zero(renderPassInfo);
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VkResult result = vkCreateRenderPass(device, &renderPassInfo, 0, &renderPass);
        assert(result == VK_SUCCESS);
    }

    VkGraphicsPipelineCreateInfo pipelineInfo;
    zero(pipelineInfo);
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = vertexInputInfo;
    pipelineInfo.pInputAssemblyState = inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = rasterizer;
    pipelineInfo.pMultisampleState = multisampling;
    pipelineInfo.pColorBlendState = colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VkPipeline graphicsPipeline;
    assert(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, 0, &graphicsPipeline) == VK_SUCCESS);

    VkFramebuffer* swapChainFramebuffers = malloc(sizeof(VkFramebuffer) * swapChain.imageCount);
    for (u32 index = 0; index < swapChain.imageCount; index++) {
        VkImageView attachments[] = { swapChainImageViews[index] };
        VkFramebufferCreateInfo framebufferInfo;
        zero(framebufferInfo);
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = surfaceCapabilities.currentExtent.width;
        framebufferInfo.height = surfaceCapabilities.currentExtent.height;
        framebufferInfo.layers = 1;
        VkResult result = vkCreateFramebuffer(device, &framebufferInfo, 0, swapChainFramebuffers + index);
        assert(result == VK_SUCCESS);
    }

    swapChain.commandBuffers = malloc(sizeof(VkCommandBuffer) * swapChain.imageCount);
    VkCommandBufferAllocateInfo allocInfo;
    zero(allocInfo);
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = swapChain.imageCount;
    assert(vkAllocateCommandBuffers(device, &allocInfo, swapChain.commandBuffers) == VK_SUCCESS);

    for (size_t index = 0; index < swapChain.imageCount; index++) {
        VkCommandBufferBeginInfo beginInfo;
        zero(beginInfo);
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = 0;
        assert(vkBeginCommandBuffer(swapChain.commandBuffers[index], &beginInfo) == VK_SUCCESS);

        VkRenderPassBeginInfo renderPassInfo;
        zero(renderPassInfo);
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[index];
        renderPassInfo.renderArea.offset.x = 0;
        renderPassInfo.renderArea.offset.y = 0;
        renderPassInfo.renderArea.extent = surfaceCapabilities.currentExtent;

        VkClearValue clearColor = { {{0.01f, 0.01f, 0.01f, 1.0f}} };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(swapChain.commandBuffers[index], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(swapChain.commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        vkCmdDraw(swapChain.commandBuffers[index], 3, 1, 0, 0);

        vkCmdEndRenderPass(swapChain.commandBuffers[index]);

        assert(vkEndCommandBuffer(swapChain.commandBuffers[index]) == VK_SUCCESS);
    }

    return swapChain;
}

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
    windowClass.lpfnWndProc = windowProc;
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
    VkPresentModeKHR presentMode;
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

    VkCommandPool commandPool;
    VkCommandPoolCreateInfo poolInfo;
    zero(poolInfo);
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    poolInfo.flags = 0;
    assert(vkCreateCommandPool(device, &poolInfo, 0, &commandPool) == VK_SUCCESS);

    VkShaderModule vertShaderModule = createShaderModule("build/vert.spv", device);
    VkShaderModule fragShaderModule = createShaderModule("build/frag.spv", device);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo;
    zero(vertShaderStageInfo);
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo;
    zero(fragShaderStageInfo);
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    zero(vertexInputInfo);
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    zero(inputAssembly);
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterizer;
    zero(rasterizer);
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling;
    zero(multisampling);
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    zero(colorBlendAttachment);
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending;
    zero(colorBlending);
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineDynamicStateCreateInfo dynamicState;
    zero(dynamicState);
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineLayout pipelineLayout;
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo;
        zero(pipelineLayoutInfo);
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, 0, &pipelineLayout);
        assert(result == VK_SUCCESS);
    }

    SwapChain swapChain = createSwapChain(
        physicalDevice,
        device,
        surface,
        presentMode,
        shaderStages,
        &vertexInputInfo,
        &inputAssembly,
        &rasterizer,
        &multisampling,
        &colorBlending,
        pipelineLayout,
        graphicsQueueFamilyIndex,
        commandPool
    );

#define MAX_FRAMES_IN_FLIGHT 2
    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];
    VkFence* imagesInFlight = malloc(sizeof(VkFence) * swapChain.imageCount);
    VkSemaphoreCreateInfo semaphoreInfo;
    zero(semaphoreInfo);
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo;
    zero(fenceInfo);
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (u32 index = 0; index < MAX_FRAMES_IN_FLIGHT; index++) {
        assert(vkCreateSemaphore(device, &semaphoreInfo, 0, imageAvailableSemaphores + index) == VK_SUCCESS);
        assert(vkCreateSemaphore(device, &semaphoreInfo, 0, renderFinishedSemaphores + index) == VK_SUCCESS);
        assert(vkCreateFence(device, &fenceInfo, 0, inFlightFences + index) == VK_SUCCESS);
    }
    for (u32 index = 0; index < swapChain.imageCount; ++index) {
        imagesInFlight[index] = VK_NULL_HANDLE;
    }

    //
    //
    //

    ShowWindow(window, SW_SHOWMINIMIZED);
    ShowWindow(window, SW_SHOWNORMAL);

    u32 currentFrame = 0;

    while (globalRunning) {
        MSG message;
        while (PeekMessageW(&message, window, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        vkWaitForFences(device, 1, inFlightFences + currentFrame, VK_TRUE, UINT64_MAX);

        u32 imageIndex;
        vkAcquireNextImageKHR(device, swapChain.swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, imagesInFlight + imageIndex, VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        VkSubmitInfo submitInfo;
        zero(submitInfo);
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &swapChain.commandBuffers[imageIndex];
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        vkResetFences(device, 1, inFlightFences + currentFrame);
        assert(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) == VK_SUCCESS);

        VkPresentInfoKHR presentInfo;
        zero(presentInfo);
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = { swapChain.swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = 0;
        vkQueuePresentKHR(graphicsQueue, &presentInfo);

        ++currentFrame;
        if (currentFrame == MAX_FRAMES_IN_FLIGHT) {
            currentFrame = 0;
        }
    }

    vkQueueWaitIdle(graphicsQueue);

    return 0;
}
