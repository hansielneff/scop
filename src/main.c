#include "util.h"

#include <stdlib.h>
#include <string.h>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define WINDOW_TITLE "scop"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

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

static void onExit(void)
{
    glfwTerminate();
}

int main(void)
{
    if (atexit(onExit) != 0)
        PANIC("%s\n", "Failed to register atexit function");

    if (!glfwInit())
        PANIC("%s\n", "Failed to initialize GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (window == NULL)
        PANIC("%s\n", "Failed to create GLFW window");

    u32 glfwExtensionCount = 0;
    const char** glfwExtensionNames = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensionNames == NULL)
        PANIC("%s\n", "System does not provide the Vulkan instance extensions required by GLFW");

    u32 enabledExtensionCount = instanceExtensionCount + glfwExtensionCount;
    const char** enabledExtensionNames = mallocOrDie(enabledExtensionCount * sizeof(char*));
    memcpy(enabledExtensionNames, instanceExtensionNames, instanceExtensionCount * sizeof(char*));
    memcpy(enabledExtensionNames + instanceExtensionCount, glfwExtensionNames, glfwExtensionCount * sizeof(char*));

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
        .enabledLayerCount = validationLayerCount,
        .ppEnabledLayerNames = validationLayerNames,
        .enabledExtensionCount = enabledExtensionCount,
        .ppEnabledExtensionNames = enabledExtensionNames
    };

    VkInstance instance;
    if (vkCreateInstance(&createInfo, NULL, &instance) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create Vulkan instance");

    freeAndNull(enabledExtensionNames);

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create window surface");

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

            physicalDevice = physicalDevices[i];
            queueFamilyIndex = j;
            break;
        }

        freeAndNull(queueFamilyProperties);
    }

    if (physicalDevice == VK_NULL_HANDLE)
        PANIC("%s\n", "Failed to find a suitable physical device");

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities) != VK_SUCCESS)
        PANIC("%s\n", "Failed to determine physical device surface capabilities");
    
    VkExtent2D surfaceExtent = surfaceCapabilities.currentExtent;
    if (surfaceExtent.width == UINT32_MAX)
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        u32 minWidth = surfaceCapabilities.minImageExtent.width;
        u32 maxWidth = surfaceCapabilities.maxImageExtent.width;
        surfaceExtent.width = (u32)(width > minWidth) ? width : minWidth;
        surfaceExtent.width = (u32)(width < maxWidth) ? width : maxWidth;

        u32 minHeight = surfaceCapabilities.minImageExtent.height;
        u32 maxHeight = surfaceCapabilities.maxImageExtent.height;
        surfaceExtent.height = (u32)(height > minHeight) ? height : minHeight;
        surfaceExtent.height = (u32)(height < maxHeight) ? height : maxHeight;
    }

    u32 surfaceFormatCount;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, NULL) != VK_SUCCESS)
        PANIC("%s\n", "Failed to determine physical device surface format count");

    if (surfaceFormatCount == 0)
        PANIC("%s\n", "Physical device does not support any surface formats");
    
    VkSurfaceFormatKHR* surfaceFormats = mallocOrDie(sizeof(VkSurfaceFormatKHR) * surfaceFormatCount);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats) != VK_SUCCESS)
        PANIC("%s\n", "Failed to enumerate physical device surface formats");

    VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0];
    for (u32 i = 0; i < surfaceFormatCount; i++)
    {
        if (surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            surfaceFormat = surfaceFormats[i];
            break;
        }
    }

    freeAndNull(surfaceFormats);

    u32 surfacePresentModeCount;
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, NULL) != VK_SUCCESS)
        PANIC("%s\n", "Failed to determine physical device surface present mode count");

    if (surfacePresentModeCount == 0)
        PANIC("%s\n", "Physical device does not support any surface present modes");

    VkPresentModeKHR* surfacePresentModes = mallocOrDie(sizeof(VkPresentModeKHR) * surfacePresentModeCount);
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, NULL) != VK_SUCCESS)
        PANIC("%s\n", "Failed to enumerate physical device surface present modes");

    // NOTE(Hans): The standard FIFO present mode is guaranteed to exist
    VkPresentModeKHR surfacePresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 i = 0; i < surfacePresentModeCount; i++)
    {
        if (surfacePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            surfacePresentMode = surfacePresentModes[i];
            break;
        }
    }

    freeAndNull(surfacePresentModes);

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = validationLayerCount,
        .ppEnabledLayerNames = validationLayerNames,
        .enabledExtensionCount = deviceExtensionCount,
        .ppEnabledExtensionNames = deviceExtensionNames
    };

    VkDevice device;
    vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device);

    physicalDevice = NULL;
    freeAndNull(physicalDevices);

    VkQueue queue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

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

    u32 swapchainImageCount;
    if (vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL) != VK_SUCCESS)
        PANIC("%s\n", "Failed to determine swapchain image count");

    VkImage* swapchainImages = mallocOrDie(sizeof(VkImage) * swapchainImageCount);
    if (vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages) != VK_SUCCESS)
        PANIC("%s\n", "Failed to get handles to swapchain images");

    VkImageView* swapchainImageViews = mallocOrDie(sizeof(VkImageView) * swapchainImageCount);
    for (u32 i = 0; i < swapchainImageCount; i++)
    {
        VkImageViewCreateInfo imageViewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surfaceFormat.format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1
            }
        };
        
        if (vkCreateImageView(device, &imageViewCreateInfo, NULL, &swapchainImageViews[i]) != VK_SUCCESS)
            PANIC("%s\n", "Failed to create image view");
    }

    VkAttachmentDescription colorAttachmentDescription = {
        .format = surfaceFormat.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentReference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpassDescription = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentReference,
    };

    VkSubpassDependency subpassDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo renderPassCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachmentDescription,
        .subpassCount = 1,
        .pSubpasses = &subpassDescription,
        .dependencyCount = 1,
        .pDependencies = &subpassDependency
    };

    VkRenderPass renderPass;
    if (vkCreateRenderPass(device, &renderPassCreateInfo, NULL, &renderPass) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create render pass");

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

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
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
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.f
    };

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE
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
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
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

    VkFramebuffer* swapchainFramebuffers = mallocOrDie(sizeof(VkFramebuffer) * swapchainImageCount);
    for (u32 i = 0; i < swapchainImageCount; i++)
    {
        VkFramebufferCreateInfo framebufferCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass,
            .attachmentCount = 1,
            .pAttachments = &swapchainImageViews[i],
            .width = surfaceExtent.width,
            .height = surfaceExtent.height,
            .layers = 1
        };

        if (vkCreateFramebuffer(device, &framebufferCreateInfo, NULL, &swapchainFramebuffers[i]) != VK_SUCCESS) {
            PANIC("%s\n", "Failed to create framebuffer");
        }
    }

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
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer) != VK_SUCCESS)
        PANIC("%s\n", "Failed to allocate command buffers");

    VkSemaphoreCreateInfo semaphoreCreateInfo = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    if (vkCreateSemaphore(device, &semaphoreCreateInfo, NULL, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreCreateInfo, NULL, &renderFinishedSemaphore) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create semaphores");

    VkFenceCreateInfo fenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VkFence inFlightFence;
    if (vkCreateFence(device, &fenceCreateInfo, NULL, &inFlightFence) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create fence");

    VkViewport viewport = {
        .x = 0.f,
        .y = 0.f,
        .width = (f32)surfaceExtent.width,
        .height = (f32)surfaceExtent.height,
        .minDepth = 0.f,
        .maxDepth = 1.f
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = surfaceExtent
    };

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
            PANIC("%s\n", "Failed to wait for fences");

        if (vkResetFences(device, 1, &inFlightFence) != VK_SUCCESS)
            PANIC("%s\n", "Failed to reset fences");

        u32 imageIndex;
        if (vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex) != VK_SUCCESS)
            PANIC("%s\n", "Failed to acquire next image from swapchain");

        if (vkResetCommandBuffer(commandBuffer, 0) != VK_SUCCESS)
            PANIC("%s\n", "Failed to reset command buffer");
        
        VkCommandBufferBeginInfo commandBufferBeginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
            PANIC("%s\n", "Failed to begin command buffer");

        VkClearValue clearValue = {.color = {0.f, 0.f, 0.f, 1.f}};
        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPass,
            .framebuffer = swapchainFramebuffers[imageIndex],
            .renderArea = {
                .offset = {0, 0},
                .extent = surfaceExtent
            },
            .clearValueCount = 1,
            .pClearValues = &clearValue
        };

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
            PANIC("%s\n", "Failed to end command buffer");
        
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = ARR_LEN(waitSemaphores),
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = ARR_LEN(signalSemaphores),
            .pSignalSemaphores = signalSemaphores
        };

        if (vkQueueSubmit(queue, 1, &submitInfo, inFlightFence) != VK_SUCCESS)
            PANIC("%s\n", "Failed to submit command buffers to queue");

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = ARR_LEN(signalSemaphores),
            .pWaitSemaphores = signalSemaphores,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex
        };

        if (vkQueuePresentKHR(queue, &presentInfo) != VK_SUCCESS)
            PANIC("%s\n", "Failed to queue presentation");
    }

    vkDeviceWaitIdle(device);

    vkDestroySemaphore(device, imageAvailableSemaphore, NULL);
    vkDestroySemaphore(device, renderFinishedSemaphore, NULL);
    vkDestroyFence(device, inFlightFence, NULL);

    vkDestroyCommandPool(device, commandPool, NULL);
    vkDestroyPipeline(device, graphicsPipeline, NULL);
    vkDestroyPipelineLayout(device, pipelineLayout, NULL);
    vkDestroyRenderPass(device, renderPass, NULL);

    for (u32 i = 0; i < swapchainImageCount; i++)
    {
        vkDestroyFramebuffer(device, swapchainFramebuffers[i], NULL);
        vkDestroyImageView(device, swapchainImageViews[i], NULL);
    }

    freeAndNull(swapchainFramebuffers);
    freeAndNull(swapchainImageViews);
    freeAndNull(swapchainImages);

    vkDestroySwapchainKHR(device, swapchain, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);

    return EXIT_SUCCESS;
}
