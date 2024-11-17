#include "util.h"
#include "maths.h"
#include "obj_parser.h"
#include "texture_data.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define WINDOW_TITLE "scop"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define MAX_FRAMES_IN_FLIGHT 2

static Vertex* g_vertices = NULL;
static size_t g_vertexCount = 0;

static u32* g_indices = NULL;
static size_t g_indexCount = 0;

static f32 g_modelX = 0.0f;
static f32 g_modelY = 0.0f;
static f32 g_modelZ = 0.0f;

static bool g_showTexture = false;
static f32 g_colorToTextureRatio = 0.0f;
static f32 g_colorToTextureTransitionRate = 0.01f;

static const char* validationLayerNames[] = {
#if DEBUG
    "VK_LAYER_KHRONOS_validation"
#endif
};
static u32 validationLayerCount = ARR_LEN(validationLayerNames);

static const char* instanceExtensionNames[] = {
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
};
static u32 instanceExtensionCount = ARR_LEN(instanceExtensionNames);

static const char* deviceExtensionNames[] = {
    "VK_KHR_portability_subset",
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
static u32 deviceExtensionCount = ARR_LEN(deviceExtensionNames);

static void normalizeAndCenterModel()
{
    f32 vertexMinX = FLT_MAX;
    f32 vertexMinY = FLT_MAX;
    f32 vertexMinZ = FLT_MAX;
    f32 vertexMaxX = -FLT_MAX;
    f32 vertexMaxY = -FLT_MAX;
    f32 vertexMaxZ = -FLT_MAX;
    for (size_t i = 0; i < g_vertexCount; i++)
    {
        vertexMinX = (g_vertices[i].pos.x < vertexMinX) ? g_vertices[i].pos.x : vertexMinX;
        vertexMinY = (g_vertices[i].pos.y < vertexMinY) ? g_vertices[i].pos.y : vertexMinY;
        vertexMinZ = (g_vertices[i].pos.z < vertexMinZ) ? g_vertices[i].pos.z : vertexMinZ;
        vertexMaxX = (g_vertices[i].pos.x > vertexMaxX) ? g_vertices[i].pos.x : vertexMaxX;
        vertexMaxY = (g_vertices[i].pos.y > vertexMaxY) ? g_vertices[i].pos.y : vertexMaxY;
        vertexMaxZ = (g_vertices[i].pos.z > vertexMaxZ) ? g_vertices[i].pos.z : vertexMaxZ;
    }

    f32 bBoxExtentX = vertexMaxX - vertexMinX;
    f32 bBoxExtentY = vertexMaxY - vertexMinY;
    f32 bBoxExtentZ = vertexMaxZ - vertexMinZ;
    f32 normalizationScalar = (bBoxExtentX > bBoxExtentY) ? bBoxExtentX : bBoxExtentY;
    normalizationScalar = (normalizationScalar > bBoxExtentZ) ? normalizationScalar : bBoxExtentZ;

    f32 bBoxCenterX = vertexMinX + bBoxExtentX * 0.5;
    f32 bBoxCenterY = vertexMinY + bBoxExtentY * 0.5;
    f32 bBoxCenterZ = vertexMinZ + bBoxExtentZ * 0.5;
    for (size_t i = 0; i < g_vertexCount; i++)
    {
        g_vertices[i].pos.x -= bBoxCenterX;
        g_vertices[i].pos.y -= bBoxCenterY;
        g_vertices[i].pos.z -= bBoxCenterZ;

        g_vertices[i].pos.x /= normalizationScalar;
        g_vertices[i].pos.y /= normalizationScalar;
        g_vertices[i].pos.z /= normalizationScalar;

        g_vertices[i].texCoord.x = g_vertices[i].pos.y;
        g_vertices[i].texCoord.y = g_vertices[i].pos.z;
    }
}

static u32* readShaderBytecode(const char* filePath, const char* mode, usize* outBytesRead)
{
    FILE* file = fopen(filePath, mode);
    if (file == NULL)
        PANIC("%s%s\n", "Failed to open file: ", filePath);

    if (fseek(file, 0, SEEK_END) == -1)
        PANIC("%s%s\n", "Failed to seek end of file: ", filePath);

    long fileSize = ftell(file);
    if (fileSize == -1)
        PANIC("%s%s\n", "Failed to determine size of file: ", filePath);

    rewind(file);

    ASSERT(sizeof(void*) % sizeof(u32) == 0);
    void* bytecode;
    if (posix_memalign(&bytecode, sizeof(void*), fileSize) != 0)
        PANIC("%s%s\n", "Failed to allocate memory for file: ", filePath);

    size_t bytesRead = fread(bytecode, 1, fileSize, file);

    if (fclose(file) == EOF)
        INFORM("%s%s\n", "Failed to close file: ", filePath);
    
    if (bytesRead != fileSize || ferror(file))
        PANIC("%s%s\n", "Failed to read file: ", filePath);

    if (outBytesRead != NULL)
        *outBytesRead = (usize)bytesRead;

    return bytecode;
}

VkInstance createVulkanInstance()
{
    u32 glfwExtensionCount = 0;
    const char** glfwExtensionNames = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensionNames == NULL)
        PANIC("%s\n", "System does not provide the Vulkan instance extensions required by GLFW");

    u32 enabledExtensionCount = instanceExtensionCount + glfwExtensionCount;
    const char** enabledExtensionNames = mallocOrDie(enabledExtensionCount * sizeof(char*));
    memcpy(enabledExtensionNames, instanceExtensionNames, instanceExtensionCount * sizeof(char*));
    memcpy(enabledExtensionNames + instanceExtensionCount, glfwExtensionNames, glfwExtensionCount * sizeof(char*));

    VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
        .enabledLayerCount = validationLayerCount,
        .ppEnabledLayerNames = validationLayerNames,
        .enabledExtensionCount = enabledExtensionCount,
        .ppEnabledExtensionNames = enabledExtensionNames
    };

    VkInstance instance;
    if (vkCreateInstance(&instanceCreateInfo, NULL, &instance) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create Vulkan instance");

    freeAndNull(enabledExtensionNames);

    return instance;
}

