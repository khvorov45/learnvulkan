#include "stdint.h"
#include "stddef.h"

#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "math.h"

#include "windows.h"
#include "windowsx.h"

#include "vulkan/vulkan.h"
#include "vulkan/vulkan_win32.h"

#include "msg.c"

#define true 1
#define false 0

#define TAU32 6.28318530717958647692f
#define assert(expr) if (!(expr)) { *((int*)0) = 0; }
#define zero(x) ZeroMemory(&x, sizeof(x))
#define arrayCount(arr) sizeof(arr) / sizeof(arr[0])

typedef uint32_t u32;
typedef uint64_t u64;
typedef uint16_t u16;
typedef int32_t i32;
typedef size_t usize;
typedef intptr_t isize;
typedef int32_t b32;
typedef float f32;

typedef struct v2 {
    f32 x;
    f32 y;
} v2;

typedef union v3 {
    struct {
        f32 x;
        f32 y;
        f32 z;
    };
    struct {
        f32 r;
        f32 g;
        f32 b;
    };
} v3;

typedef struct Vertex {
    v3 pos;
    v3 color;
    v2 texture;
} Vertex;

typedef struct m4 {
    f32 m0, m4, m8, m12;
    f32 m1, m5, m9, m13;
    f32 m2, m6, m10, m14;
    f32 m3, m7, m11, m15;
} m4;

typedef struct Rect {
    v3 topleft;
    v3 bottomright;
    v2 textopleft;
    v2 texbottomright;
} Rect;

typedef struct VertexIndexBuffer {
    Vertex* vertexData;
    u16* indexData;
    u32 curVertex;
    u32 curIndex;
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    VkDeviceMemory vertexMemory;
    VkDeviceMemory indexMemory;
} VertexIndexBuffer;

typedef struct UniformBufferObject {
    m4 model;
    m4 view;
    m4 proj;
} UniformBufferObject;

typedef struct SwapChain {
    VkSwapchainKHR swapChain;
    u32 imageCount;
    v2 surfaceDim;
    VkCommandBuffer* commandBuffers;
    VkFramebuffer* framebuffers;
    VkPipeline graphicsPipeline;
    VkRenderPass renderPass;
    VkImageView* imageViews;
    VkBuffer* uniformBuffers;
    VkDeviceMemory* uniformBuffersMemory;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout* layouts;
    VkDescriptorSet* descriptorSets;
    VertexIndexBuffer* vertexIndexBuffer;
} SwapChain;

static b32 globalRunning = true;
static void* globalMainFibre = 0;
static void* globalPollEventsFibre = 0;

v3
v3new(f32 x, f32 y, f32 z) {
    v3 result = { .x = x, .y = y, .z = z };
    return result;
}

m4
m4transpose(m4 mat) {
    // Taken from
    // https://github.com/raysan5/raylib/blob/master/src/raymath.h

    m4 result = { 0 };

    result.m0 = mat.m0;
    result.m1 = mat.m4;
    result.m2 = mat.m8;
    result.m3 = mat.m12;
    result.m4 = mat.m1;
    result.m5 = mat.m5;
    result.m6 = mat.m9;
    result.m7 = mat.m13;
    result.m8 = mat.m2;
    result.m9 = mat.m6;
    result.m10 = mat.m10;
    result.m11 = mat.m14;
    result.m12 = mat.m3;
    result.m13 = mat.m7;
    result.m14 = mat.m11;
    result.m15 = mat.m15;

    return result;
}

m4
m4identity() {
    // Taken from
    // https://github.com/raysan5/raylib/blob/master/src/raymath.h

    m4 result = { 1.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 1.0f, 0.0f,
                  0.0f, 0.0f, 0.0f, 1.0f };
    return result;
}

m4
m4translation(f32 x, f32 y, f32 z) {
    // Taken from
    // https://github.com/raysan5/raylib/blob/master/src/raymath.h

    m4 result = { 1.0f, 0.0f, 0.0f, x,
                  0.0f, 1.0f, 0.0f, y,
                  0.0f, 0.0f, 1.0f, z,
                  0.0f, 0.0f, 0.0f, 1.0f };
    return result;
}

m4
m4rotationZ(f32 radians) {
    // Taken from
    // https://github.com/raysan5/raylib/blob/master/src/raymath.h

    m4 result = { 1.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 1.0f, 0.0f,
                  0.0f, 0.0f, 0.0f, 1.0f };

    f32 cosres = cosf(radians);
    f32 sinres = sinf(radians);

    result.m0 = cosres;
    result.m1 = sinres;
    result.m4 = -sinres;
    result.m5 = cosres;

    return result;
}

