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

static const std::vector<const char*> device_extensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
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
	std::optional<uint32_t> present_family;

	bool is_complete() const {
		return graphics_family.has_value() && present_family.has_value();
	}
};

static queue_family_indices find_queue_families(VkPhysicalDevice physdev, VkSurfaceKHR surface) {
	queue_family_indices indices;

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physdev, &queue_family_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physdev, &queue_family_count, queue_families.data());

	for (size_t i = 0; i < queue_family_count; i++) {
		if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && !indices.graphics_family.has_value()) {
			indices.graphics_family = i;
		}

		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physdev, i, surface, &present_support);

		if (present_support && !indices.present_family.has_value()) {
			indices.present_family = i;
		}
	}

	return indices;
}

struct swapchain_support_details {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> present_modes;

	bool is_adequate() const {
		return !formats.empty() && !present_modes.empty();
	}

	VkSurfaceFormatKHR get_surface_format() {
		for (const auto& format : formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return format;
			}
		}

		return formats[0];
	}

	VkPresentModeKHR get_present_mode() {
		for (const auto& mode : present_modes) {
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return mode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D get_extent(GLFWwindow* window) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}
		else {
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			VkExtent2D actual_width = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actual_width.width = std::clamp(actual_width.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actual_width.height = std::clamp(actual_width.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actual_width;
		}
	}
};

static swapchain_support_details get_swapchain_details(VkPhysicalDevice physdev, VkSurfaceKHR surface) {
	swapchain_support_details details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physdev, surface, &details.capabilities);

	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physdev, surface, &format_count, nullptr);

	if (format_count != 0) {
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physdev, surface, &format_count, details.formats.data());
	}

	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physdev, surface, &present_mode_count, nullptr);

	if (present_mode_count != 0) {
		details.present_modes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physdev, surface, &present_mode_count, details.present_modes.data());
	}

	return details;
}

static bool check_device_extension_support(VkPhysicalDevice device) {
	uint32_t extension_count;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

	std::vector<VkExtensionProperties> available_extensions(extension_count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

	std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());

	for (const auto& extension : available_extensions) {
		required_extensions.erase(extension.extensionName);
	}

	return required_extensions.empty();
}

static int rate_physical_device(VkPhysicalDevice physdev, VkSurfaceKHR surface) {
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physdev, &properties);

	queue_family_indices indices = find_queue_families(physdev, surface);
	if (!indices.is_complete()) {
		return -1;
	}

	swapchain_support_details swapchain_details = get_swapchain_details(physdev, surface);
	if (!swapchain_details.is_adequate()) {
		return -1;
	}

	int score = 0;

	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
		score += 100;
	}

	return score;
}

static VkPhysicalDevice find_physical_device(VkInstance instance, VkSurfaceKHR surface) {
	uint32_t physical_device_count = 0;
	vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);

	std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
	vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data());

	VkPhysicalDevice best_device = nullptr;
	int highest_score = std::numeric_limits<int>::min();
	for (VkPhysicalDevice physdev : physical_devices) {
		int score = rate_physical_device(physdev, surface);
		if (score > highest_score) {
			highest_score = score;
			best_device = physdev;
		}
	}

	return best_device;
}

static VkDevice create_device(VkPhysicalDevice physdev, queue_family_indices indices) {
	std::vector< VkDeviceQueueCreateInfo> queue_create_infos;

	std::set<uint32_t> unique_queue_families = { indices.graphics_family.value(), indices.present_family.value() };
	float queue_priority = 1.0f;

	for (uint32_t queue_family_index : unique_queue_families) {
		VkDeviceQueueCreateInfo queue_create_info{};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_family_index;
		queue_create_info.queueCount = 1;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}

	VkPhysicalDeviceFeatures device_features{};
	
	VkDeviceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.queueCreateInfoCount = queue_create_infos.size();
	create_info.pEnabledFeatures = &device_features;

	create_info.enabledExtensionCount = 0;

	create_info.enabledLayerCount = validation_layers.size();
	create_info.ppEnabledLayerNames = validation_layers.data();

	create_info.enabledExtensionCount = device_extensions.size();
	create_info.ppEnabledExtensionNames = device_extensions.data();

	VkDevice device;
	VkResult result = vkCreateDevice(physdev, &create_info, nullptr, &device);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create device. {}", string_VkResult(result));
		return nullptr;
	}

	return device;
}