VkPhysicalDevice pickPhysicalDeviceAndQueueFamily(VkInstance instance, VkSurfaceKHR surface, u32* outQueueFamilyIndex)
{
    u32 physicalDeviceCount = 0;
    if (vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, NULL) != VK_SUCCESS)
        PANIC("%s\n", "Failed to determine physical device count");

    VkPhysicalDevice* physicalDevices = mallocOrDie(sizeof(VkPhysicalDevice) * physicalDeviceCount);
    if (vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices) != VK_SUCCESS)
        PANIC("%s\n", "Failed to enumerate physical devices");

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    u32 queueFamilyIndex = 0;
    for (u32 i = 0; i < physicalDeviceCount && physicalDevice == VK_NULL_HANDLE; i++)
    {
        u32 queueFamilyPropertiesCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyPropertiesCount, NULL);
        VkQueueFamilyProperties* queueFamilyProperties = mallocOrDie(sizeof(VkQueueFamilyProperties) * queueFamilyPropertiesCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyPropertiesCount, queueFamilyProperties);

        for (u32 j = 0; j < queueFamilyPropertiesCount; j++)
        {
            if (!(queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                continue;

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevices[i], j, surface, &presentSupport);
            if (!presentSupport)
                continue;

            u32 deviceExtensionPropertiesCount;
            if (vkEnumerateDeviceExtensionProperties(physicalDevices[i], NULL, &deviceExtensionPropertiesCount, NULL) != VK_SUCCESS)
                PANIC("%s\n", "Failed to determine physical device extension properties count");

            VkExtensionProperties* deviceExtensionProperties = mallocOrDie(sizeof(VkExtensionProperties) * deviceExtensionPropertiesCount);
            if (vkEnumerateDeviceExtensionProperties(physicalDevices[i], NULL, &deviceExtensionPropertiesCount, deviceExtensionProperties) != VK_SUCCESS)
                PANIC("%s\n", "Failed to enumerate physical device extension properties");
            
            bool requiredDeviceExtensionsAvailable = true;
            for (usize x = 0; x < ARR_LEN(deviceExtensionNames); x++)
            {
                bool isExtensionAvailable = false;
                for (u32 y = 0; y < deviceExtensionPropertiesCount; y++)
                    isExtensionAvailable |= (strcmp(deviceExtensionNames[x], deviceExtensionProperties[y].extensionName) == 0);
                requiredDeviceExtensionsAvailable &= isExtensionAvailable;
            }

            freeAndNull(deviceExtensionProperties);

            if (!requiredDeviceExtensionsAvailable)
                continue;

            VkPhysicalDeviceFeatures physicalDeviceFeatures;
            vkGetPhysicalDeviceFeatures(physicalDevices[i], &physicalDeviceFeatures);
            if (!physicalDeviceFeatures.samplerAnisotropy)
                continue;

            physicalDevice = physicalDevices[i];
            queueFamilyIndex = j;
            break;
        }

        freeAndNull(queueFamilyProperties);
    }

    freeAndNull(physicalDevices);

    if (physicalDevice == VK_NULL_HANDLE)
        PANIC("%s\n", "Failed to find a suitable physical device");
    
    if (outQueueFamilyIndex != NULL)
        *outQueueFamilyIndex = queueFamilyIndex;

    return physicalDevice;
}

u32 pickMemoryType(VkPhysicalDevice physicalDevice, u32 typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++)
        if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    PANIC("%s\n", "Failed to find suitable memory type");
}