m4
m4rotation(v3 axis, f32 radians) {
    // Taken from
    // https://github.com/raysan5/raylib/blob/master/src/raymath.h

    m4 result = { 0 };

    f32 x = axis.x, y = axis.y, z = axis.z;

    f32 lengthSquared = x * x + y * y + z * z;

    if ((lengthSquared != 1.0f) && (lengthSquared != 0.0f)) {
        f32 ilength = 1.0f / sqrtf(lengthSquared);
        x *= ilength;
        y *= ilength;
        z *= ilength;
    }

    f32 sinres = sinf(radians);
    f32 cosres = cosf(radians);
    f32 t = 1.0f - cosres;

    result.m0 = x * x * t + cosres;
    result.m1 = y * x * t + z * sinres;
    result.m2 = z * x * t - y * sinres;
    result.m3 = 0.0f;

    result.m4 = x * y * t - z * sinres;
    result.m5 = y * y * t + cosres;
    result.m6 = z * y * t + x * sinres;
    result.m7 = 0.0f;

    result.m8 = x * z * t + y * sinres;
    result.m9 = y * z * t - x * sinres;
    result.m10 = z * z * t + cosres;
    result.m11 = 0.0f;

    result.m12 = 0.0f;
    result.m13 = 0.0f;
    result.m14 = 0.0f;
    result.m15 = 1.0f;

    return result;
}

m4
m4scale(f32 x, f32 y, f32 z) {
    // Taken from
    // https://github.com/raysan5/raylib/blob/master/src/raymath.h

    m4 result = { x, 0.0f, 0.0f, 0.0f,
                  0.0f, y, 0.0f, 0.0f,
                  0.0f, 0.0f, z, 0.0f,
                  0.0f, 0.0f, 0.0f, 1.0f };
    return result;
}

m4
m4lookat(v3 eye, v3 target, v3 up) {
    // Taken from
    // https://github.com/raysan5/raylib/blob/master/src/raymath.h

    m4 result = { 0 };

    f32 length = 0.0f;
    f32 ilength = 0.0f;

    // Vector3Subtract(eye, target)
    v3 vz = { eye.x - target.x, eye.y - target.y, eye.z - target.z };

    // Vector3Normalize(vz)
    v3 v = vz;
    length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length == 0.0f) length = 1.0f;
    ilength = 1.0f / length;
    vz.x *= ilength;
    vz.y *= ilength;
    vz.z *= ilength;

    // Vector3CrossProduct(up, vz)
    v3 vx = { up.y * vz.z - up.z * vz.y, up.z * vz.x - up.x * vz.z, up.x * vz.y - up.y * vz.x };

    // Vector3Normalize(x)
    v = vx;
    length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length == 0.0f) length = 1.0f;
    ilength = 1.0f / length;
    vx.x *= ilength;
    vx.y *= ilength;
    vx.z *= ilength;

    // Vector3CrossProduct(vz, vx)
    v3 vy = { vz.y * vx.z - vz.z * vx.y, vz.z * vx.x - vz.x * vx.z, vz.x * vx.y - vz.y * vx.x };

    result.m0 = vx.x;
    result.m1 = vy.x;
    result.m2 = vz.x;
    result.m3 = 0.0f;
    result.m4 = vx.y;
    result.m5 = vy.y;
    result.m6 = vz.y;
    result.m7 = 0.0f;
    result.m8 = vx.z;
    result.m9 = vy.z;
    result.m10 = vz.z;
    result.m11 = 0.0f;
    result.m12 = -(vx.x * eye.x + vx.y * eye.y + vx.z * eye.z);   // Vector3DotProduct(vx, eye)
    result.m13 = -(vy.x * eye.x + vy.y * eye.y + vy.z * eye.z);   // Vector3DotProduct(vy, eye)
    result.m14 = -(vz.x * eye.x + vz.y * eye.y + vz.z * eye.z);   // Vector3DotProduct(vz, eye)
    result.m15 = 1.0f;

    return result;
}

m4
m4perspective(f32 fovYRadians, f32 aspect, f32 nearPlane, f32 farPlane) {
    // Taken from
    // https://github.com/raysan5/raylib/blob/master/src/raymath.h

    m4 result = { 0 };

    f32 top = nearPlane * tanf(fovYRadians * 0.5f);
    f32 bottom = -top;
    f32 right = top * aspect;
    f32 left = -right;

    f32 rl = right - left;
    f32 tb = top - bottom;
    f32 fn = farPlane - nearPlane;

    result.m0 = (nearPlane * 2.0f) / rl;
    result.m5 = (nearPlane * 2.0f) / tb;
    result.m8 = (right + left) / rl;
    result.m9 = (top + bottom) / tb;
    result.m10 = -(farPlane + nearPlane) / fn;
    result.m11 = -1.0f;
    result.m14 = -(farPlane * nearPlane * 2.0f) / fn;

    return result;
}


