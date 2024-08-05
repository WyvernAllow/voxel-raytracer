#include <spdlog/spdlog.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

static void on_glfw_error(int error, const char* description) {
	spdlog::error("GLFW Error: {}", description);
}

int main(int argc, char** argv) {
	glfwSetErrorCallback(on_glfw_error);

	if (!glfwInit()) {
		spdlog::critical("Failed to initialize GLFW.");
		return 1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(800, 450, "Voxel Raytracer", nullptr, nullptr);
	if (!window) {
		spdlog::critical("Failed to create window.");
		return 1;
	}

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}