VkSurfaceFormatKHR pickSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    u32 formatCount;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL) != VK_SUCCESS)
        PANIC("%s\n", "Failed to determine physical device surface format count");

    if (formatCount == 0)
        PANIC("%s\n", "Physical device does not support any surface formats");
    
    VkSurfaceFormatKHR* formats = mallocOrDie(sizeof(VkSurfaceFormatKHR) * formatCount);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats) != VK_SUCCESS)
        PANIC("%s\n", "Failed to enumerate physical device surface formats");

    VkSurfaceFormatKHR format = formats[0];
    for (u32 i = 0; i < formatCount; i++)
    {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            format = formats[i];
            break;
        }
    }

    freeAndNull(formats);

    return format;
}

VkPresentModeKHR pickSurfacePresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    u32 presentModeCount;
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL) != VK_SUCCESS)
        PANIC("%s\n", "Failed to determine physical device surface present mode count");

    if (presentModeCount == 0)
        PANIC("%s\n", "Physical device does not support any surface present modes");

    VkPresentModeKHR* presentModes = mallocOrDie(sizeof(VkPresentModeKHR) * presentModeCount);
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL) != VK_SUCCESS)
        PANIC("%s\n", "Failed to enumerate physical device surface present modes");

    // NOTE(Hans): The standard FIFO present mode is guaranteed to exist
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 i = 0; i < presentModeCount; i++)
    {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            presentMode = presentModes[i];
            break;
        }
    }

    freeAndNull(presentModes);

    return presentMode;
}

VkExtent2D querySurfaceExtent(GLFWwindow* window, VkSurfaceCapabilitiesKHR surfaceCapabilities)
{
    VkExtent2D extent = surfaceCapabilities.currentExtent;
    if (extent.width == UINT32_MAX)
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        u32 minWidth = surfaceCapabilities.minImageExtent.width;
        u32 maxWidth = surfaceCapabilities.maxImageExtent.width;
        extent.width = (u32)(width > minWidth) ? width : minWidth;
        extent.width = (u32)(width < maxWidth) ? width : maxWidth;

        u32 minHeight = surfaceCapabilities.minImageExtent.height;
        u32 maxHeight = surfaceCapabilities.maxImageExtent.height;
        extent.height = (u32)(height > minHeight) ? height : minHeight;
        extent.height = (u32)(height < maxHeight) ? height : maxHeight;
    }

    return extent;
}

VkRenderPass createRenderPass(VkDevice device, VkFormat pixelFormat)
{
    VkAttachmentDescription attachmentDescriptions[]  = {
        { // Color
            .format = pixelFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        },
        { // Depth
            .format = VK_FORMAT_D32_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }
    };

    VkAttachmentReference colorAttachmentReference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depthAttachmentReference = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpassDescription = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentReference,
        .pDepthStencilAttachment = &depthAttachmentReference
    };

    VkSubpassDependency subpassDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo renderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = attachmentDescriptions,
        .subpassCount = 1,
        .pSubpasses = &subpassDescription,
        .dependencyCount = 1,
        .pDependencies = &subpassDependency
    };

    VkRenderPass renderPass;
    if (vkCreateRenderPass(device, &renderPassCreateInfo, NULL, &renderPass) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create render pass");
    
    return renderPass;
}

VkPipeline createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout, VkPipelineLayout* outPipelineLayout)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
    };

    usize vertShaderCodeSize;
    u32* vertShaderCode = readShaderBytecode("shaders/vert.spv", "rb", &vertShaderCodeSize);
    shaderModuleCreateInfo.codeSize = vertShaderCodeSize;
    shaderModuleCreateInfo.pCode = vertShaderCode;

    VkShaderModule vertShaderModule;
    if (vkCreateShaderModule(device, &shaderModuleCreateInfo, NULL, &vertShaderModule) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create vertex shader module");
    freeAndNull(vertShaderCode);

    usize fragShaderCodeSize;
    u32* fragShaderCode = readShaderBytecode("shaders/frag.spv", "rb", &fragShaderCodeSize);
    shaderModuleCreateInfo.codeSize = fragShaderCodeSize;
    shaderModuleCreateInfo.pCode = fragShaderCode;

    VkShaderModule fragShaderModule;
    if (vkCreateShaderModule(device, &shaderModuleCreateInfo, NULL, &fragShaderModule) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create fragment shader module");
    freeAndNull(fragShaderCode);

    VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main"
        },
    };

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ARR_LEN(dynamicStates),
        .pDynamicStates = dynamicStates
    };

    VkVertexInputBindingDescription vertexInputBindingDescription = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, pos)
        },
        {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, texCoord)
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertexInputBindingDescription,
        .vertexAttributeDescriptionCount = ARR_LEN(vertexInputAttributeDescriptions),
        .pVertexAttributeDescriptions = vertexInputAttributeDescriptions
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.f
    };

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachmentState
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
    };

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &pipelineLayout) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shaderStageCreateInfos,
        .pVertexInputState = &vertexInputStateCreateInfo,
        .pInputAssemblyState = &inputAssemblyStateCreateInfo,
        .pViewportState = &viewportStateCreateInfo,
        .pRasterizationState = &rasterizationStateCreateInfo,
        .pMultisampleState = &multisampleStateCreateInfo,
        .pDepthStencilState = &depthStencilStateCreateInfo,
        .pColorBlendState = &colorBlendStateCreateInfo,
        .pDynamicState = &dynamicStateCreateInfo,
        .layout = pipelineLayout,
        .renderPass = renderPass,
        .subpass = 0
    };

    VkPipeline graphicsPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, NULL, &graphicsPipeline) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create graphics pipeline");

    vkDestroyShaderModule(device, vertShaderModule, NULL);
    vkDestroyShaderModule(device, fragShaderModule, NULL);

    ASSERT(outPipelineLayout != NULL);
    *outPipelineLayout = pipelineLayout;

    return graphicsPipeline;
}