m4
m4mul(m4 left, m4 right) {
    // Taken from
    // https://github.com/raysan5/raylib/blob/master/src/raymath.h

    m4 result = { 0 };

    result.m0 = left.m0 * right.m0 + left.m1 * right.m4 + left.m2 * right.m8 + left.m3 * right.m12;
    result.m1 = left.m0 * right.m1 + left.m1 * right.m5 + left.m2 * right.m9 + left.m3 * right.m13;
    result.m2 = left.m0 * right.m2 + left.m1 * right.m6 + left.m2 * right.m10 + left.m3 * right.m14;
    result.m3 = left.m0 * right.m3 + left.m1 * right.m7 + left.m2 * right.m11 + left.m3 * right.m15;
    result.m4 = left.m4 * right.m0 + left.m5 * right.m4 + left.m6 * right.m8 + left.m7 * right.m12;
    result.m5 = left.m4 * right.m1 + left.m5 * right.m5 + left.m6 * right.m9 + left.m7 * right.m13;
    result.m6 = left.m4 * right.m2 + left.m5 * right.m6 + left.m6 * right.m10 + left.m7 * right.m14;
    result.m7 = left.m4 * right.m3 + left.m5 * right.m7 + left.m6 * right.m11 + left.m7 * right.m15;
    result.m8 = left.m8 * right.m0 + left.m9 * right.m4 + left.m10 * right.m8 + left.m11 * right.m12;
    result.m9 = left.m8 * right.m1 + left.m9 * right.m5 + left.m10 * right.m9 + left.m11 * right.m13;
    result.m10 = left.m8 * right.m2 + left.m9 * right.m6 + left.m10 * right.m10 + left.m11 * right.m14;
    result.m11 = left.m8 * right.m3 + left.m9 * right.m7 + left.m10 * right.m11 + left.m11 * right.m15;
    result.m12 = left.m12 * right.m0 + left.m13 * right.m4 + left.m14 * right.m8 + left.m15 * right.m12;
    result.m13 = left.m12 * right.m1 + left.m13 * right.m5 + left.m14 * right.m9 + left.m15 * right.m13;
    result.m14 = left.m12 * right.m2 + left.m13 * right.m6 + left.m14 * right.m10 + left.m15 * right.m14;
    result.m15 = left.m12 * right.m3 + left.m13 * right.m7 + left.m14 * right.m11 + left.m15 * right.m15;

    return result;
}

void
printMsgName(u32 msg_code) {
    char* name = findMsgName(msg_code);
    if (name) {
        char buffer[64];
        snprintf(buffer, 64, "%s\n", name);
        OutputDebugString(buffer);
    } else {
        OutputDebugString("MSG UNRECOGNIZED\n");
    }
}

LRESULT windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE: case WM_DESTROY: case WM_QUIT: {
        globalRunning = false;
    } break;
    case WM_ERASEBKGND: {
        return 1; // NOTE(sen) Prevents flickering
    } break;
    case WM_NCCALCSIZE: {
        return 0; // NOTE(sen) Removes window decorations
    } break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

Rect
moveRect(Rect rect, f32 byx, f32 byy, f32 byz) {
    Rect result = rect;
    result.topleft.x += byx;
    result.topleft.y += byy;
    result.topleft.z += byz;
    result.bottomright.x += byx;
    result.bottomright.y += byy;
    result.bottomright.z += byz;
    return result;
}

void
pushRect(VertexIndexBuffer* buffer, Rect rect) {

    assert(rect.topleft.z == rect.bottomright.z);

    v3 black = { 0 };

    buffer->vertexData[buffer->curVertex].pos = rect.topleft;
    buffer->vertexData[buffer->curVertex].color = black;
    buffer->vertexData[buffer->curVertex].texture = rect.textopleft;

    v3 topright = rect.topleft;
    topright.x = rect.bottomright.x;

    v2 textopright = rect.textopleft;
    textopright.x = rect.texbottomright.x;

    buffer->vertexData[buffer->curVertex + 1].pos = topright;
    buffer->vertexData[buffer->curVertex + 1].color = black;
    buffer->vertexData[buffer->curVertex + 1].texture = textopright;

    v3 bottomleft = rect.bottomright;
    bottomleft.x = rect.topleft.x;

    v2 texbottomleft = rect.texbottomright;
    texbottomleft.x = rect.textopleft.x;

    buffer->vertexData[buffer->curVertex + 2].pos = bottomleft;
    buffer->vertexData[buffer->curVertex + 2].color = black;
    buffer->vertexData[buffer->curVertex + 2].texture = texbottomleft;

    buffer->vertexData[buffer->curVertex + 3].pos = rect.bottomright;
    buffer->vertexData[buffer->curVertex + 3].color = black;
    buffer->vertexData[buffer->curVertex + 3].texture = rect.texbottomright;

    buffer->indexData[buffer->curIndex + 0] = buffer->curVertex + 0;
    buffer->indexData[buffer->curIndex + 1] = buffer->curVertex + 1;
    buffer->indexData[buffer->curIndex + 2] = buffer->curVertex + 2;
    buffer->indexData[buffer->curIndex + 3] = buffer->curVertex + 1;
    buffer->indexData[buffer->curIndex + 4] = buffer->curVertex + 3;
    buffer->indexData[buffer->curIndex + 5] = buffer->curVertex + 2;

    buffer->curVertex += 4;
    buffer->curIndex += 6;
}

