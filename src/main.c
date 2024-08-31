#include "util.h"

#include <stdlib.h>
#include <string.h>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define WINDOW_TITLE "scop"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

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

    const char* portabilityExtensionNames[] = {
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };
    u32 portabilityExtensionCount = ARR_LEN(portabilityExtensionNames);

    u32 enabledExtensionCount = glfwExtensionCount + portabilityExtensionCount;
    const char** enabledExtensionNames = mallocOrDie(enabledExtensionCount * sizeof(char*));
    memcpy(enabledExtensionNames, glfwExtensionNames, glfwExtensionCount * sizeof(char*));
    memcpy(enabledExtensionNames + glfwExtensionCount, portabilityExtensionNames, portabilityExtensionCount * sizeof(char*));

    const char* enabledLayerNames[] = {
#if DEBUG
        "VK_LAYER_KHRONOS_validation"
#endif
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
        .enabledLayerCount = ARR_LEN(enabledLayerNames),
        .ppEnabledLayerNames = enabledLayerNames,
        .enabledExtensionCount = enabledExtensionCount,
        .ppEnabledExtensionNames = enabledExtensionNames
    };

    VkInstance instance;
    if (vkCreateInstance(&createInfo, NULL, &instance) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create Vulkan instance");

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS)
        PANIC("%s\n", "Failed to create window surface");

    u32 physicalDeviceCount = 0;
    if (vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, NULL) != VK_SUCCESS)
        PANIC("%s\n", "Failed to determine physical device count");

    VkPhysicalDevice* physicalDevices = mallocOrDie(sizeof(VkPhysicalDevice) * physicalDeviceCount);
    if (vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices) != VK_SUCCESS)
        PANIC("%s\n", "Failed to enumerate physical devices");

    VkPhysicalDevice selectedPhysicalDevice = VK_NULL_HANDLE;
    u32 queueFamilyIndex = 0;
    for (usize i = 0; i < physicalDeviceCount && selectedPhysicalDevice == VK_NULL_HANDLE; i++)
    {
        u32 queueFamilyPropertyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyPropertyCount, NULL);
        VkQueueFamilyProperties* queueFamilyProperties = mallocOrDie(sizeof(VkQueueFamilyProperties) * queueFamilyPropertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyPropertyCount, queueFamilyProperties);

        for (usize j = 0; j < queueFamilyPropertyCount; j++)
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevices[i], j, surface, &presentSupport);
            if (presentSupport && (queueFamilyProperties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                selectedPhysicalDevice = physicalDevices[i];
                queueFamilyIndex = j;
                break;
            }
        }

        freeAndNull(queueFamilyProperties);
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    const char* deviceExtensionNames[] = {"VK_KHR_portability_subset"};
    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = ARR_LEN(enabledLayerNames),
        .ppEnabledLayerNames = enabledLayerNames,
        .enabledExtensionCount = ARR_LEN(deviceExtensionNames),
        .ppEnabledExtensionNames = deviceExtensionNames
    };

    VkDevice device;
    vkCreateDevice(selectedPhysicalDevice, &deviceCreateInfo, NULL, &device);

    selectedPhysicalDevice = NULL;
    freeAndNull(physicalDevices);

    VkQueue queue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);

    return EXIT_SUCCESS;
}