static VkSwapchainKHR createSwapchain(VkDevice device, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR surfaceCapabilities, VkSurfaceFormatKHR surfaceFormat, VkPresentModeKHR surfacePresentMode, VkExtent2D surfaceExtent)
{
    u32 swapchainMinImageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 && swapchainMinImageCount > surfaceCapabilities.maxImageCount)
        swapchainMinImageCount = surfaceCapabilities.maxImageCount;
    
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = swapchainMinImageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = surfaceExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = surfacePresentMode,
        .clipped = VK_TRUE
    };

    VkSwapchainKHR swapchain;
    if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &swapchain) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create swapchain");
    
    return swapchain;
}

static void destroySwapchain(VkDevice device, VkSwapchainKHR* swapchain, u32 imageCount, VkImage* images, VkImageView* imageViews, VkFramebuffer* framebuffers)
{
    if (vkDeviceWaitIdle(device) != VK_SUCCESS)
        PANIC("%s\n", "Failed to wait for device to be idle");

    for (u32 i = 0; i < imageCount; i++)
    {
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
        vkDestroyImageView(device, imageViews[i], NULL);
    }

    freeAndNull(framebuffers);
    freeAndNull(imageViews);
    freeAndNull(images);

    vkDestroySwapchainKHR(device, *swapchain, NULL);
    *swapchain = VK_NULL_HANDLE;
}

static VkImage* getSwapchainImages(VkDevice device, VkSwapchainKHR swapchain, u32* outImageCount)
{
    u32 imageCount;
    if (vkGetSwapchainImagesKHR(device, swapchain, &imageCount, NULL) != VK_SUCCESS)
        PANIC("%s\n", "Failed to determine swapchain image count");

    VkImage* images = mallocOrDie(sizeof(VkImage) * imageCount);
    if (vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images) != VK_SUCCESS)
        PANIC("%s\n", "Failed to get handles to swapchain images");

    if (outImageCount != NULL)
        *outImageCount = imageCount;

    return images;
}

static VkImageView* createSwapchainImageViews(VkDevice device, VkImage* images, u32 imageCount, VkFormat pixelFormat)
{
    VkImageView* imageViews = mallocOrDie(sizeof(VkImageView) * imageCount);
    for (u32 i = 0; i < imageCount; i++)
    {
        VkImageViewCreateInfo imageViewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = pixelFormat,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1
            }
        };
        
        if (vkCreateImageView(device, &imageViewCreateInfo, NULL, &imageViews[i]) != VK_SUCCESS)
            PANIC("%s\n", "Failed to create image view");
    }

    return imageViews;
}

static VkFramebuffer* createSwapchainFramebuffers(VkDevice device, VkImageView* imageViews, u32 imageCount, VkExtent2D extent, VkRenderPass renderPass, VkImageView depthImageView)
{
    VkFramebuffer* framebuffers = mallocOrDie(sizeof(VkFramebuffer) * imageCount);
    for (u32 i = 0; i < imageCount; i++)
    {
        VkImageView attachments[] = {imageViews[i], depthImageView};
        VkFramebufferCreateInfo framebufferCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass,
            .attachmentCount = ARR_LEN(attachments),
            .pAttachments = attachments,
            .width = extent.width,
            .height = extent.height,
            .layers = 1
        };

        if (vkCreateFramebuffer(device, &framebufferCreateInfo, NULL, &framebuffers[i]) != VK_SUCCESS) {
            PANIC("%s\n", "Failed to create framebuffer");
        }
    }

    return framebuffers;
}

VkBuffer createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceMemory* outMemory, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    ASSERT(outMemory != NULL);

    VkBufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buffer;
    if (vkCreateBuffer(device, &createInfo, NULL, &buffer) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create buffer");

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
    u32 memoryTypeIndex = pickMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, properties);

    VkMemoryAllocateInfo memoryAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };

    if (vkAllocateMemory(device, &memoryAllocateInfo, NULL, outMemory) != VK_SUCCESS)
        PANIC("%s\n", "Failed to allocate buffer memory");

    vkBindBufferMemory(device, buffer, *outMemory, 0);

    return buffer;
}

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = commandPool,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer) != VK_SUCCESS)
        PANIC("%s\n", "Failed to allocate command buffer for buffer transfer");

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        PANIC("%s\n", "Failed to begin command buffer for buffer transfer");
    
    return commandBuffer;
}