VkShaderModule
createShaderModule(char* filename, VkDevice device) {
    FILE* file;
    fopen_s(&file, filename, "rb");
    fseek(file, 0, SEEK_END);
    i32 fileSize = ftell(file);
    void* contents = malloc(fileSize);
    fseek(file, 0, SEEK_SET);
    usize read = fread(contents, fileSize, 1, file);
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

u32
findMemoryTypeIndex(
    VkPhysicalDevice physicalDevice,
    u32 memoryTypeBits,
    VkMemoryPropertyFlags properties
) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    i32 memIndex;
    {
        b32 found = false;
        for (u32 index = 0; index < memProperties.memoryTypeCount; index++) {
            b32 correctType = memoryTypeBits & (1 << index);
            b32 correctProperties = (memProperties.memoryTypes[index].propertyFlags & properties) == properties;
            if (correctType && correctProperties) {
                found = true;
                memIndex = index;
                break;
            }
        }
        assert(found);
    }
    return memIndex;
}

void
createBuffer(
    VkDevice device, VkPhysicalDevice physicalDevice,
    VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
    VkBuffer* buffer, VkDeviceMemory* bufferMemory
) {
    VkBufferCreateInfo bufferInfo;
    zero(bufferInfo);
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    {
        VkResult result = vkCreateBuffer(device, &bufferInfo, 0, buffer);
        assert(result == VK_SUCCESS);
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    i32 bufferMemIndex = findMemoryTypeIndex(physicalDevice, memRequirements.memoryTypeBits, properties);

    VkMemoryAllocateInfo allocInfo;
    zero(allocInfo);
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = bufferMemIndex;

    {
        VkResult result = vkAllocateMemory(device, &allocInfo, 0, bufferMemory);
        assert(result == VK_SUCCESS);
    }

    assert(vkBindBufferMemory(device, *buffer, *bufferMemory, 0) == VK_SUCCESS);
}

void
createMappedBuffer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkDeviceSize dataSize,
    VkCommandPool commandPool,
    VkQueue queue,
    VkBufferUsageFlags bufferUsage,
    VkBuffer* buffer,
    VkDeviceMemory* bufferMemory,
    void** gpuData
) {

    createBuffer(
        device, physicalDevice, dataSize,
        bufferUsage,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        buffer, bufferMemory
    );

    {
        VkResult result = vkMapMemory(device, *bufferMemory, 0, dataSize, 0, gpuData);
        assert(result == VK_SUCCESS);
    }
}

