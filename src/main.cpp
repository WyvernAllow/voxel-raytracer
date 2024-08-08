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
#include <fstream>

static const int MAX_FRAMES_IN_FLIGHT = 2;

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

static std::vector<char> read_file(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		spdlog::error("Failed to read file: {}", filename);
		return std::vector<char>();
	}

	size_t file_size = file.tellg();
	std::vector<char> buffer(file_size);
	file.seekg(0);
	file.read(buffer.data(), file_size);

	file.close();

	return buffer;
}

static VkShaderModule create_shader_module(VkDevice device, const std::vector<char>& code) {
	VkShaderModuleCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = code.size();
	create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shader;
	VkResult result = vkCreateShaderModule(device, &create_info, nullptr, &shader);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create shader module. {}", string_VkResult(result));
		return nullptr;
	}

	return shader;
}

static VkPipelineLayout create_pipeline_layout(VkDevice device) {
	VkPipelineLayoutCreateInfo pipeline_layout_info{};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 0;
	pipeline_layout_info.pSetLayouts = nullptr;
	pipeline_layout_info.pushConstantRangeCount = 0;
	pipeline_layout_info.pPushConstantRanges = nullptr;

	VkPipelineLayout pipeline_layout;
	VkResult result = vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create pipeline layout. {}", string_VkResult(result));
		return nullptr;
	}

	return pipeline_layout;
}

static VkRenderPass create_render_pass(VkDevice device, swapchain_support_details details) {
	VkAttachmentDescription color_attachment{};
	color_attachment.format = details.get_surface_format().format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref{};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;

	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dependency;

	VkRenderPass render_pass;
	VkResult result = vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create render pass. {}", string_VkResult(result));
		return nullptr;
	}

	return render_pass;
}

static VkPipeline create_graphics_pipeline(VkDevice device, VkExtent2D swapchain_extent, VkPipelineLayout layout, VkRenderPass render_pass) {
	auto vert_code = read_file("res/shaders/main.vert.spv");
	auto frag_code = read_file("res/shaders/main.frag.spv");

	VkShaderModule vert_shader = create_shader_module(device, vert_code);
	VkShaderModule frag_shader = create_shader_module(device, frag_code);
	
	VkPipelineShaderStageCreateInfo vert_stage{};
	vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_stage.module = vert_shader;
	vert_stage.pName = "main";

	VkPipelineShaderStageCreateInfo frag_stage{};
	frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_stage.module = frag_shader;
	frag_stage.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages[] = { vert_stage, frag_stage };

	VkPipelineDynamicStateCreateInfo dynamic_state{};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = 0;
	dynamic_state.pDynamicStates = nullptr;

	VkPipelineVertexInputStateCreateInfo vertex_input_info{};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.vertexBindingDescriptionCount = 0;
	vertex_input_info.pVertexBindingDescriptions = nullptr;
	vertex_input_info.vertexAttributeDescriptionCount = 0;
	vertex_input_info.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo input_assembly{};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapchain_extent.width;
	viewport.height = (float)swapchain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchain_extent;

	VkPipelineViewportStateCreateInfo viewport_state{};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState color_blend_attachment{};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo color_blending{};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.logicOpEnable = VK_FALSE;
	color_blending.attachmentCount = 1;
	color_blending.pAttachments = &color_blend_attachment;

	VkGraphicsPipelineCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.stageCount = 2;
	create_info.pStages = shader_stages;
	create_info.pVertexInputState = &vertex_input_info;
	create_info.pInputAssemblyState = &input_assembly;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterizer;
	create_info.pMultisampleState = &multisampling;
	create_info.pColorBlendState = &color_blending;
	create_info.pDynamicState = &dynamic_state;

	create_info.layout = layout;
	create_info.renderPass = render_pass;
	create_info.subpass = 0;

	VkPipeline graphics_pipeline;
	VkResult result = vkCreateGraphicsPipelines(device, nullptr, 1, &create_info, nullptr, &graphics_pipeline);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create graphics pipeline. {}", string_VkResult(result));
		return nullptr;
	}

	vkDestroyShaderModule(device, vert_shader, nullptr);
	vkDestroyShaderModule(device, frag_shader, nullptr);

	return graphics_pipeline;
}

static std::vector<VkFramebuffer> create_framebuffers(std::vector<VkImageView> image_views, VkDevice device, VkRenderPass render_pass, VkExtent2D extent) {
	std::vector<VkFramebuffer> swapchain_framebuffers;

	for (size_t i = 0; i < image_views.size(); i++) {
		VkImageView attachments[] = {
			image_views[i]
		};

		VkFramebufferCreateInfo framebuffer_info{};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = render_pass;
		framebuffer_info.attachmentCount = 1;
		framebuffer_info.pAttachments = attachments;
		framebuffer_info.width = extent.width;
		framebuffer_info.height = extent.height;
		framebuffer_info.layers = 1;

		VkFramebuffer framebuffer;
		VkResult result = vkCreateFramebuffer(device, &framebuffer_info, nullptr, &framebuffer);
		if (result != VK_SUCCESS) {
			spdlog::error("Failed to create framebuffer. {}", string_VkResult(result));
		}

		swapchain_framebuffers.push_back(framebuffer);
	}
	
	return swapchain_framebuffers;
}

static VkCommandPool create_command_pool(VkDevice device, queue_family_indices indices) {
	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = indices.graphics_family.value();

	VkCommandPool pool;
	VkResult result = vkCreateCommandPool(device, &pool_info, nullptr, &pool);
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to create command pool. {}", string_VkResult(result));
		return nullptr;
	}

	return pool;
}