void endSingleTimeCommands(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        PANIC("%s\n", "Failed to end command buffer for buffer transfer");
    
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };

    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
        PANIC("%s\n", "Failed to submit command buffer for buffer transfer to queue");

    if (vkQueueWaitIdle(queue) != VK_SUCCESS)
        PANIC("%s\n", "Failed to wait for queue to be idle");

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void copyBuffer(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);
    VkBufferCopy copyRegion = {.size = size};
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);
    endSingleTimeCommands(device, queue, commandPool, commandBuffer);
}

void copyBufferToImage(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

    VkBufferImageCopy region = {
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageExtent = {width, height, 1}
    };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(device, queue, commandPool, commandBuffer);
}

VkBuffer createVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandPool commandPool, VkDeviceMemory* outMemory)
{
    VkDeviceSize bufferSize = sizeof(g_vertices[0]) * g_vertexCount;

    VkDeviceMemory stagingBufferMemory;
    VkBuffer stagingBuffer = createBuffer(physicalDevice, device, &stagingBufferMemory, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* bufferData;
    if (vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &bufferData) != VK_SUCCESS)
        PANIC("%s\n", "Failed to map vertex buffer memory");
    memcpy(bufferData, g_vertices, bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    VkBuffer buffer = createBuffer(physicalDevice, device, outMemory, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    copyBuffer(device, queue, commandPool, stagingBuffer, buffer, bufferSize);
    vkDestroyBuffer(device, stagingBuffer, NULL);
    vkFreeMemory(device, stagingBufferMemory, NULL);

    return buffer;
}

VkBuffer createIndexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandPool commandPool, VkDeviceMemory* outMemory)
{
    VkDeviceSize bufferSize = sizeof(g_indices[0]) * g_indexCount;

    VkDeviceMemory stagingBufferMemory;
    VkBuffer stagingBuffer = createBuffer(physicalDevice, device, &stagingBufferMemory, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* bufferData;
    if (vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &bufferData) != VK_SUCCESS)
        PANIC("%s\n", "Failed to map index buffer memory");
    memcpy(bufferData, g_indices, bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    VkBuffer buffer = createBuffer(physicalDevice, device, outMemory, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    copyBuffer(device, queue, commandPool, stagingBuffer, buffer, bufferSize);
    vkDestroyBuffer(device, stagingBuffer, NULL);
    vkFreeMemory(device, stagingBufferMemory, NULL);

    return buffer;
}

void transitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

    VkImageMemoryBarrier imageMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkPipelineStageFlags srcStageFlags;
    VkPipelineStageFlags dstStageFlags;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        PANIC("%s\n", "Unsupported layout transition");
    }

    vkCmdPipelineBarrier(commandBuffer, srcStageFlags, dstStageFlags, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);

    endSingleTimeCommands(device, queue, commandPool, commandBuffer);
}

VkImage createImage(VkPhysicalDevice physicalDevice, VkDevice device, u32 width, u32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory* outImageMemory)
{
    ASSERT(outImageMemory != NULL);

    VkImageCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage image;
    if (vkCreateImage(device, &createInfo, NULL, &image) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create texture image");

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(device, image, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = pickMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, properties)
    };

    if (vkAllocateMemory(device, &memoryAllocateInfo, NULL, outImageMemory) != VK_SUCCESS)
        PANIC("%s\n", "Failed to allocate memory for texture image");

    if (vkBindImageMemory(device, image, *outImageMemory, 0) != VK_SUCCESS)
        PANIC("%s\n", "Failed to bind image memory for texture image");
    
    return image;
}

VkImage createTextureImage(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkCommandPool commandPool, VkDeviceMemory* outImageMemory)
{
    VkDeviceSize imageSize = g_textureDataWidth * g_textureDataHeight * 4;
    VkDeviceMemory stagingBufferMemory;
    VkBuffer stagingBuffer = createBuffer(physicalDevice, device, &stagingBufferMemory, imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    void* data;
    if (vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data) != VK_SUCCESS)
        PANIC("%s\n", "Failed to map memory for texture image");
    memcpy(data, g_textureData, imageSize);
    vkUnmapMemory(device, stagingBufferMemory);

    VkImage textureImage = createImage(physicalDevice, device, g_textureDataWidth, g_textureDataHeight, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outImageMemory);
    transitionImageLayout(device, queue, commandPool, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(device, queue, commandPool, stagingBuffer, textureImage, g_textureDataWidth, g_textureDataHeight);
    transitionImageLayout(device, queue, commandPool, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, NULL);
    vkFreeMemory(device, stagingBufferMemory, NULL);

    return textureImage;
}

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = aspectFlags,
            .levelCount = 1,
            .layerCount = 1
        }
    };

    VkImageView imageView;   
    if (vkCreateImageView(device, &createInfo, NULL, &imageView) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create image view");

    return imageView;
}