void
initSwapChain(
    SwapChain* swapChain,
    VkSwapchainKHR oldSwapChain,
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
    VkQueue graphicsQueue,
    VkCommandPool commandPool,
    VkDescriptorSetLayout descriptorSetLayout,
    VkImageView textureImageView,
    VkSampler textureSampler
) {
    ZeroMemory(swapChain, sizeof(SwapChain));

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
        free(formats);

        swapChain->surfaceDim.x = (f32)surfaceCapabilities.currentExtent.width;
        swapChain->surfaceDim.y = (f32)surfaceCapabilities.currentExtent.height;
    }

    {
        swapChain->imageCount = surfaceCapabilities.minImageCount + 1;
        assert(swapChain->imageCount <= surfaceCapabilities.maxImageCount);

        VkSwapchainCreateInfoKHR createInfo;
        ZeroMemory(&createInfo, sizeof(VkSwapchainCreateInfoKHR));
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = swapChain->imageCount;
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
        createInfo.oldSwapchain = oldSwapChain;

        VkResult result = vkCreateSwapchainKHR(device, &createInfo, 0, &swapChain->swapChain);
        assert(result == VK_SUCCESS);
    }

    vkGetSwapchainImagesKHR(device, swapChain->swapChain, &swapChain->imageCount, 0);
    VkImage* swapChainImages = malloc(sizeof(VkImage) * swapChain->imageCount);
    vkGetSwapchainImagesKHR(device, swapChain->swapChain, &swapChain->imageCount, swapChainImages);
    swapChain->imageViews = malloc(sizeof(VkImageView) * swapChain->imageCount);
    for (u32 imageIndex = 0; imageIndex < swapChain->imageCount; ++imageIndex) {
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

        VkResult result = vkCreateImageView(device, &createInfo, 0, swapChain->imageViews + imageIndex);
        assert(result == VK_SUCCESS);
    }
    free(swapChainImages);

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

        VkResult result = vkCreateRenderPass(device, &renderPassInfo, 0, &swapChain->renderPass);
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
    pipelineInfo.renderPass = swapChain->renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    assert(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, 0, &swapChain->graphicsPipeline) == VK_SUCCESS);

    swapChain->framebuffers = malloc(sizeof(VkFramebuffer) * swapChain->imageCount);
    for (u32 index = 0; index < swapChain->imageCount; index++) {
        VkImageView attachments[] = { swapChain->imageViews[index] };
        VkFramebufferCreateInfo framebufferInfo;
        zero(framebufferInfo);
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = swapChain->renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = surfaceCapabilities.currentExtent.width;
        framebufferInfo.height = surfaceCapabilities.currentExtent.height;
        framebufferInfo.layers = 1;
        VkResult result = vkCreateFramebuffer(device, &framebufferInfo, 0, swapChain->framebuffers + index);
        assert(result == VK_SUCCESS);
    }

    swapChain->uniformBuffers = malloc(sizeof(VkBuffer) * swapChain->imageCount);
    swapChain->uniformBuffersMemory = malloc(sizeof(VkDeviceMemory) * swapChain->imageCount);
    for (size_t index = 0; index < swapChain->imageCount; index++) {
        createBuffer(
            device, physicalDevice,
            sizeof(UniformBufferObject),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            swapChain->uniformBuffers + index, swapChain->uniformBuffersMemory + index
        );
    }

    VkDescriptorPoolSize poolSizes[2] = { 0 };
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = swapChain->imageCount;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = swapChain->imageCount;

    VkDescriptorPoolCreateInfo poolInfo = { 0 };
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = arrayCount(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = swapChain->imageCount;

    assert(vkCreateDescriptorPool(device, &poolInfo, 0, &swapChain->descriptorPool) == VK_SUCCESS);

    {
        swapChain->layouts = malloc(sizeof(VkDescriptorSetLayout) * swapChain->imageCount);
        for (u32 index = 0; index < swapChain->imageCount; index++) {
            swapChain->layouts[index] = descriptorSetLayout;
        }
        swapChain->descriptorSets = malloc(sizeof(VkDescriptorSet) * swapChain->imageCount);
        VkDescriptorSetAllocateInfo allocInfo;
        zero(allocInfo);
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = swapChain->descriptorPool;
        allocInfo.descriptorSetCount = swapChain->imageCount;
        allocInfo.pSetLayouts = swapChain->layouts;
        assert(vkAllocateDescriptorSets(device, &allocInfo, swapChain->descriptorSets) == VK_SUCCESS);
    }

    for (u32 index = 0; index < swapChain->imageCount; index++) {
        VkDescriptorBufferInfo bufferInfo;
        zero(bufferInfo);
        bufferInfo.buffer = swapChain->uniformBuffers[index];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo = { 0 };
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView;
        imageInfo.sampler = textureSampler;

        VkWriteDescriptorSet descriptorWrites[2] = { 0 };
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = swapChain->descriptorSets[index];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = swapChain->descriptorSets[index];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, arrayCount(descriptorWrites), descriptorWrites, 0, 0);
    }

    swapChain->commandBuffers = malloc(sizeof(VkCommandBuffer) * swapChain->imageCount);
    VkCommandBufferAllocateInfo allocInfo;
    zero(allocInfo);
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = swapChain->imageCount;
    assert(vkAllocateCommandBuffers(device, &allocInfo, swapChain->commandBuffers) == VK_SUCCESS);

    swapChain->vertexIndexBuffer = malloc(sizeof(VertexIndexBuffer) * swapChain->imageCount);

    for (u32 index = 0; index < swapChain->imageCount; index++) {

        VertexIndexBuffer* buf = swapChain->vertexIndexBuffer + index;

        buf->curIndex = 0;
        buf->curVertex = 0;

        createMappedBuffer(
            device, physicalDevice,
            sizeof(Vertex) * 1000,
            commandPool,
            graphicsQueue,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            &buf->vertexBuffer, &buf->vertexMemory,
            &buf->vertexData
        );

        createMappedBuffer(
            device, physicalDevice,
            sizeof(u16) * 1000,
            commandPool,
            graphicsQueue,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            &buf->indexBuffer, &buf->indexMemory,
            &buf->indexData
        );

    }

}

void
cmdTransitionLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        assert(!"unsupported layout transition");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, 0,
        0, 0,
        1, &barrier
    );
}

void
cleanupSwapChain(SwapChain* swapChain, VkDevice device, VkCommandPool commandPool) {
    vkDeviceWaitIdle(device);
    for (size_t index = 0; index < swapChain->imageCount; index++) {
        vkDestroyFramebuffer(device, swapChain->framebuffers[index], 0);
    }
    vkFreeCommandBuffers(device, commandPool, swapChain->imageCount, swapChain->commandBuffers);
    vkDestroyPipeline(device, swapChain->graphicsPipeline, 0);
    vkDestroyRenderPass(device, swapChain->renderPass, 0);
    for (size_t index = 0; index < swapChain->imageCount; index++) {
        vkDestroyImageView(device, swapChain->imageViews[index], 0);
        vkDestroyBuffer(device, swapChain->uniformBuffers[index], 0);
        vkFreeMemory(device, swapChain->uniformBuffersMemory[index], 0);
    }
    vkDestroySwapchainKHR(device, swapChain->swapChain, 0);
    vkDestroyDescriptorPool(device, swapChain->descriptorPool, 0);
    free(swapChain->imageViews);
    free(swapChain->framebuffers);
    free(swapChain->commandBuffers);
    free(swapChain->uniformBuffers);
    free(swapChain->uniformBuffersMemory);
    free(swapChain->layouts);
    free(swapChain->descriptorSets);

    for (size_t index = 0; index < swapChain->imageCount; index++) {

        VertexIndexBuffer* buf = swapChain->vertexIndexBuffer + index;

        vkUnmapMemory(device, buf->indexMemory);
        vkUnmapMemory(device, buf->vertexMemory);

        vkFreeMemory(device, buf->indexMemory, 0);
        vkFreeMemory(device, buf->vertexMemory, 0);

        vkDestroyBuffer(device, buf->vertexBuffer, 0);
        vkDestroyBuffer(device, buf->indexBuffer, 0);

    }

    free(swapChain->vertexIndexBuffer);

}