static std::vector<VkCommandBuffer> create_command_buffers(VkDevice device, VkCommandPool pool, size_t count) {
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = count;

	std::vector<VkCommandBuffer> buffers(count);
	VkResult result = vkAllocateCommandBuffers(device, &alloc_info, buffers.data());
	if (result != VK_SUCCESS) {
		spdlog::error("Failed to allocate command buffer. {}", string_VkResult(result));
		return std::vector<VkCommandBuffer>();
	}

	return buffers;
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

	VkSurfaceFormatKHR surface_format = swapchain_details.get_surface_format();
	VkPresentModeKHR present_mode = swapchain_details.get_present_mode();
	VkExtent2D extent = swapchain_details.get_extent(window);

	std::vector<VkImageView> swapchain_image_views = create_swapchain_image_views(device, swapchain_images, surface_format);
	if (swapchain_image_views.empty()) {
		spdlog::critical("Failed to create swapchain image views.");
		return 1;
	}

	VkPipelineLayout pipeline_layout = create_pipeline_layout(device);
	if (!pipeline_layout) {
		spdlog::critical("Cannot proceed without a pipeline layout.");
		return 1;
	}

	VkRenderPass render_pass = create_render_pass(device, swapchain_details);
	if (!render_pass) {
		spdlog::critical("Cannot proceed without a render pass.");
		return 1;
	}

	VkPipeline pipeline = create_graphics_pipeline(device, extent, pipeline_layout, render_pass);
	if (!pipeline) {
		spdlog::critical("Cannot proceed without a pipeline.");
		return 1;
	}

	std::vector<VkFramebuffer> swapchain_framebuffers = create_framebuffers(swapchain_image_views, device, render_pass, extent);
	if (swapchain_framebuffers.empty()) {
		spdlog::critical("Cannot proceed without framebuffers.");
		return 1;
	}

	VkCommandPool command_pool = create_command_pool(device, physdev_indices);
	if (!command_pool) {
		spdlog::critical("Cannot proceed without a command pool.");
		return 1;
	}

	std::vector<VkCommandBuffer> command_buffers = create_command_buffers(device, command_pool, MAX_FRAMES_IN_FLIGHT);
	if (command_buffers.empty()) {
		spdlog::critical("Failed to create command buffers.");
		return 1;
	}

	std::vector<VkSemaphore> image_available_semaphores(MAX_FRAMES_IN_FLIGHT);
	std::vector<VkSemaphore> render_finished_semaphores(MAX_FRAMES_IN_FLIGHT);
	std::vector<VkFence> in_flight_fences(MAX_FRAMES_IN_FLIGHT);



	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkSemaphoreCreateInfo semaphore_info{};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphores[i]) != VK_SUCCESS ||
			vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]) != VK_SUCCESS) {
			spdlog::critical("Failed to create semaphores.");
			return 1;
		}
	}

	uint32_t current_frame = 0;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		VkFence* in_flight_fence = &in_flight_fences[current_frame];
		VkCommandBuffer* command_buffer = &command_buffers[current_frame];
		VkSemaphore* image_available_semaphore = &image_available_semaphores[current_frame];
		VkSemaphore* render_finished_semaphore = &render_finished_semaphores[current_frame];

		vkWaitForFences(device, 1, in_flight_fence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, in_flight_fence);

		uint32_t image_index = 0;
		vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, *image_available_semaphore, VK_NULL_HANDLE, &image_index);

		vkResetCommandBuffer(*command_buffer, 0);

		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = 0;
		begin_info.pInheritanceInfo = nullptr;

		VkResult begin_result = vkBeginCommandBuffer(*command_buffer, &begin_info);
		if (begin_result != VK_SUCCESS) {
			spdlog::error("Failed to begin recording command buffer.");
			return 1;
		}

		VkRenderPassBeginInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = render_pass;
		render_pass_info.framebuffer = swapchain_framebuffers[image_index];
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = extent;

		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
		render_pass_info.clearValueCount = 1;
		render_pass_info.pClearValues = &clearColor;

		vkCmdBeginRenderPass(*command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(*command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdDraw(*command_buffer, 3, 1, 0, 0);
		vkCmdEndRenderPass(*command_buffer);

		VkResult end_result = vkEndCommandBuffer(*command_buffer);

		if (end_result != VK_SUCCESS) {
			spdlog::error("Failed to record command buffer.");
			return 1;
		}

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore wait_semaphores[] = { *image_available_semaphore };
		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;

		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = command_buffer;

		VkSemaphore signal_semaphores[] = { *render_finished_semaphore };
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;

		if (vkQueueSubmit(graphics_queue, 1, &submit_info, *in_flight_fence) != VK_SUCCESS) {
			spdlog::critical("Failed to submit draw command buffer");
			return 1;
		}

		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;

		VkSwapchainKHR swapchains[] = { swapchain };
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swapchains;
		present_info.pImageIndices = &image_index;

		vkQueuePresentKHR(present_queue, &present_info);

		current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	vkDeviceWaitIdle(device);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, image_available_semaphores[i], nullptr);
		vkDestroySemaphore(device, render_finished_semaphores[i], nullptr);
		vkDestroyFence(device, in_flight_fences[i], nullptr);
	}

	vkDestroyCommandPool(device, command_pool, nullptr);

	for (const auto& framebuffer : swapchain_framebuffers) {
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}

	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyRenderPass(device, render_pass, nullptr);
	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);

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