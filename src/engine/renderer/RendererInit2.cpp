#include "engine/hzpch.h"

#include "engine/renderer/Renderer.h"

#include <set>

uint32_t Renderer::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
	auto memProps = Renderer::Data.PhysicalDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
	{
		if (typeFilter & (1 << i) &&
			(memProps.memoryTypes[i].propertyFlags & properties) == properties)
			return i;
	}

	SPDLOG_ERROR("No suitable memory type found!");
	return uint32_t();
}

bool CheckValidationLayerSupport(const std::vector<const char*>& validationLayers)
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
			return false;
	}

	return true;
}

vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
	for (const auto& fmt : availableFormats)
		if (fmt.format == vk::Format::eB8G8R8A8Srgb &&
			fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			return fmt;
	return availableFormats[0];
}

vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
	/*for (const auto& mode : availablePresentModes)
		if (mode == vk::PresentModeKHR::eMailbox)
			return mode;*/
	return vk::PresentModeKHR::eFifo; // v-sync
}

vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& caps)
{
	if (caps.currentExtent.width != UINT32_MAX)
		return caps.currentExtent;

	int w, h;
	glfwGetFramebufferSize(Renderer::Data.Window, &w, &h);

	vk::Extent2D extent{ uint32_t(w), uint32_t(h) };

	extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
	extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);

	return extent;
}

bool Renderer::IsDeviceSuitable(vk::PhysicalDevice device)
{
	// check extensions
	const auto availableExtensions = device.enumerateDeviceExtensionProperties();
	std::set<std::string> requiredExtensions(Data.DeviceExtensions.begin(), Data.DeviceExtensions.end());

	for (const auto& ext : availableExtensions)
		requiredExtensions.erase(ext.extensionName);
	const bool extensionsSupported = requiredExtensions.empty();

	// check swap chain support
	bool swapChainAdequate = false;
	if (extensionsSupported)
	{
		const auto swapChainSupport = QuerySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
	}

	// features
	vk::PhysicalDeviceFeatures features = device.getFeatures();

	return (
		FindQueueFamilies(device).IsComplete() &&
		extensionsSupported &&
		swapChainAdequate &&
		features.samplerAnisotropy
		);
}

QueueFamilyIndices Renderer::FindQueueFamilies(vk::PhysicalDevice device)
{
	QueueFamilyIndices indices{};

	auto queueFamilies = device.getQueueFamilyProperties();

	uint32_t i = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
			indices.GraphicsFamily = i;
		if (device.getSurfaceSupportKHR(i, Data.Surface))
			indices.PresentFamily = i;

		if (indices.IsComplete())
			break;

		i++;
	}

	return indices;
}

SwapChainSupportDetails Renderer::QuerySwapChainSupport(vk::PhysicalDevice device)
{
	SwapChainSupportDetails details;

	details.Caps = device.getSurfaceCapabilitiesKHR(Data.Surface);
	details.Formats = device.getSurfaceFormatsKHR(Data.Surface);
	details.PresentModes = device.getSurfacePresentModesKHR(Data.Surface);

	return details;
}

bool Renderer::CreateInstance()
{
	// Use validation layers if this is a debug build
#if DEBUG
	Data.ValidationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

	// VkApplicationInfo allows the programmer to specifiy some basic information about the
	// program, which can be useful for layers and tools to provide more debug information.
	vk::ApplicationInfo appInfo = vk::ApplicationInfo()
		.setPApplicationName("Some Application")
		.setApplicationVersion(1)
		.setPEngineName("LunarG SDK")
		.setEngineVersion(1)
		.setApiVersion(VK_API_VERSION_1_0);

	// VkInstanceCreateInfo is where the programmer specifies the layers and/or extensions that
	// are needed.
	uint32_t glfwExtensionCount;
	const char** glfwExtensions =
		glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	vk::InstanceCreateInfo instInfo = vk::InstanceCreateInfo()
		.setFlags(vk::InstanceCreateFlags())
		.setPApplicationInfo(&appInfo)
		.setEnabledExtensionCount(glfwExtensionCount)
		.setPpEnabledExtensionNames(glfwExtensions)
		.setPEnabledLayerNames(Data.ValidationLayers);

	// Create the Vulkan instance.
	try {
		Data.Instance = vk::createInstance(instInfo);
	}
	catch (std::exception e) {
		std::cout << "Could not create a Vulkan instance: " << e.what() << std::endl;
		return false;
	}

	return true;
}