VkSampler createTextureSampler(VkPhysicalDevice physicalDevice, VkDevice device)
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    VkSamplerCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = physicalDeviceProperties.limits.maxSamplerAnisotropy,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f
    };

    VkSampler textureSampler;
    if (vkCreateSampler(device, &createInfo, NULL, &textureSampler) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create texture sampler");

    return textureSampler;
}

typedef struct
{
    Mat4 model;
    Mat4 view;
    Mat4 proj;
    f32 colorToTextureRatio;
} UniformBufferObject;

void createUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice device, VkBuffer uniformBuffers[], VkDeviceMemory uniformBuffersMemory[], void* uniformBuffersMapped[])
{
    ASSERT(uniformBuffers != NULL);
    ASSERT(uniformBuffersMemory != NULL);
    ASSERT(uniformBuffersMapped != NULL);

    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        uniformBuffers[i] = createBuffer(physicalDevice, device, &uniformBuffersMemory[i], bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]) != VK_SUCCESS)
            PANIC("%s\n", "Failed to map uniform buffer memory");
    }
}

void updateUniformBuffer(void* uniformBuffersMapped[], VkExtent2D surfaceExtent, u32 currentImage)
{
    float time = (float)clock() / CLOCKS_PER_SEC;
    if (time == -1)
        PANIC("%s\n", "Failed to read system clock");

    UniformBufferObject ubo = {
        .model = mulMat4(translate((Vec3){g_modelX, g_modelY, g_modelZ}), rotateRH(time, (Vec3){0.0f, 1.0f, 0.0f})),
        .view = LookAtRH((Vec3){0.0f, 0.0f, 2.0f}, (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){0.0f, 1.0f, 0.0f}),
        .proj = perspectiveRH(45.0f, surfaceExtent.width / (f32)surfaceExtent.height, 0.1f, 100.0f),
        .colorToTextureRatio = g_colorToTextureRatio
    };

    ubo.proj.elements[1][1] *= -1;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

typedef struct
{
    int width;
    int height;
    bool resized;
} WindowFramebufferInfo;

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    WindowFramebufferInfo* framebufferInfo = glfwGetWindowUserPointer(window);
    framebufferInfo->width = width;
    framebufferInfo->height = height;
    framebufferInfo->resized = true;
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS)
        return;
    
    switch (key)
    {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, 1);
            break;
        case GLFW_KEY_T:
            g_showTexture = !g_showTexture;
            break;
        case GLFW_KEY_A:
            g_modelX += 1.0f;
            break;
        case GLFW_KEY_D:
            g_modelX -= 1.0f;
            break;
        case GLFW_KEY_Q:
            g_modelY += 1.0f;
            break;
        case GLFW_KEY_E:
            g_modelY -= 1.0f;
            break;
        case GLFW_KEY_W:
            g_modelZ += 1.0f;
            break;
        case GLFW_KEY_S:
            g_modelZ -= 1.0f;
            break;
    }
}

static void onExit(void)
{
    glfwTerminate();

    freeAndNull(g_vertices);
    freeAndNull(g_indices);
}

