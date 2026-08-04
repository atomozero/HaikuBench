/*
 * Copyright 2025 Andrea Bernardi
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "VulkanBenchWindow.h"

#include <Alert.h>
#include <Box.h>
#include <Button.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <SeparatorView.h>
#include <Window.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "VulkanBench"


// Dynamic Vulkan function pointers
static PFN_vkGetInstanceProcAddr						dvkGetInstanceProcAddr;
static PFN_vkCreateInstance								dvkCreateInstance;
static PFN_vkDestroyInstance							dvkDestroyInstance;
static PFN_vkEnumeratePhysicalDevices					dvkEnumeratePhysicalDevices;
static PFN_vkGetPhysicalDeviceProperties				dvkGetPhysicalDeviceProperties;
static PFN_vkGetPhysicalDeviceMemoryProperties			dvkGetPhysicalDeviceMemoryProperties;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties		dvkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkCreateDevice								dvkCreateDevice;
static PFN_vkDestroyDevice								dvkDestroyDevice;
static PFN_vkGetDeviceQueue								dvkGetDeviceQueue;
static PFN_vkCreateCommandPool							dvkCreateCommandPool;
static PFN_vkDestroyCommandPool							dvkDestroyCommandPool;
static PFN_vkAllocateCommandBuffers						dvkAllocateCommandBuffers;
static PFN_vkFreeCommandBuffers							dvkFreeCommandBuffers;
static PFN_vkBeginCommandBuffer							dvkBeginCommandBuffer;
static PFN_vkEndCommandBuffer							dvkEndCommandBuffer;
static PFN_vkQueueSubmit								dvkQueueSubmit;
static PFN_vkQueueWaitIdle								dvkQueueWaitIdle;
static PFN_vkCreateBuffer								dvkCreateBuffer;
static PFN_vkDestroyBuffer								dvkDestroyBuffer;
static PFN_vkGetBufferMemoryRequirements				dvkGetBufferMemoryRequirements;
static PFN_vkAllocateMemory								dvkAllocateMemory;
static PFN_vkFreeMemory									dvkFreeMemory;
static PFN_vkBindBufferMemory							dvkBindBufferMemory;
static PFN_vkMapMemory									dvkMapMemory;
static PFN_vkUnmapMemory								dvkUnmapMemory;
static PFN_vkFlushMappedMemoryRanges					dvkFlushMappedMemoryRanges;
static PFN_vkInvalidateMappedMemoryRanges				dvkInvalidateMappedMemoryRanges;
static PFN_vkCmdCopyBuffer								dvkCmdCopyBuffer;
static PFN_vkCmdFillBuffer								dvkCmdFillBuffer;
static PFN_vkCreateComputePipelines						dvkCreateComputePipelines;
static PFN_vkDestroyPipeline							dvkDestroyPipeline;
static PFN_vkCreatePipelineLayout						dvkCreatePipelineLayout;
static PFN_vkDestroyPipelineLayout						dvkDestroyPipelineLayout;
static PFN_vkCreateShaderModule							dvkCreateShaderModule;
static PFN_vkDestroyShaderModule						dvkDestroyShaderModule;
static PFN_vkCreateDescriptorSetLayout					dvkCreateDescriptorSetLayout;
static PFN_vkDestroyDescriptorSetLayout					dvkDestroyDescriptorSetLayout;
static PFN_vkCreateDescriptorPool						dvkCreateDescriptorPool;
static PFN_vkDestroyDescriptorPool						dvkDestroyDescriptorPool;
static PFN_vkAllocateDescriptorSets						dvkAllocateDescriptorSets;
static PFN_vkUpdateDescriptorSets						dvkUpdateDescriptorSets;
static PFN_vkCmdBindPipeline							dvkCmdBindPipeline;
static PFN_vkCmdBindDescriptorSets						dvkCmdBindDescriptorSets;
static PFN_vkCmdDispatch								dvkCmdDispatch;
static PFN_vkCmdPipelineBarrier							dvkCmdPipelineBarrier;
static PFN_vkCreateFence								dvkCreateFence;
static PFN_vkDestroyFence								dvkDestroyFence;
static PFN_vkWaitForFences								dvkWaitForFences;
static PFN_vkResetFences								dvkResetFences;
static PFN_vkResetCommandBuffer							dvkResetCommandBuffer;

static image_id sVulkanImage = -1;


static const rgb_color kDarkBg = {13, 17, 23, 255};
static const rgb_color kGreen = {57, 211, 83, 255};
static const rgb_color kWhite = {230, 237, 243, 255};
static const rgb_color kGray = {170, 180, 195, 255};
static const rgb_color kLabelWhite = {200, 210, 225, 255};
static const rgb_color kYellow = {230, 200, 50, 255};
static const rgb_color kCyan = {80, 200, 220, 255};


static const char* kTestNames[kNumVkTests] = {
	"Memory Bandwidth",
	"Compute Shader",
	"Buffer Copy (dev)",
	"Buffer Fill Rate"
};

static const char* kTestUnits[kNumVkTests] = {
	"MB/s",
	"GFLOPS",
	"MB/s",
	"MB/s"
};


static BStringView*
_MakeLabel(const char* name, const char* text, const rgb_color& color,
	float fontSize = 11.0f, bool bold = false)
{
	BStringView* view = new BStringView(name, text);
	view->SetViewColor(kDarkBg);
	view->SetLowColor(kDarkBg);
	view->SetHighColor(color);

	BFont font;
	view->GetFont(&font);
	font.SetSize(fontSize);
	if (bold)
		font.SetFace(B_BOLD_FACE);
	view->SetFont(&font);

	return view;
}


// Load Vulkan library dynamically
static bool
_LoadVulkan()
{
	if (sVulkanImage >= 0)
		return true;

	sVulkanImage = load_add_on("libvulkan.so.1");
	if (sVulkanImage < 0)
		sVulkanImage = load_add_on("libvulkan.so");
	if (sVulkanImage < 0)
		return false;

	if (get_image_symbol(sVulkanImage, "vkGetInstanceProcAddr",
			B_SYMBOL_TYPE_TEXT, (void**)&dvkGetInstanceProcAddr) != B_OK) {
		unload_add_on(sVulkanImage);
		sVulkanImage = -1;
		return false;
	}

	// Global functions (no instance needed)
	dvkCreateInstance = (PFN_vkCreateInstance)
		dvkGetInstanceProcAddr(NULL, "vkCreateInstance");

	return dvkCreateInstance != NULL;
}


static void
_LoadInstanceFunctions(VkInstance instance)
{
	#define LOAD(fn) dvk##fn = (PFN_vk##fn) \
		dvkGetInstanceProcAddr(instance, "vk" #fn)

	LOAD(DestroyInstance);
	LOAD(EnumeratePhysicalDevices);
	LOAD(GetPhysicalDeviceProperties);
	LOAD(GetPhysicalDeviceMemoryProperties);
	LOAD(GetPhysicalDeviceQueueFamilyProperties);
	LOAD(CreateDevice);
	LOAD(DestroyDevice);
	LOAD(GetDeviceQueue);
	LOAD(CreateCommandPool);
	LOAD(DestroyCommandPool);
	LOAD(AllocateCommandBuffers);
	LOAD(FreeCommandBuffers);
	LOAD(BeginCommandBuffer);
	LOAD(EndCommandBuffer);
	LOAD(QueueSubmit);
	LOAD(QueueWaitIdle);
	LOAD(CreateBuffer);
	LOAD(DestroyBuffer);
	LOAD(GetBufferMemoryRequirements);
	LOAD(AllocateMemory);
	LOAD(FreeMemory);
	LOAD(BindBufferMemory);
	LOAD(MapMemory);
	LOAD(UnmapMemory);
	LOAD(FlushMappedMemoryRanges);
	LOAD(InvalidateMappedMemoryRanges);
	LOAD(CmdCopyBuffer);
	LOAD(CmdFillBuffer);
	LOAD(CreateComputePipelines);
	LOAD(DestroyPipeline);
	LOAD(CreatePipelineLayout);
	LOAD(DestroyPipelineLayout);
	LOAD(CreateShaderModule);
	LOAD(DestroyShaderModule);
	LOAD(CreateDescriptorSetLayout);
	LOAD(DestroyDescriptorSetLayout);
	LOAD(CreateDescriptorPool);
	LOAD(DestroyDescriptorPool);
	LOAD(AllocateDescriptorSets);
	LOAD(UpdateDescriptorSets);
	LOAD(CmdBindPipeline);
	LOAD(CmdBindDescriptorSets);
	LOAD(CmdDispatch);
	LOAD(CmdPipelineBarrier);
	LOAD(CreateFence);
	LOAD(DestroyFence);
	LOAD(WaitForFences);
	LOAD(ResetFences);
	LOAD(ResetCommandBuffer);

	#undef LOAD
}


// Find memory type index
static uint32
_FindMemoryType(VkPhysicalDevice physDev, uint32 typeBits,
	VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProps;
	dvkGetPhysicalDeviceMemoryProperties(physDev, &memProps);

	for (uint32 i = 0; i < memProps.memoryTypeCount; i++) {
		if ((typeBits & (1 << i))
			&& (memProps.memoryTypes[i].propertyFlags & properties)
				== properties) {
			return i;
		}
	}
	return UINT32_MAX;
}


// SPIR-V compute shader (compiled from GLSL):
//   #version 450
//   layout(local_size_x = 256) in;
//   layout(std430, binding = 0) buffer Buf { float data[]; };
//   void main() {
//       uint i = gl_GlobalInvocationID.x;
//       float v = data[i];
//       for (int j = 0; j < 256; j++) {
//           v = fma(v, 1.0001, 0.0001);  // 2 FLOP per iteration
//       }
//       data[i] = v;
//   }
// Compiled with: glslangValidator -V -o comp.spv comp.glsl
// Then extracted as uint32 array
static bool
_CompileComputeShader(VkDevice device, VkShaderModule* module)
{
	// Use shaderc at runtime to compile GLSL to SPIR-V
	// This avoids embedding binary SPIR-V and works across driver versions
	const char* glsl =
		"#version 450\n"
		"layout(local_size_x = 256) in;\n"
		"layout(std430, binding = 0) buffer Buf { float data[]; };\n"
		"void main() {\n"
		"    uint i = gl_GlobalInvocationID.x;\n"
		"    float v = data[i];\n"
		"    for (int j = 0; j < 256; j++) {\n"
		"        v = v * 1.00001 + 0.00001;\n"
		"    }\n"
		"    data[i] = v;\n"
		"}\n";

	// Write temp file, compile with glslangValidator
	FILE* f = fopen("/tmp/hbench_comp.glsl", "w");
	if (f == NULL)
		return false;
	fputs(glsl, f);
	fclose(f);

	int ret = system("glslc -fshader-stage=compute "
		"-o /tmp/hbench_comp.spv /tmp/hbench_comp.glsl 2>/dev/null");
	if (ret != 0) {
		// Fallback to glslangValidator
		ret = system("glslangValidator -V -o /tmp/hbench_comp.spv "
			"/tmp/hbench_comp.glsl 2>/dev/null");
	}
	if (ret != 0) {
		remove("/tmp/hbench_comp.glsl");
		return false;
	}

	// Read SPIR-V binary
	f = fopen("/tmp/hbench_comp.spv", "rb");
	if (f == NULL)
		return false;

	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);

	uint32* code = (uint32*)malloc(size);
	if (code == NULL) {
		fclose(f);
		return false;
	}
	size_t bytesRead = fread(code, 1, size, f);
	fclose(f);

	if (bytesRead != size) {
		free(code);
		remove("/tmp/hbench_comp.glsl");
		remove("/tmp/hbench_comp.spv");
		return false;
	}

	VkShaderModuleCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = size;
	ci.pCode = code;

	VkResult r = dvkCreateShaderModule(device, &ci, NULL, module);
	free(code);

	// Clean up temp files
	remove("/tmp/hbench_comp.glsl");
	remove("/tmp/hbench_comp.spv");

	return r == VK_SUCCESS;
}


// #pragma mark - VulkanBenchPanel


VulkanBenchPanel::VulkanBenchPanel(BMessenger target)
	:
	BView("vulkanPanel", B_WILL_DRAW | B_SUPPORTS_LAYOUT),
	fTarget(target),
	fBenchThread(-1),
	fInstance(NULL),
	fPhysicalDevice(NULL),
	fDevice(NULL),
	fCommandPool(NULL),
	fCommandBuffer(NULL),
	fQueueFamily(0),
	fQueue(NULL)
{
	fResults = {};

	SetViewColor(kDarkBg);
	BView* topView = this;

	// System info panel (BBox with title like MainWindow)
	BBox* sysBox = new BBox("vkSysBox");
	sysBox->SetViewColor(kDarkBg);
	sysBox->SetLowColor(kDarkBg);

	BStringView* sysTitle = _MakeLabel("vkSysTitle",
		B_TRANSLATE("Vulkan Device"), kLabelWhite, 12.0f, true);
	sysTitle->SetViewColor(kDarkBg);
	sysTitle->SetLowColor(kDarkBg);
	sysBox->SetLabel(sysTitle);

	BView* sysInner = new BView("vkSysInner", B_WILL_DRAW);
	sysInner->SetViewColor(kDarkBg);

	fDeviceLabel = _MakeLabel("dev", B_TRANSLATE("Device: detecting..."),
		kWhite, 11.0f, true);
	fDriverLabel = _MakeLabel("drv", B_TRANSLATE("Driver: --"),
		kCyan, 11.0f);

	BLayoutBuilder::Group<>(sysInner, B_VERTICAL, 2)
		.SetInsets(8, 8, 8, 8)
		.Add(fDeviceLabel)
		.Add(fDriverLabel)
	.End();
	sysBox->AddChild(sysInner);

	// Results panel (BBox with title like MainWindow)
	BBox* resBox = new BBox("vkResBox");
	resBox->SetViewColor(kDarkBg);
	resBox->SetLowColor(kDarkBg);

	BStringView* resTitle = _MakeLabel("vkResTitle",
		B_TRANSLATE("Benchmark Results"), kLabelWhite, 12.0f, true);
	resTitle->SetViewColor(kDarkBg);
	resTitle->SetLowColor(kDarkBg);
	resBox->SetLabel(resTitle);

	BView* resInner = new BView("vkResInner", B_WILL_DRAW);
	resInner->SetViewColor(kDarkBg);

	BStringView* header = _MakeLabel("hdr", B_TRANSLATE("Compute Tests"),
		kCyan, 11.0f, true);

	BFont monoFont(be_fixed_font);
	monoFont.SetSize(11.0f);

	for (int32 i = 0; i < kNumVkTests; i++) {
		BString name;
		name.SetToFormat("vkTest%" B_PRId32, i);
		BString text;
		text.SetToFormat("  %-22s       --       ", kTestNames[i]);
		fTestLabels[i] = _MakeLabel(name.String(), text.String(),
			kGray, 11.0f);
		fTestLabels[i]->SetFont(&monoFont);
	}

	fScoreLabel = _MakeLabel("score",
		"  Overall Score           --       ", kGreen, 11.0f, true);
	{
		BFont scoreMono(be_fixed_font);
		scoreMono.SetSize(11.0f);
		scoreMono.SetFace(B_BOLD_FACE);
		fScoreLabel->SetFont(&scoreMono);
	}

	BLayoutBuilder::Group<>(resInner, B_VERTICAL, 1)
		.SetInsets(8, 8, 8, 8)
		.Add(header)
		.Add(fTestLabels[0])
		.Add(fTestLabels[1])
		.Add(fTestLabels[2])
		.Add(fTestLabels[3])
		.Add(new BSeparatorView(B_HORIZONTAL))
		.Add(fScoreLabel)
	.End();
	resBox->AddChild(resInner);

	// Status + button
	fStatusLabel = _MakeLabel("status",
		B_TRANSLATE("Offscreen compute tests. No window surface needed."),
		kGray, 10.0f);

	BButton* startButton = new BButton("startBtn",
		B_TRANSLATE("Run Vulkan Benchmark"), new BMessage(kMsgVkBenchStart));

	BLayoutBuilder::Group<>(topView, B_VERTICAL, 6)
		.SetInsets(12, 12, 12, 12)
		.Add(sysBox)
		.Add(resBox)
		.AddGlue()
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_HORIZONTAL, 5)
			.Add(fStatusLabel)
			.AddGlue()
			.Add(startButton)
		.End()
	.End();
}


VulkanBenchPanel::~VulkanBenchPanel()
{
	if (fBenchThread >= 0) {
		status_t result;
		wait_for_thread(fBenchThread, &result);
	}
	_Cleanup();
}


void
VulkanBenchPanel::AttachedToWindow()
{
	BView::AttachedToWindow();
	// The Run button's message must reach this panel, not the window looper.
	if (BButton* button = dynamic_cast<BButton*>(FindView("startBtn")))
		button->SetTarget(this);
}


void
VulkanBenchPanel::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgVkBenchStart:
			if (fBenchThread >= 0)
				break;
			fStatusLabel->SetText(B_TRANSLATE("Initializing Vulkan..."));
			fStatusLabel->SetHighColor(kYellow);
			fBenchThread = spawn_thread(_BenchThread, "vulkan_bench",
				B_NORMAL_PRIORITY, this);
			if (fBenchThread >= 0)
				resume_thread(fBenchThread);
			break;

		default:
			BView::MessageReceived(message);
			break;
	}
}


bool
VulkanBenchPanel::_InitVulkan()
{
	if (!_LoadVulkan())
		return false;

	// Create instance
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "HaikuBench";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "HaikuBench";
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instCI = {};
	instCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instCI.pApplicationInfo = &appInfo;

	VkInstance instance;
	if (dvkCreateInstance(&instCI, NULL, &instance) != VK_SUCCESS)
		return false;
	fInstance = instance;

	_LoadInstanceFunctions(instance);

	// Enumerate physical devices
	uint32 devCount = 0;
	dvkEnumeratePhysicalDevices(instance, &devCount, NULL);
	if (devCount == 0) {
		dvkDestroyInstance(instance, NULL);
		fInstance = NULL;
		return false;
	}

	VkPhysicalDevice* devices = new VkPhysicalDevice[devCount];
	dvkEnumeratePhysicalDevices(instance, &devCount, devices);
	VkPhysicalDevice physDev = devices[0]; // Use first device
	fPhysicalDevice = physDev;
	delete[] devices;

	// Get device properties
	VkPhysicalDeviceProperties props;
	dvkGetPhysicalDeviceProperties(physDev, &props);
	fResults.deviceName = props.deviceName;

	BString drvVer;
	drvVer.SetToFormat("%d.%d.%d",
		VK_VERSION_MAJOR(props.driverVersion),
		VK_VERSION_MINOR(props.driverVersion),
		VK_VERSION_PATCH(props.driverVersion));

	const char* devType = "Unknown";
	switch (props.deviceType) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: devType = "iGPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: devType = "dGPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: devType = "vGPU"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: devType = "CPU"; break;
		default: break;
	}
	fResults.driverInfo.SetToFormat("%s | %s | Vulkan %d.%d",
		devType, drvVer.String(),
		VK_VERSION_MAJOR(props.apiVersion),
		VK_VERSION_MINOR(props.apiVersion));

	// Find compute queue family
	uint32 qfCount = 0;
	dvkGetPhysicalDeviceQueueFamilyProperties(physDev, &qfCount, NULL);
	VkQueueFamilyProperties* qfProps = new VkQueueFamilyProperties[qfCount];
	dvkGetPhysicalDeviceQueueFamilyProperties(physDev, &qfCount, qfProps);

	fQueueFamily = UINT32_MAX;
	for (uint32 i = 0; i < qfCount; i++) {
		if (qfProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			fQueueFamily = i;
			break;
		}
	}
	delete[] qfProps;

	if (fQueueFamily == UINT32_MAX) {
		_Cleanup();
		return false;
	}

	// Create logical device
	float priority = 1.0f;
	VkDeviceQueueCreateInfo queueCI = {};
	queueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCI.queueFamilyIndex = fQueueFamily;
	queueCI.queueCount = 1;
	queueCI.pQueuePriorities = &priority;

	VkDeviceCreateInfo devCI = {};
	devCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	devCI.queueCreateInfoCount = 1;
	devCI.pQueueCreateInfos = &queueCI;

	VkDevice device;
	if (dvkCreateDevice(physDev, &devCI, NULL, &device) != VK_SUCCESS) {
		_Cleanup();
		return false;
	}
	fDevice = device;

	VkQueue queue;
	dvkGetDeviceQueue(device, fQueueFamily, 0, &queue);
	fQueue = queue;

	// Create command pool
	VkCommandPoolCreateInfo poolCI = {};
	poolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCI.queueFamilyIndex = fQueueFamily;
	poolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool cmdPool;
	if (dvkCreateCommandPool(device, &poolCI, NULL, &cmdPool) != VK_SUCCESS) {
		_Cleanup();
		return false;
	}
	fCommandPool = cmdPool;

	// Allocate command buffer
	VkCommandBufferAllocateInfo cmdAI = {};
	cmdAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAI.commandPool = cmdPool;
	cmdAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdAI.commandBufferCount = 1;

	VkCommandBuffer cmdBuf;
	if (dvkAllocateCommandBuffers(device, &cmdAI, &cmdBuf) != VK_SUCCESS) {
		_Cleanup();
		return false;
	}
	fCommandBuffer = cmdBuf;

	return true;
}


void
VulkanBenchPanel::_Cleanup()
{
	VkDevice device = (VkDevice)fDevice;
	VkInstance instance = (VkInstance)fInstance;

	if (device != VK_NULL_HANDLE && dvkDestroyDevice != NULL) {
		if (dvkQueueWaitIdle != NULL && fQueue != NULL)
			dvkQueueWaitIdle((VkQueue)fQueue);
		if (fCommandBuffer != NULL && dvkFreeCommandBuffers != NULL)
			dvkFreeCommandBuffers(device, (VkCommandPool)fCommandPool,
				1, (VkCommandBuffer*)&fCommandBuffer);
		if (fCommandPool != NULL && dvkDestroyCommandPool != NULL)
			dvkDestroyCommandPool(device, (VkCommandPool)fCommandPool, NULL);
		dvkDestroyDevice(device, NULL);
	}
	if (instance != VK_NULL_HANDLE && dvkDestroyInstance != NULL)
		dvkDestroyInstance(instance, NULL);

	fDevice = NULL;
	fInstance = NULL;
	fCommandPool = NULL;
	fCommandBuffer = NULL;
	fQueue = NULL;
}


float
VulkanBenchPanel::_TestMemoryBandwidth()
{
	VkDevice device = (VkDevice)fDevice;
	VkPhysicalDevice physDev = (VkPhysicalDevice)fPhysicalDevice;
	const VkDeviceSize bufSize = 64 * 1024 * 1024; // 64 MB

	// Create host-visible buffer
	VkBufferCreateInfo bufCI = {};
	bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufCI.size = bufSize;
	bufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		| VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VkBuffer buffer;
	if (dvkCreateBuffer(device, &bufCI, NULL, &buffer) != VK_SUCCESS)
		return -1.0f;

	VkMemoryRequirements memReq;
	dvkGetBufferMemoryRequirements(device, buffer, &memReq);

	uint32 memIdx = _FindMemoryType(physDev, memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (memIdx == UINT32_MAX) {
		dvkDestroyBuffer(device, buffer, NULL);
		return -1.0f;
	}

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = memIdx;

	VkDeviceMemory memory;
	if (dvkAllocateMemory(device, &allocInfo, NULL, &memory) != VK_SUCCESS) {
		dvkDestroyBuffer(device, buffer, NULL);
		return -1.0f;
	}
	dvkBindBufferMemory(device, buffer, memory, 0);

	// Map and measure write bandwidth
	void* mapped = NULL;
	dvkMapMemory(device, memory, 0, bufSize, 0, &mapped);

	bigtime_t start = system_time();
	int32 iterations = 0;
	bigtime_t deadline = start + 2000000; // 2 seconds

	while (system_time() < deadline) {
		memset(mapped, 0x55, bufSize);
		__asm__ __volatile__("" : : "r"(*(volatile char*)mapped) : "memory");
		iterations++;
	}

	bigtime_t elapsed = system_time() - start;
	dvkUnmapMemory(device, memory);
	dvkFreeMemory(device, memory, NULL);
	dvkDestroyBuffer(device, buffer, NULL);

	if (elapsed < 1000)
		return -1.0f;
	return (float)((double)iterations * (double)bufSize / (double)elapsed);
}


float
VulkanBenchPanel::_TestComputeShader()
{
	VkDevice device = (VkDevice)fDevice;
	VkPhysicalDevice physDev = (VkPhysicalDevice)fPhysicalDevice;
	VkCommandBuffer cmdBuf = (VkCommandBuffer)fCommandBuffer;
	VkQueue queue = (VkQueue)fQueue;

	// Compile shader
	VkShaderModule shaderModule;
	if (!_CompileComputeShader(device, &shaderModule))
		return -1.0f;

	// Create buffer (1M floats = 4 MB)
	const uint32 numElements = 1024 * 1024;
	const VkDeviceSize bufSize = numElements * sizeof(float);

	VkBufferCreateInfo bufCI = {};
	bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufCI.size = bufSize;
	bufCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	VkBuffer buffer;
	if (dvkCreateBuffer(device, &bufCI, NULL, &buffer) != VK_SUCCESS) {
		dvkDestroyShaderModule(device, shaderModule, NULL);
		return -1.0f;
	}

	VkMemoryRequirements memReq;
	dvkGetBufferMemoryRequirements(device, buffer, &memReq);

	uint32 memIdx = _FindMemoryType(physDev, memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (memIdx == UINT32_MAX) {
		dvkDestroyBuffer(device, buffer, NULL);
		dvkDestroyShaderModule(device, shaderModule, NULL);
		return -1.0f;
	}

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = memIdx;

	VkDeviceMemory memory;
	if (dvkAllocateMemory(device, &allocInfo, NULL, &memory) != VK_SUCCESS) {
		dvkDestroyBuffer(device, buffer, NULL);
		dvkDestroyShaderModule(device, shaderModule, NULL);
		return -1.0f;
	}
	dvkBindBufferMemory(device, buffer, memory, 0);

	// Init buffer data
	float* mapped = NULL;
	dvkMapMemory(device, memory, 0, bufSize, 0, (void**)&mapped);
	for (uint32 i = 0; i < numElements; i++)
		mapped[i] = 1.0f;
	dvkUnmapMemory(device, memory);

	// Create descriptor set layout
	VkDescriptorSetLayoutBinding binding = {};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo dslCI = {};
	dslCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslCI.bindingCount = 1;
	dslCI.pBindings = &binding;

	VkDescriptorSetLayout dsLayout;
	if (dvkCreateDescriptorSetLayout(device, &dslCI, NULL, &dsLayout)
			!= VK_SUCCESS) {
		dvkFreeMemory(device, memory, NULL);
		dvkDestroyBuffer(device, buffer, NULL);
		return -1.0f;
	}

	// Pipeline layout
	VkPipelineLayoutCreateInfo plCI = {};
	plCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plCI.setLayoutCount = 1;
	plCI.pSetLayouts = &dsLayout;

	VkPipelineLayout pipeLayout;
	if (dvkCreatePipelineLayout(device, &plCI, NULL, &pipeLayout)
			!= VK_SUCCESS) {
		dvkDestroyDescriptorSetLayout(device, dsLayout, NULL);
		dvkFreeMemory(device, memory, NULL);
		dvkDestroyBuffer(device, buffer, NULL);
		return -1.0f;
	}

	// Compute pipeline
	VkComputePipelineCreateInfo cpCI = {};
	cpCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cpCI.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cpCI.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cpCI.stage.module = shaderModule;
	cpCI.stage.pName = "main";
	cpCI.layout = pipeLayout;

	VkPipeline pipeline;
	if (dvkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpCI,
			NULL, &pipeline) != VK_SUCCESS) {
		dvkDestroyShaderModule(device, shaderModule, NULL);
		dvkDestroyPipelineLayout(device, pipeLayout, NULL);
		dvkDestroyDescriptorSetLayout(device, dsLayout, NULL);
		dvkFreeMemory(device, memory, NULL);
		dvkDestroyBuffer(device, buffer, NULL);
		return -1.0f;
	}

	// Descriptor pool + set
	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo dpCI = {};
	dpCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpCI.maxSets = 1;
	dpCI.poolSizeCount = 1;
	dpCI.pPoolSizes = &poolSize;

	VkDescriptorPool descPool;
	if (dvkCreateDescriptorPool(device, &dpCI, NULL, &descPool)
			!= VK_SUCCESS) {
		dvkDestroyPipeline(device, pipeline, NULL);
		dvkDestroyShaderModule(device, shaderModule, NULL);
		dvkDestroyPipelineLayout(device, pipeLayout, NULL);
		dvkDestroyDescriptorSetLayout(device, dsLayout, NULL);
		dvkFreeMemory(device, memory, NULL);
		dvkDestroyBuffer(device, buffer, NULL);
		return -1.0f;
	}

	VkDescriptorSetAllocateInfo dsAI = {};
	dsAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dsAI.descriptorPool = descPool;
	dsAI.descriptorSetCount = 1;
	dsAI.pSetLayouts = &dsLayout;

	VkDescriptorSet descSet;
	if (dvkAllocateDescriptorSets(device, &dsAI, &descSet)
			!= VK_SUCCESS) {
		dvkDestroyDescriptorPool(device, descPool, NULL);
		dvkDestroyPipeline(device, pipeline, NULL);
		dvkDestroyShaderModule(device, shaderModule, NULL);
		dvkDestroyPipelineLayout(device, pipeLayout, NULL);
		dvkDestroyDescriptorSetLayout(device, dsLayout, NULL);
		dvkFreeMemory(device, memory, NULL);
		dvkDestroyBuffer(device, buffer, NULL);
		return -1.0f;
	}

	VkDescriptorBufferInfo bufInfo = {};
	bufInfo.buffer = buffer;
	bufInfo.offset = 0;
	bufInfo.range = bufSize;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descSet;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.pBufferInfo = &bufInfo;
	dvkUpdateDescriptorSets(device, 1, &write, 0, NULL);

	// Create fence
	VkFenceCreateInfo fenceCI = {};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence;
	if (dvkCreateFence(device, &fenceCI, NULL, &fence) != VK_SUCCESS) {
		dvkDestroyDescriptorPool(device, descPool, NULL);
		dvkDestroyPipeline(device, pipeline, NULL);
		dvkDestroyShaderModule(device, shaderModule, NULL);
		dvkDestroyPipelineLayout(device, pipeLayout, NULL);
		dvkDestroyDescriptorSetLayout(device, dsLayout, NULL);
		dvkFreeMemory(device, memory, NULL);
		dvkDestroyBuffer(device, buffer, NULL);
		return -1.0f;
	}

	// Run benchmark: dispatch many times, measure total time
	const uint32 workGroups = numElements / 256;
	int32 dispatches = 0;

	bigtime_t start = system_time();
	bigtime_t deadline = start + 3000000; // 3 seconds

	while (system_time() < deadline) {
		dvkResetCommandBuffer(cmdBuf, 0);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		dvkBeginCommandBuffer(cmdBuf, &beginInfo);

		dvkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
		dvkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
			pipeLayout, 0, 1, &descSet, 0, NULL);

		// Multiple dispatches per submit to amortize overhead
		for (int32 d = 0; d < 16; d++)
			dvkCmdDispatch(cmdBuf, workGroups, 1, 1);

		dvkEndCommandBuffer(cmdBuf);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;

		dvkResetFences(device, 1, &fence);
		dvkQueueSubmit(queue, 1, &submitInfo, fence);
		dvkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

		dispatches += 16;
	}

	bigtime_t elapsed = system_time() - start;

	// Cleanup
	dvkDestroyFence(device, fence, NULL);
	dvkDestroyDescriptorPool(device, descPool, NULL);
	dvkDestroyPipeline(device, pipeline, NULL);
	dvkDestroyPipelineLayout(device, pipeLayout, NULL);
	dvkDestroyDescriptorSetLayout(device, dsLayout, NULL);
	dvkDestroyShaderModule(device, shaderModule, NULL);
	dvkFreeMemory(device, memory, NULL);
	dvkDestroyBuffer(device, buffer, NULL);

	if (elapsed < 1000 || dispatches == 0)
		return -1.0f;

	// Each dispatch: numElements invocations, each does 256 iterations
	// of v*1.00001+0.00001 = 2 FLOP (mul + add)
	double totalFLOP = (double)dispatches * (double)numElements * 256.0 * 2.0;
	double seconds = (double)elapsed / 1000000.0;
	return (float)(totalFLOP / seconds / 1e9); // GFLOPS
}


float
VulkanBenchPanel::_TestBufferCopy()
{
	VkDevice device = (VkDevice)fDevice;
	VkPhysicalDevice physDev = (VkPhysicalDevice)fPhysicalDevice;
	VkCommandBuffer cmdBuf = (VkCommandBuffer)fCommandBuffer;
	VkQueue queue = (VkQueue)fQueue;

	const VkDeviceSize bufSize = 32 * 1024 * 1024; // 32 MB

	// Create two device-local buffers
	VkBufferCreateInfo bufCI = {};
	bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufCI.size = bufSize;
	bufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
		| VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VkBuffer bufA = VK_NULL_HANDLE, bufB = VK_NULL_HANDLE;
	VkDeviceMemory memA = VK_NULL_HANDLE, memB = VK_NULL_HANDLE;

	if (dvkCreateBuffer(device, &bufCI, NULL, &bufA) != VK_SUCCESS
		|| dvkCreateBuffer(device, &bufCI, NULL, &bufB) != VK_SUCCESS)
		return -1.0f;

	VkMemoryRequirements memReq;
	dvkGetBufferMemoryRequirements(device, bufA, &memReq);

	// Try device local first, fall back to host visible
	uint32 memIdx = _FindMemoryType(physDev, memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (memIdx == UINT32_MAX)
		memIdx = _FindMemoryType(physDev, memReq.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	if (memIdx == UINT32_MAX) {
		dvkDestroyBuffer(device, bufA, NULL);
		dvkDestroyBuffer(device, bufB, NULL);
		return -1.0f;
	}

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = memIdx;

	if (dvkAllocateMemory(device, &allocInfo, NULL, &memA) != VK_SUCCESS
		|| dvkAllocateMemory(device, &allocInfo, NULL, &memB) != VK_SUCCESS) {
		if (memA != VK_NULL_HANDLE) dvkFreeMemory(device, memA, NULL);
		dvkDestroyBuffer(device, bufA, NULL);
		dvkDestroyBuffer(device, bufB, NULL);
		return -1.0f;
	}
	dvkBindBufferMemory(device, bufA, memA, 0);
	dvkBindBufferMemory(device, bufB, memB, 0);

	// Create fence
	VkFenceCreateInfo fenceCI = {};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence;
	if (dvkCreateFence(device, &fenceCI, NULL, &fence) != VK_SUCCESS) {
		dvkFreeMemory(device, memA, NULL);
		dvkFreeMemory(device, memB, NULL);
		dvkDestroyBuffer(device, bufA, NULL);
		dvkDestroyBuffer(device, bufB, NULL);
		return -1.0f;
	}

	int32 copies = 0;
	bigtime_t start = system_time();
	bigtime_t deadline = start + 2000000;

	while (system_time() < deadline) {
		dvkResetCommandBuffer(cmdBuf, 0);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		dvkBeginCommandBuffer(cmdBuf, &beginInfo);

		VkBufferCopy region = {};
		region.size = bufSize;
		dvkCmdCopyBuffer(cmdBuf, bufA, bufB, 1, &region);

		dvkEndCommandBuffer(cmdBuf);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;

		dvkResetFences(device, 1, &fence);
		dvkQueueSubmit(queue, 1, &submitInfo, fence);
		dvkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

		copies++;
	}

	bigtime_t elapsed = system_time() - start;

	dvkDestroyFence(device, fence, NULL);
	dvkFreeMemory(device, memA, NULL);
	dvkFreeMemory(device, memB, NULL);
	dvkDestroyBuffer(device, bufA, NULL);
	dvkDestroyBuffer(device, bufB, NULL);

	if (elapsed < 1000 || copies == 0)
		return -1.0f;
	return (float)((double)copies * (double)bufSize / (double)elapsed);
}


float
VulkanBenchPanel::_TestFillRate()
{
	VkDevice device = (VkDevice)fDevice;
	VkPhysicalDevice physDev = (VkPhysicalDevice)fPhysicalDevice;
	VkCommandBuffer cmdBuf = (VkCommandBuffer)fCommandBuffer;
	VkQueue queue = (VkQueue)fQueue;

	const VkDeviceSize bufSize = 64 * 1024 * 1024; // 64 MB

	VkBufferCreateInfo bufCI = {};
	bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufCI.size = bufSize;
	bufCI.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VkBuffer buffer;
	if (dvkCreateBuffer(device, &bufCI, NULL, &buffer) != VK_SUCCESS)
		return -1.0f;

	VkMemoryRequirements memReq;
	dvkGetBufferMemoryRequirements(device, buffer, &memReq);

	uint32 memIdx = _FindMemoryType(physDev, memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (memIdx == UINT32_MAX)
		memIdx = _FindMemoryType(physDev, memReq.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	if (memIdx == UINT32_MAX) {
		dvkDestroyBuffer(device, buffer, NULL);
		return -1.0f;
	}

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = memIdx;

	VkDeviceMemory memory;
	if (dvkAllocateMemory(device, &allocInfo, NULL, &memory) != VK_SUCCESS) {
		dvkDestroyBuffer(device, buffer, NULL);
		return -1.0f;
	}
	dvkBindBufferMemory(device, buffer, memory, 0);

	VkFenceCreateInfo fenceCI = {};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence;
	if (dvkCreateFence(device, &fenceCI, NULL, &fence) != VK_SUCCESS) {
		dvkFreeMemory(device, memory, NULL);
		dvkDestroyBuffer(device, buffer, NULL);
		return -1.0f;
	}

	int32 fills = 0;
	bigtime_t start = system_time();
	bigtime_t deadline = start + 2000000;

	while (system_time() < deadline) {
		dvkResetCommandBuffer(cmdBuf, 0);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		dvkBeginCommandBuffer(cmdBuf, &beginInfo);

		dvkCmdFillBuffer(cmdBuf, buffer, 0, bufSize, 0xDEADBEEF);

		dvkEndCommandBuffer(cmdBuf);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;

		dvkResetFences(device, 1, &fence);
		dvkQueueSubmit(queue, 1, &submitInfo, fence);
		dvkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

		fills++;
	}

	bigtime_t elapsed = system_time() - start;

	dvkDestroyFence(device, fence, NULL);
	dvkFreeMemory(device, memory, NULL);
	dvkDestroyBuffer(device, buffer, NULL);

	if (elapsed < 1000 || fills == 0)
		return -1.0f;
	return (float)((double)fills * (double)bufSize / (double)elapsed);
}


void
VulkanBenchPanel::_RunBenchmark()
{
	if (!_InitVulkan()) {
		if (LockLooper()) {
			fStatusLabel->SetText(B_TRANSLATE(
				"Vulkan not available. Install vulkan + mesa_lavapipe."));
			fStatusLabel->SetHighColor((rgb_color){255, 80, 80, 255});
			UnlockLooper();
		}
		return;
	}

	if (LockLooper()) {
		BString devText;
		devText.SetToFormat("Device: %s",
			fResults.deviceName.String());
		fDeviceLabel->SetText(devText.String());

		BString drvText;
		drvText.SetToFormat("Driver: %s", fResults.driverInfo.String());
		fDriverLabel->SetText(drvText.String());
		UnlockLooper();
	}

	typedef float (VulkanBenchPanel::*TestFunc)();
	TestFunc tests[] = {
		&VulkanBenchPanel::_TestMemoryBandwidth,
		&VulkanBenchPanel::_TestComputeShader,
		&VulkanBenchPanel::_TestBufferCopy,
		&VulkanBenchPanel::_TestFillRate
	};

	for (int32 i = 0; i < kNumVkTests; i++) {
		if (LockLooper()) {
			BString status;
			status.SetToFormat("Running: %s (%" B_PRId32 "/%d)...",
				kTestNames[i], i + 1, kNumVkTests);
			fStatusLabel->SetText(status.String());
			fStatusLabel->SetHighColor(kYellow);

			BString running;
			running.SetToFormat("  %-22s  running...", kTestNames[i]);
			fTestLabels[i]->SetText(running.String());
			fTestLabels[i]->SetHighColor(kYellow);
			UnlockLooper();
		}

		fResults.scores[i] = (this->*tests[i])();

		if (LockLooper()) {
			BString text;
			if (fResults.scores[i] < 0.0f) {
				text.SetToFormat("  %-22s       N/A %s",
					kTestNames[i], kTestUnits[i]);
				fTestLabels[i]->SetHighColor(kGray);
			} else if (fResults.scores[i] >= 10000.0f) {
				text.SetToFormat("  %-22s  %8.0f %s",
					kTestNames[i], fResults.scores[i], kTestUnits[i]);
				fTestLabels[i]->SetHighColor(kGreen);
			} else {
				text.SetToFormat("  %-22s  %8.1f %s",
					kTestNames[i], fResults.scores[i], kTestUnits[i]);
				fTestLabels[i]->SetHighColor(kGreen);
			}
			fTestLabels[i]->SetText(text.String());
			UnlockLooper();
		}
	}

	// Calculate overall score (geometric mean of valid tests)
	float product = 1.0f;
	int32 validCount = 0;
	for (int32 i = 0; i < kNumVkTests; i++) {
		if (fResults.scores[i] > 0.0f) {
			product *= fResults.scores[i];
			validCount++;
		}
	}
	if (validCount > 0)
		fResults.overallScore = powf(product, 1.0f / validCount);
	fResults.valid = (validCount > 0);

	if (LockLooper()) {
		if (fResults.valid) {
			BString scoreText;
			scoreText.SetToFormat(
				"  Overall Score       %8.1f pts",
				fResults.overallScore);
			fScoreLabel->SetText(scoreText.String());
		}

		fStatusLabel->SetText(B_TRANSLATE("Vulkan benchmark complete!"));
		fStatusLabel->SetHighColor(kGreen);
		UnlockLooper();
	}

	// Send results to main window
	BMessage msg(kMsgVkBenchResult);
	msg.AddFloat("vk_score", fResults.overallScore);
	msg.AddString("vk_device", fResults.deviceName);
	msg.AddString("vk_driver", fResults.driverInfo);
	for (int32 i = 0; i < kNumVkTests; i++)
		msg.AddFloat("vk_test", fResults.scores[i]);
	fTarget.SendMessage(&msg);
}


/*static*/ int32
VulkanBenchPanel::_BenchThread(void* data)
{
	VulkanBenchPanel* window = static_cast<VulkanBenchPanel*>(data);
	window->_RunBenchmark();

	if (window->LockLooper()) {
		window->fBenchThread = -1;
		window->UnlockLooper();
	}

	return 0;
}