bool Renderer::PickPhysicalDevice()
{
	auto devices = Data.Instance.enumeratePhysicalDevices();

	for (auto& device : devices)
	{
		if (IsDeviceSuitable(device))
		{
			Data.PhysicalDevice = device;
			break;
		}
	}

	if (!Data.PhysicalDevice)
	{
		SPDLOG_ERROR("No suitable graphics device was found!");
		return false;
	}

	Data.QueueIndices = FindQueueFamilies(Data.PhysicalDevice);

	return true;
}

bool Renderer::CreateLogicalDevice()
{
	// queue create info
	float priority = 1.0f;

	std::vector<vk::DeviceQueueCreateInfo> queueInfos;
	std::set<uint32_t> uniqueQueueFamilies{
		Data.QueueIndices.GraphicsFamily.value(),
		Data.QueueIndices.PresentFamily.value(),
	};

	for (auto familyIndex : uniqueQueueFamilies)
		queueInfos.push_back(vk::DeviceQueueCreateInfo({}, familyIndex, 1, &priority));

	// create device
	vk::PhysicalDeviceFeatures deviceFeatures;
	deviceFeatures.setSamplerAnisotropy(true);

	vk::DeviceCreateInfo createInfo;
	createInfo
		.setQueueCreateInfos(queueInfos)
		.setPEnabledExtensionNames(Data.DeviceExtensions)
		.setPEnabledFeatures(&deviceFeatures)
		.setPEnabledLayerNames(Data.ValidationLayers);

	Data.Device = Data.PhysicalDevice.createDevice(createInfo);

	if (!Data.Device)
	{
		SPDLOG_ERROR("Device creation failed!");
		return false;
	}

	// get graphics queue
	Data.GraphicsQueue = Data.Device.getQueue(Data.QueueIndices.GraphicsFamily.value(), 0);

	if (!Data.GraphicsQueue)
	{
		SPDLOG_ERROR("Graphics queue creation failed!");
		return false;
	}

	// get presentation queue
	Data.PresentationQueue = Data.Device.getQueue(Data.QueueIndices.PresentFamily.value(), 0);

	if (!Data.PresentationQueue)
	{
		SPDLOG_ERROR("Presentation queue creation failed!");
		return false;
	}

	return true;
}

bool Renderer::CreateSwapChain()
{
	auto support = QuerySwapChainSupport(Data.PhysicalDevice);

	auto format = ChooseSwapSurfaceFormat(support.Formats);
	auto presentMode = ChooseSwapPresentMode(support.PresentModes);
	auto extent = ChooseSwapExtent(support.Caps);

	uint32_t imageCount = support.Caps.minImageCount + 1;
	if (imageCount > support.Caps.maxImageCount && support.Caps.maxImageCount > 0)
		imageCount = support.Caps.minImageCount;

	vk::SwapchainCreateInfoKHR info;
	info.setSurface(Data.Surface)
		.setMinImageCount(imageCount)
		.setImageFormat(format.format)
		.setImageColorSpace(format.colorSpace)
		.setPresentMode(presentMode)
		.setImageExtent(extent)
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
		.setPreTransform(support.Caps.currentTransform)
		.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
		.setClipped(true);

	std::array<uint32_t, 2> queueFamilyIndices{
		Data.QueueIndices.GraphicsFamily.value(),
		Data.QueueIndices.PresentFamily.value(),
	};

	if (Data.QueueIndices.GraphicsFamily != Data.QueueIndices.PresentFamily)
	{
		info.setImageSharingMode(vk::SharingMode::eConcurrent)
			.setQueueFamilyIndices(queueFamilyIndices);
	}
	else
	{
		info.setImageSharingMode(vk::SharingMode::eExclusive);
	}

	Data.Swapchain = Data.Device.createSwapchainKHR(info);
	if (!Data.Swapchain)
	{
		SPDLOG_ERROR("Failed to create swapchain!");
		return false;
	}

	Data.SwapchainImages = Data.Device.getSwapchainImagesKHR(Data.Swapchain);
	Data.SwapchainFormat = format.format;
	Data.SwapchainExtent = extent;

	return true;
}