static VkSurfaceKHR create_surface(GLFWwindow* window, VkInstance instance) {
	VkSurfaceKHR surface;
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create surface. {}", string_VkResult(result));
		return nullptr;
	}

	return surface;
}

static VkSwapchainKHR create_swapchain(VkDevice device, GLFWwindow* window, VkSurfaceKHR surface, queue_family_indices indices, swapchain_support_details details) {
	VkSurfaceFormatKHR surface_format = details.get_surface_format();
	VkPresentModeKHR present_mode = details.get_present_mode();
	VkExtent2D extent = details.get_extent(window);

	uint32_t image_count = details.capabilities.minImageCount + 1;
	if (details.capabilities.maxImageCount > 0 && image_count > details.capabilities.maxImageCount) {
		image_count = details.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t index_array[] = {indices.graphics_family.value(), indices.present_family.value()};

	if (indices.graphics_family.value() != indices.present_family.value()) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = index_array;
	}
	else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = nullptr;
	}

	create_info.preTransform = details.capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;

	VkSwapchainKHR swapchain;
	VkResult result = vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create swapchain. {}", string_VkResult(result));
		return nullptr;
	}

	return swapchain;
}

static std::vector<VkImageView> create_swapchain_image_views(VkDevice device, std::vector<VkImage> swapchain_images, VkSurfaceFormatKHR surface_format) {
	std::vector<VkImageView> image_views;

	for (size_t i = 0; i < swapchain_images.size(); i++) {
		VkImageViewCreateInfo view_info{};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.image = swapchain_images[i];
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = surface_format.format;
		view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;
		
		VkImageView view;
		VkResult result = vkCreateImageView(device, &view_info, nullptr, &view);
		if (result != VK_SUCCESS) {
			spdlog::error("Failed to create image view.");
		}

		image_views.push_back(view);
	}

	return image_views;
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

	VkSurfaceKHR surface = create_surface(window, instance);
	if (!surface) {
		spdlog::critical("Cannot proceed without a surface.");
		return 1;
	}

	VkPhysicalDevice physdev = find_physical_device(instance, surface);
	if (!physdev) {
		spdlog::critical("Failed to find a suitable physical device.");
		return 1;
	}

	VkPhysicalDeviceProperties physdev_props;
	vkGetPhysicalDeviceProperties(physdev, &physdev_props);

	spdlog::info("Selected physical device name: {}", physdev_props.deviceName);
	spdlog::info("Selected physical device type: {}", string_VkPhysicalDeviceType(physdev_props.deviceType));

	queue_family_indices physdev_indices = find_queue_families(physdev, surface);
	spdlog::info("Graphics family index: {}", physdev_indices.graphics_family.value());
	spdlog::info("Present family index: {}", physdev_indices.present_family.value());

	swapchain_support_details swapchain_details = get_swapchain_details(physdev, surface);

	VkDevice device = create_device(physdev, physdev_indices);
	if (!device) {
		spdlog::critical("Cannot proceed without a device.");
		return 1;
	}

	VkQueue graphics_queue;
	vkGetDeviceQueue(device, physdev_indices.graphics_family.value(), 0, &graphics_queue);

	VkQueue present_queue;
	vkGetDeviceQueue(device, physdev_indices.present_family.value(), 0, &present_queue);

	VkSwapchainKHR swapchain = create_swapchain(device, window, surface, physdev_indices, swapchain_details);
	if (!swapchain) {
		spdlog::critical("Cannot proceed without a swapchain.");
		return 1;
	}

	uint32_t swapchain_image_count = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr);

	std::vector<VkImage> swapchain_images(swapchain_image_count);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images.data());

	std::vector<VkImageView> swapchain_image_views = create_swapchain_image_views(device, swapchain_images, swapchain_details.get_surface_format());
	if (swapchain_image_views.empty()) {
		spdlog::critical("Failed to create swapchain image views.");
		return 1;
	}

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}

	for (const auto& view : swapchain_image_views) {
		vkDestroyImageView(device, view, nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	DestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}