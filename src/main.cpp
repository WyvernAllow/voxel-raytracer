#include <spdlog/spdlog.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <limits>

static const std::vector<const char*> validation_layers = {
	"VK_LAYER_KHRONOS_validation"
};

static VKAPI_ATTR VkBool32 VKAPI_CALL on_vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* data,
	void* user_data) {

	switch (severity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		spdlog::trace("Vulkan: {}", data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		spdlog::info("Vulkan: {}", data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		spdlog::warn("Vulkan: {}", data->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		spdlog::error("Vulkan: {}", data->pMessage);
		break;
	}

	return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

static VkDebugUtilsMessengerCreateInfoEXT get_debug_utils_messenger_create_info() {
	VkDebugUtilsMessengerCreateInfoEXT create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

	create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

	create_info.pfnUserCallback = on_vk_debug_callback;
	create_info.pUserData = nullptr;

	return create_info;
}

static void on_glfw_error(int error, const char* description) {
	spdlog::error("GLFW Error: {}", description);
}

static VkInstance create_instance(VkDebugUtilsMessengerCreateInfoEXT& debug_utils_messenger_create_info) {
	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instance_info{};
	instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_info.pApplicationInfo = &app_info;

	uint32_t glfw_extension_count = 0;
	const char** glfw_extensions;

	glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

	std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	instance_info.enabledExtensionCount = extensions.size();
	instance_info.ppEnabledExtensionNames = extensions.data();

	instance_info.enabledLayerCount = validation_layers.size();
	instance_info.ppEnabledLayerNames = validation_layers.data();

	instance_info.pNext = &debug_utils_messenger_create_info;

	VkInstance instance;
	VkResult result = vkCreateInstance(&instance_info, nullptr, &instance);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create VkInstance. {}", string_VkResult(result));
		return nullptr;
	}

	return instance;
}

static VkDebugUtilsMessengerEXT create_debug_utils_messenger(VkInstance instance, VkDebugUtilsMessengerCreateInfoEXT& debug_utils_messenger_create_info) {
	VkDebugUtilsMessengerEXT messenger;
	VkResult result = CreateDebugUtilsMessengerEXT(instance, &debug_utils_messenger_create_info, nullptr, &messenger);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create VkDebugUtilsMessengerEXT. {}", string_VkResult(result));
		return nullptr;
	}

	return messenger;
}

struct queue_family_indices {
	std::optional<uint32_t> graphics_family;

	bool is_complete() const {
		return graphics_family.has_value();
	}
};

static queue_family_indices find_queue_families(VkPhysicalDevice physdev) {
	queue_family_indices indices;

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physdev, &queue_family_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physdev, &queue_family_count, queue_families.data());

	for (size_t i = 0; i < queue_family_count; i++) {
		if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && !indices.graphics_family.has_value()) {
			indices.graphics_family = i;
		}
	}

	return indices;
}

static int rate_physical_device(VkPhysicalDevice physdev) {
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physdev, &properties);

	queue_family_indices indices = find_queue_families(physdev);
	if (!indices.is_complete()) {
		return -1;
	}

	int score = 0;

	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
		score += 100;
	}

	return score;
}

static VkPhysicalDevice find_physical_device(VkInstance instance) {
	uint32_t physical_device_count = 0;
	vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);

	std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
	vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data());

	VkPhysicalDevice best_device = nullptr;
	int highest_score = std::numeric_limits<int>::min();
	for (VkPhysicalDevice physdev : physical_devices) {
		int score = rate_physical_device(physdev);
		if (score > highest_score) {
			highest_score = score;
			best_device = physdev;
		}
	}

	return best_device;
}

static VkDevice create_device(VkPhysicalDevice physdev, queue_family_indices indices) {
	VkDeviceQueueCreateInfo queue_create_info{};
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex = indices.graphics_family.value();
	queue_create_info.queueCount = 1;

	float queue_priority = 1.0f;
	queue_create_info.pQueuePriorities = &queue_priority;

	VkPhysicalDeviceFeatures device_features{};
	
	VkDeviceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = &queue_create_info;
	create_info.queueCreateInfoCount = 1;
	create_info.pEnabledFeatures = &device_features;

	create_info.enabledExtensionCount = 0;

	create_info.enabledLayerCount = validation_layers.size();
	create_info.ppEnabledLayerNames = validation_layers.data();

	VkDevice device;
	VkResult result = vkCreateDevice(physdev, &create_info, nullptr, &device);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create device. {}", string_VkResult(result));
		return nullptr;
	}

	return device;
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
	
	VkDebugUtilsMessengerCreateInfoEXT debug_utils_info = get_debug_utils_messenger_create_info();

	VkInstance instance = create_instance(debug_utils_info);
	if (!instance) {
		spdlog::critical("Cannot proceed without a valid instance.");
		return 1;
	}

	VkDebugUtilsMessengerEXT debug_messenger = create_debug_utils_messenger(instance, debug_utils_info);
	if (!debug_messenger) {
		spdlog::critical("Cannot proceed without a debug utils messenger.");
		return 1;
	}

	VkPhysicalDevice physdev = find_physical_device(instance);
	if (!physdev) {
		spdlog::critical("Failed to find a suitable physical device.");
		return 1;
	}

	VkPhysicalDeviceProperties physdev_props;
	vkGetPhysicalDeviceProperties(physdev, &physdev_props);

	spdlog::info("Selected physical device name: {}", physdev_props.deviceName);
	spdlog::info("Selected physical device type: {}", string_VkPhysicalDeviceType(physdev_props.deviceType));

	queue_family_indices physdev_indices = find_queue_families(physdev);
	spdlog::info("Graphics family index: {}", physdev_indices.graphics_family.value());

	VkDevice device = create_device(physdev, physdev_indices);
	if (!device) {
		spdlog::critical("Cannot proceed without a device.");
		return 1;
	}

	VkQueue graphics_queue;
	vkGetDeviceQueue(device, physdev_indices.graphics_family.value(), 0, &graphics_queue);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}

	vkDestroyDevice(device, nullptr);
	DestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}