VkCommandBuffer
beginSingleTimeCommandBuffer(VkDevice device, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo = { 0 };
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo;
    zero(beginInfo);
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void
endSingleTimeCommandBuffer(VkCommandBuffer commandBuffer, VkDevice device, VkQueue queue, VkCommandPool commandPool) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo;
    zero(submitInfo);
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

int WINAPI
WinMain(
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
        VkDeviceQueueCreateInfo queueCreateInfo = { 0 };
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        f32 queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
        assert(deviceFeatures.samplerAnisotropy);

        VkDeviceCreateInfo createInfo = { 0 };
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
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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

    VkVertexInputBindingDescription bindingDescription;
    zero(bindingDescription);
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription posDescription;
    zero(posDescription);
    posDescription.binding = 0;
    posDescription.location = 0;
    posDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
    posDescription.offset = offsetof(Vertex, pos);

    VkVertexInputAttributeDescription colDescription;
    zero(colDescription);
    colDescription.binding = 0;
    colDescription.location = 1;
    colDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
    colDescription.offset = offsetof(Vertex, color);

    VkVertexInputAttributeDescription texDescription = { 0 };
    texDescription.binding = 0;
    texDescription.location = 2;
    texDescription.format = VK_FORMAT_R32G32_SFLOAT;
    texDescription.offset = offsetof(Vertex, texture);

    VkVertexInputAttributeDescription attDescriptions[] = {
        posDescription,
        colDescription,
        texDescription
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    zero(vertexInputInfo);
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = arrayCount(attDescriptions);
    vertexInputInfo.pVertexAttributeDescriptions = attDescriptions;

    u32 textureWidth = 2;
    u32 textureHeight = 2;
    u32 textureSize = textureWidth * textureHeight * sizeof(u32);
    u32* texture = malloc(textureSize);
    texture[0] = 0xFFFF0000;
    texture[1] = 0xFF00FF00;
    texture[2] = 0xFF0000FF;
    texture[3] = 0xFF000000;

    VkBuffer textureStagingBuffer;
    VkDeviceMemory textureStagingBufferMemory;
    createBuffer(
        device, physicalDevice,
        textureSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &textureStagingBuffer, &textureStagingBufferMemory
    );
    void* textureGpuData;
    vkMapMemory(device, textureStagingBufferMemory, 0, textureSize, 0, &textureGpuData);
    memcpy(textureGpuData, texture, textureSize);
    vkUnmapMemory(device, textureStagingBufferMemory);

    VkImageCreateInfo textureImageInfo = { 0 };
    textureImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    textureImageInfo.imageType = VK_IMAGE_TYPE_2D;
    textureImageInfo.extent.width = textureWidth;
    textureImageInfo.extent.height = textureHeight;
    textureImageInfo.extent.depth = 1;
    textureImageInfo.mipLevels = 1;
    textureImageInfo.arrayLayers = 1;
    textureImageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    textureImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    textureImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    textureImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    textureImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    textureImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VkImage textureImage;
    assert(vkCreateImage(device, &textureImageInfo, 0, &textureImage) == VK_SUCCESS);

    {
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, textureImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo = { 0 };
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryTypeIndex(
            physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        VkDeviceMemory textureImageMemory;
        assert(vkAllocateMemory(device, &allocInfo, 0, &textureImageMemory) == VK_SUCCESS);
        assert(vkBindImageMemory(device, textureImage, textureImageMemory, 0) == VK_SUCCESS);

        VkCommandBuffer commandBuffer = beginSingleTimeCommandBuffer(device, commandPool);

        cmdTransitionLayout(
            commandBuffer, textureImage, textureImageInfo.initialLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        VkBufferImageCopy region = { 0 };
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset.x = 0;
        region.imageOffset.y = 0;
        region.imageOffset.z = 0;
        region.imageExtent.width = textureWidth;
        region.imageExtent.height = textureHeight;
        region.imageExtent.depth = 1;

        vkCmdCopyBufferToImage(
            commandBuffer,
            textureStagingBuffer,
            textureImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        cmdTransitionLayout(
            commandBuffer, textureImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        endSingleTimeCommandBuffer(commandBuffer, device, graphicsQueue, commandPool);
    }

    VkImageView textureImageView;
    {
        VkImageViewCreateInfo viewInfo = { 0 };
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = textureImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = textureImageInfo.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        assert(vkCreateImageView(device, &viewInfo, 0, &textureImageView) == VK_SUCCESS)
    }

    VkSamplerCreateInfo samplerInfo = { 0 };
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    {
        VkPhysicalDeviceProperties properties = { 0 };
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    }
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    VkSampler textureSampler;
    assert(vkCreateSampler(device, &samplerInfo, 0, &textureSampler) == VK_SUCCESS);

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

    VkDescriptorSetLayoutBinding samplerLayoutBinding = { 0 };
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = 0;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding uboLayoutBinding;
    zero(uboLayoutBinding);
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {
        samplerLayoutBinding,
        uboLayoutBinding
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo;
    zero(layoutInfo);
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = arrayCount(bindings);
    layoutInfo.pBindings = bindings;

    VkDescriptorSetLayout descriptorSetLayout;
    assert(vkCreateDescriptorSetLayout(device, &layoutInfo, 0, &descriptorSetLayout) == VK_SUCCESS);

    VkPipelineLayout pipelineLayout;
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo;
        zero(pipelineLayoutInfo);
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, 0, &pipelineLayout);
        assert(result == VK_SUCCESS);
    }

    SwapChain swapChain;
    initSwapChain(
        &swapChain,
        VK_NULL_HANDLE,
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
        graphicsQueue,
        commandPool,
        descriptorSetLayout,
        textureImageView,
        textureSampler
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

    ShowWindow(window, SW_SHOWNORMAL);

    u32 currentFrame = 0;

    TRACKMOUSEEVENT trackMouse;
    trackMouse.cbSize = sizeof(TRACKMOUSEEVENT);
    trackMouse.dwFlags = TME_LEAVE;
    trackMouse.hwndTrack = window;
    trackMouse.dwHoverTime = HOVER_DEFAULT;

    i32 currentMouseX = -1;
    i32 currentMouseY = -1;
    i32 rightOfMouseWhenSizeStarted = 0;
    i32 belowMouseWhenSizeStarted = 0;
    b32 changeX = false;
    b32 changeY = false;
    i32 changeThreshold = 50;
    b32 insideChangeX = false;
    b32 insideChangeY = false;
    HCURSOR cursorSizeWE = LoadCursorW(0, (LPWSTR)IDC_SIZEWE);
    HCURSOR cursorSizeNS = LoadCursorW(0, (LPWSTR)IDC_SIZENS);
    HCURSOR cursorSizeNWSE = LoadCursorW(0, (LPWSTR)IDC_SIZENWSE);

    Rect rect1 = { 0 };

    rect1.topleft.x = -0.5f;
    rect1.topleft.y = -0.5f;
    rect1.topleft.z = 0.0f;
    rect1.bottomright.x = 0.5f;
    rect1.bottomright.y = 0.5f;
    rect1.bottomright.z = 0.0f;

    rect1.textopleft.x = 0.0f;
    rect1.textopleft.y = 1.0f;
    rect1.texbottomright.x = 1.0f;
    rect1.texbottomright.y = 0.0f;

    Rect rect2 = moveRect(rect1, 0.1f, 0.1f, -0.5f);

    b32 minimized = false;

    f32 angle = 0.0f;
    f32 xDisplacement = 0.0f;
    f32 xDirection = 1.0f;
    while (globalRunning) {

        //
        //
        //

        while (minimized) {
            MSG msg;
            while (GetMessageW(&msg, window, 0, 0) && minimized) {
                switch (msg.message) {
                case WM_SYSCOMMAND: {
                    usize cmd = msg.wParam;
                    switch (cmd) {
                    case SC_RESTORE: {
                        minimized = false;
                    } break;
                    }
                } break;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        TrackMouseEvent(&trackMouse);
        MSG msg;
        while (PeekMessageW(&msg, window, 0, 0, PM_REMOVE) != 0) {
            switch (msg.message) {
            case WM_MOUSEMOVE: {
                currentMouseX = GET_X_LPARAM(msg.lParam);
                currentMouseY = GET_Y_LPARAM(msg.lParam);
                insideChangeX = (currentMouseX < windowWidth) && (currentMouseX > windowWidth - changeThreshold);
                insideChangeY = (currentMouseY < windowHeight) && (currentMouseY > windowHeight - changeThreshold);
            } break;
            case WM_LBUTTONDOWN: {
                b32 setCapture = false;
                if (insideChangeX) {
                    changeX = true;
                    rightOfMouseWhenSizeStarted = windowWidth - currentMouseX;
                    setCapture = true;
                } if (insideChangeY) {
                    changeY = true;
                    belowMouseWhenSizeStarted = windowHeight - currentMouseY;
                    setCapture = true;
                }
                if (setCapture) {
                    SetCapture(window);
                }
            } break;
            case WM_LBUTTONUP: {
                changeX = false;
                changeY = false;
                ClipCursor(0);
                ReleaseCapture();
            } break;
            case WM_MOUSELEAVE: {
                changeX = false;
                changeY = false;
                insideChangeX = false;
                insideChangeY = false;
            } break;
            case WM_KEYDOWN: {
                usize keycode = msg.wParam;
                switch (keycode) {
                case 0x4D: { // NOTE(sen) M
                    ShowWindow(window, SW_MINIMIZE);
                    minimized = true;
                }
                }
            } break;
            default: {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            } break;
            }
        }

        if (insideChangeX && insideChangeY) {
            SetCursor(cursorSizeNWSE);
        } else if (insideChangeX) {
            SetCursor(cursorSizeWE);
        } else if (insideChangeY) {
            SetCursor(cursorSizeNS);
        } else {
            SetCursor(windowClass.hCursor);
        }

        if (changeX || changeY) {
            RECT currentRect;
            GetWindowRect(window, &currentRect);
            if (changeX) {
                i32 newWindowWidth = currentMouseX + rightOfMouseWhenSizeStarted;
                windowWidth = newWindowWidth;
            }
            if (changeY) {
                i32 newWindowHeight = currentMouseY + belowMouseWhenSizeStarted;
                windowHeight = newWindowHeight;
            }
            SetWindowPos(window, 0, currentRect.left, currentRect.top, windowWidth, windowHeight, 0);
        }

        //
        //
        //

        vkWaitForFences(device, 1, inFlightFences + currentFrame, VK_TRUE, UINT64_MAX);

        u32 imageIndex;
        {
            VkResult result = vkAcquireNextImageKHR(
                device, swapChain.swapChain, UINT64_MAX,
                imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex
            );
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                SwapChain oldSwapChain = swapChain;
                initSwapChain(
                    &swapChain,
                    oldSwapChain.swapChain,
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
                    graphicsQueue,
                    commandPool,
                    descriptorSetLayout,
                    textureImageView,
                    textureSampler
                );
                cleanupSwapChain(&oldSwapChain, device, commandPool);
                result = vkAcquireNextImageKHR(
                    device, swapChain.swapChain, UINT64_MAX,
                    imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex
                );
                assert(result == VK_SUCCESS);
            }
        }

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, imagesInFlight + imageIndex, VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        // NOTE(sen) Update uniform
        {
            UniformBufferObject ubo;
            zero(ubo);
            ubo.model = m4transpose(m4mul(
                m4mul(m4rotationZ(0), m4translation(0.0f, 0.0f, 0.0f)),
                m4scale(1.0f, 1.0f, 1.0f)
            ));
            ubo.view = m4transpose(
                m4lookat(v3new(0.0f, -1.0f, 1.5f), v3new(0.0f, 0.0f, 0.0f), v3new(0.0f, 0.0f, 1.0f))
            );
            ubo.proj = m4transpose(m4perspective(TAU32 / 8, swapChain.surfaceDim.x / swapChain.surfaceDim.y, 0.1f, 10.0f));

            void* data;
            VkDeviceMemory uniformBuffermemory = swapChain.uniformBuffersMemory[imageIndex];
            vkMapMemory(device, uniformBuffermemory, 0, sizeof(ubo), 0, &data);
            CopyMemory(data, &ubo, sizeof(ubo));
            vkUnmapMemory(device, uniformBuffermemory);

            angle += 0.001f;
            if (angle > TAU32) {
                angle -= TAU32;
            }
            xDisplacement += 0.001f * xDirection;
            if (xDisplacement > 0.5f || xDisplacement < -0.5f) {
                xDirection *= -1.0f;
            }
        }

        // NOTE(sen) Update vertex/index buffer
        VertexIndexBuffer* vertexIndexBuffer = swapChain.vertexIndexBuffer + imageIndex;

        vertexIndexBuffer->curIndex = 0;
        vertexIndexBuffer->curVertex = 0;

        rect2 = moveRect(rect2, xDisplacement * 0.001f, 0, 0);

        pushRect(vertexIndexBuffer, rect1);
        pushRect(vertexIndexBuffer, rect2);

        // NOTE(sen) Fill commands
        {
            vkResetCommandBuffer(swapChain.commandBuffers[imageIndex], 0);

            VkCommandBufferBeginInfo beginInfo;
            zero(beginInfo);
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0;
            beginInfo.pInheritanceInfo = 0;
            assert(vkBeginCommandBuffer(swapChain.commandBuffers[imageIndex], &beginInfo) == VK_SUCCESS);

            VkRenderPassBeginInfo renderPassInfo;
            zero(renderPassInfo);
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = swapChain.renderPass;
            renderPassInfo.framebuffer = swapChain.framebuffers[imageIndex];
            renderPassInfo.renderArea.offset.x = 0;
            renderPassInfo.renderArea.offset.y = 0;
            renderPassInfo.renderArea.extent.width = (u32)swapChain.surfaceDim.x;
            renderPassInfo.renderArea.extent.height = (u32)swapChain.surfaceDim.y;

            VkClearValue clearColor = { {{0.01f, 0.01f, 0.01f, 1.0f}} };
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(swapChain.commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(swapChain.commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, swapChain.graphicsPipeline);

            VkBuffer vertexBuffers[] = { vertexIndexBuffer->vertexBuffer };
            VkDeviceSize offsets[] = { 0 };

            vkCmdBindVertexBuffers(swapChain.commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(swapChain.commandBuffers[imageIndex], vertexIndexBuffer->indexBuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdBindDescriptorSets(
                swapChain.commandBuffers[imageIndex],
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout, 0, 1, swapChain.descriptorSets + imageIndex, 0, 0
            );

            vkCmdDrawIndexed(swapChain.commandBuffers[imageIndex], vertexIndexBuffer->curIndex, 1, 0, 0, 0);

            vkCmdEndRenderPass(swapChain.commandBuffers[imageIndex]);

            assert(vkEndCommandBuffer(swapChain.commandBuffers[imageIndex]) == VK_SUCCESS);
        }

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
        {
            VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);
            assert(result == VK_SUCCESS);
        }

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