int main(int argc, char* argv[])
{
    if (argc != 2)
        PANIC("%s\n", "usage: scop obj_file");

    if (atexit(onExit) != 0)
        PANIC("%s\n", "Failed to register atexit function");

    if (!glfwInit())
        PANIC("%s\n", "Failed to initialize GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (window == NULL)
        PANIC("%s\n", "Failed to create GLFW window");

    WindowFramebufferInfo windowFramebufferInfo = {
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
        .resized = false
    };

    glfwSetWindowUserPointer(window, &windowFramebufferInfo);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, keyCallback);

    parseObjFile(argv[1], &g_vertices, &g_vertexCount, &g_indices, &g_indexCount);
    normalizeAndCenterModel();

    VkInstance instance = createVulkanInstance();

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create window surface");

    u32 queueFamilyIndex;
    VkPhysicalDevice physicalDevice = pickPhysicalDeviceAndQueueFamily(instance, surface, &queueFamilyIndex);

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    VkPhysicalDeviceFeatures physicalDeviceFeatures = {.samplerAnisotropy = VK_TRUE};

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = validationLayerCount,
        .ppEnabledLayerNames = validationLayerNames,
        .enabledExtensionCount = deviceExtensionCount,
        .ppEnabledExtensionNames = deviceExtensionNames,
        .pEnabledFeatures = &physicalDeviceFeatures
    };

    VkDevice device;
    vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device);

    VkQueue queue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities) != VK_SUCCESS)
        PANIC("%s\n", "Failed to determine physical device surface capabilities");

    VkSurfaceFormatKHR surfaceFormat = pickSurfaceFormat(physicalDevice, surface);
    VkPresentModeKHR surfacePresentMode = pickSurfacePresentMode(physicalDevice, surface);

    VkRenderPass renderPass = createRenderPass(device, surfaceFormat.format);

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingUBO = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindingSampler = {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[] = {
        descriptorSetLayoutBindingUBO,
        descriptorSetLayoutBindingSampler
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARR_LEN(descriptorSetLayoutBindings),
        .pBindings = descriptorSetLayoutBindings
    };

    VkDescriptorSetLayout descriptorSetLayout;
    if (vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayout) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create descriptor set layout");

    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline = createGraphicsPipeline(device, renderPass, descriptorSetLayout, &pipelineLayout);

    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };

    VkCommandPool commandPool;
    if (vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, &commandPool) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create command pool");

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT
    };

    VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];
    if (vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers) != VK_SUCCESS)
        PANIC("%s\n", "Failed to allocate command buffers");

    VkDeviceMemory textureImageMemory;
    VkImage textureImage = createTextureImage(physicalDevice, device, queue, commandPool, &textureImageMemory);
    VkImageView textureImageView = createImageView(device, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
    VkSampler textureSampler = createTextureSampler(physicalDevice, device);

    VkDeviceMemory vertexBufferMemory;
    VkBuffer vertexBuffer = createVertexBuffer(physicalDevice, device, queue, commandPool, &vertexBufferMemory);

    VkDeviceMemory indexBufferMemory;
    VkBuffer indexBuffer = createIndexBuffer(physicalDevice, device, queue, commandPool, &indexBufferMemory);

    VkBuffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory uniformBuffersMemory[MAX_FRAMES_IN_FLIGHT];
    void* uniformBuffersMapped[MAX_FRAMES_IN_FLIGHT];
    createUniformBuffers(physicalDevice, device, uniformBuffers, uniformBuffersMemory, uniformBuffersMapped);

    VkDescriptorPoolSize descriptorPoolSizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT
        },
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT
        }
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = ARR_LEN(descriptorPoolSizes),
        .pPoolSizes = descriptorPoolSizes
    };

    VkDescriptorPool descriptorPool;
    if (vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &descriptorPool) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create descriptor pool");

    VkDescriptorSetLayout descriptorSetLayouts[MAX_FRAMES_IN_FLIGHT];
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        descriptorSetLayouts[i] = descriptorSetLayout;

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = descriptorSetLayouts
    };

    VkDescriptorSet descriptorSets[MAX_FRAMES_IN_FLIGHT];
    if (vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, descriptorSets) != VK_SUCCESS)
        PANIC("%s\n", "Failed to allocate descriptor sets");

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorBufferInfo descriptorBufferInfo = {
            .buffer = uniformBuffers[i],
            .offset = 0,
            .range = sizeof(UniformBufferObject)
        };

        VkDescriptorImageInfo descriptorImageInfo = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = textureImageView,
            .sampler = textureSampler
        };

        VkWriteDescriptorSet writeDescriptorSets[] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &descriptorBufferInfo
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .pImageInfo = &descriptorImageInfo
            }
        };

        vkUpdateDescriptorSets(device, ARR_LEN(writeDescriptorSets), writeDescriptorSets, 0, NULL);
    }

    VkSemaphoreCreateInfo semaphoreCreateInfo = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    VkFenceCreateInfo fenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(device, &semaphoreCreateInfo, NULL, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreCreateInfo, NULL, &renderFinishedSemaphores[i]) != VK_SUCCESS)
            PANIC("%s\n", "Failed to create semaphore");

        if (vkCreateFence(device, &fenceCreateInfo, NULL, &inFlightFences[i]) != VK_SUCCESS)
            PANIC("%s\n", "Failed to create fence");
    }

    VkExtent2D surfaceExtent;
    VkViewport viewport;
    VkRect2D scissor;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    u32 swapchainImageCount;
    VkImage* swapchainImages = NULL;
    VkImageView* swapchainImageViews = NULL;
    VkFramebuffer* swapchainFramebuffers = NULL;

    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    VkDeviceMemory depthImageMemory;
    VkImage depthImage;
    VkImageView depthImageView;

    u32 currentFrame = 0;
    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (windowFramebufferInfo.width == 0 && windowFramebufferInfo.height == 0)
            continue;

        if (g_showTexture)
        {
            g_colorToTextureRatio += g_colorToTextureTransitionRate;
            g_colorToTextureRatio = (g_colorToTextureRatio > 1) ? 1 : g_colorToTextureRatio;
        }
        else
        {
            g_colorToTextureRatio -= g_colorToTextureTransitionRate;
            g_colorToTextureRatio = (g_colorToTextureRatio < 0) ? 0 : g_colorToTextureRatio;
        }

        if (swapchain == VK_NULL_HANDLE)
        {
            surfaceExtent = querySurfaceExtent(window, surfaceCapabilities);

            viewport = (VkViewport){
                .x = 0.f,
                .y = 0.f,
                .width = (f32)surfaceExtent.width,
                .height = (f32)surfaceExtent.height,
                .minDepth = 0.f,
                .maxDepth = 1.f
            };

            scissor = (VkRect2D){
                .offset = {0, 0},
                .extent = surfaceExtent
            };

            depthImage = createImage(physicalDevice, device, surfaceExtent.width, surfaceExtent.height, depthFormat,
                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthImageMemory);
            depthImageView = createImageView(device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

            swapchain = createSwapchain(device, surface, surfaceCapabilities, surfaceFormat, surfacePresentMode, surfaceExtent);
            swapchainImages = getSwapchainImages(device, swapchain, &swapchainImageCount);
            swapchainImageViews = createSwapchainImageViews(device, swapchainImages, swapchainImageCount, surfaceFormat.format);
            swapchainFramebuffers = createSwapchainFramebuffers(device, swapchainImageViews, swapchainImageCount, surfaceExtent, renderPass, depthImageView);
        }

        if (vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX) != VK_SUCCESS)
            PANIC("%s\n", "Failed to wait for fences");

        u32 imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            destroySwapchain(device, &swapchain, swapchainImageCount, swapchainImages, swapchainImageViews, swapchainFramebuffers);
            continue;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            PANIC("%s\n", "Failed to acquire next image from swapchain");
        }

        if (vkResetFences(device, 1, &inFlightFences[currentFrame]) != VK_SUCCESS)
            PANIC("%s\n", "Failed to reset fences");

        if (vkResetCommandBuffer(commandBuffers[currentFrame], 0) != VK_SUCCESS)
            PANIC("%s\n", "Failed to reset command buffer");
        
        VkCommandBufferBeginInfo commandBufferBeginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        if (vkBeginCommandBuffer(commandBuffers[currentFrame], &commandBufferBeginInfo) != VK_SUCCESS)
            PANIC("%s\n", "Failed to begin command buffer");

        VkClearValue clearValues[] = {{.color = {0.f, 0.f, 0.f, 1.f}}, {.depthStencil = {1.0f, 0}}};
        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPass,
            .framebuffer = swapchainFramebuffers[imageIndex],
            .renderArea = {
                .offset = {0, 0},
                .extent = surfaceExtent
            },
            .clearValueCount = ARR_LEN(clearValues),
            .pClearValues = clearValues
        };

        vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);
        vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);

        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize vertexBufferOffsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, vertexBuffers, vertexBufferOffsets);
        vkCmdBindIndexBuffer(commandBuffers[currentFrame], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, NULL);
        vkCmdDrawIndexed(commandBuffers[currentFrame], g_indexCount, 1, 0, 0, 0);
        vkCmdEndRenderPass(commandBuffers[currentFrame]);

        if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS)
            PANIC("%s\n", "Failed to end command buffer");
        
        updateUniformBuffer(uniformBuffersMapped, surfaceExtent, currentFrame);

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = ARR_LEN(waitSemaphores),
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[currentFrame],
            .signalSemaphoreCount = ARR_LEN(signalSemaphores),
            .pSignalSemaphores = signalSemaphores
        };

        if (vkQueueSubmit(queue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
            PANIC("%s\n", "Failed to submit command buffers to queue");

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = ARR_LEN(signalSemaphores),
            .pWaitSemaphores = signalSemaphores,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex
        };

        result = vkQueuePresentKHR(queue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || windowFramebufferInfo.resized)
        {
            windowFramebufferInfo.resized = false;
            destroySwapchain(device, &swapchain, swapchainImageCount, swapchainImages, swapchainImageViews, swapchainFramebuffers);
        }
        else if (result != VK_SUCCESS)
        {
            PANIC("%s\n", "Failed to queue presentation");
        }
        
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    if (vkDeviceWaitIdle(device) != VK_SUCCESS)
        PANIC("%s\n", "Failed to wait for device to be idle");

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(device, renderFinishedSemaphores[i], NULL);
        vkDestroyFence(device, inFlightFences[i], NULL);
    }

    vkDestroyCommandPool(device, commandPool, NULL);
    vkDestroyPipeline(device, graphicsPipeline, NULL);
    vkDestroyPipelineLayout(device, pipelineLayout, NULL);
    vkDestroyDescriptorPool(device, descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, NULL);
    vkDestroyRenderPass(device, renderPass, NULL);

    if (swapchain != VK_NULL_HANDLE)
        destroySwapchain(device, &swapchain, swapchainImageCount, swapchainImages, swapchainImageViews, swapchainFramebuffers);

    vkDestroySampler(device, textureSampler, NULL);
    vkDestroyImageView(device, textureImageView, NULL);
    vkDestroyImage(device, textureImage, NULL);
    vkFreeMemory(device, textureImageMemory, NULL);

    vkDestroyImageView(device, depthImageView, NULL);
    vkDestroyImage(device, depthImage, NULL);
    vkFreeMemory(device, depthImageMemory, NULL);

    vkDestroyBuffer(device, indexBuffer, NULL);
    vkFreeMemory(device, indexBufferMemory, NULL);

    vkDestroyBuffer(device, vertexBuffer, NULL);
    vkFreeMemory(device, vertexBufferMemory, NULL);

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(device, uniformBuffers[i], NULL);
        vkFreeMemory(device, uniformBuffersMemory[i], NULL);
    }

    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);

    return EXIT_SUCCESS;
}
