#include "types.h"

#include <stdlib.h>
#include <stdio.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

int main(void)
{
    if (!glfwInit())
        return EXIT_FAILURE;

    u32 windowWidth = 1280;
    u32 windhowHeight = 720;
    cstr windowTitle = "scop";

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(windowWidth, windhowHeight, windowTitle, NULL, NULL);

    if (window == NULL)
    {
        glfwTerminate();
        return EXIT_FAILURE;
    }

    u32 extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    printf("%d extensions supported\n", extensionCount);

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}