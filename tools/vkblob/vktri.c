// Samsung's own Vulkan driver (vulkan.samsung.so, the Android HAL) drawing one
// triangle into a 64x64 offscreen image, run in an Android chroot on mainline
// amdgpu. Ground truth for the Xclipse 920 draw path: if this draws, the kernel
// is fine and Mesa's state/shaders are wrong.
// Build: make (clang + lld against the phone's bionic, no NDK)
// Run:   chroot /android/root /data/vktri [vendor lib path]
#include <stddef.h>
#include <stdint.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "spv.h"

int printf(const char *, ...);
void *dlopen(const char *, int);
void *dlsym(void *, const char *);
char *dlerror(void);
int usleep(unsigned);
void *memset(void *, int, size_t);
char *getenv(const char *);

struct hw_module_t;
struct hw_device_t {
	uint32_t tag, version;
	struct hw_module_t *module;
	uint64_t reserved[12];
	int (*close)(struct hw_device_t *);
};
struct hw_module_methods_t {
	int (*open)(const struct hw_module_t *, const char *, struct hw_device_t **);
};
struct hw_module_t {
	uint32_t tag;
	uint16_t module_api_version, hal_api_version;
	const char *id, *name, *author;
	struct hw_module_methods_t *methods;
	void *dso;
	uint64_t reserved[32 - 7];
};
struct hwvulkan_device_t {
	struct hw_device_t common;
	PFN_vkEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties;
	PFN_vkCreateInstance CreateInstance;
	PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
};

#define W 64
#define H 64
#define CK(x) do { VkResult r_ = (x); if (r_) { printf("FAIL %s = %d\n", #x, r_); return 1; } } while (0)

static PFN_vkGetInstanceProcAddr gipa;
static VkInstance inst;
#define IF(n) PFN_vk##n n = (PFN_vk##n)gipa(inst, "vk" #n)

int main(int argc, char **argv)
{
#ifdef LOADER	/* glibc build: any Vulkan driver through the loader (Mesa RADV) */
	void *h = dlopen("libvulkan.so.1", 2);
	if (!h) { printf("dlopen: %s\n", dlerror()); return 1; }
	gipa = dlsym(h, "vkGetInstanceProcAddr");
	PFN_vkCreateInstance CreateInstance = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
	printf("step: loader\n");
#else
	const char *lib = argc > 1 ? argv[1] : "/vendor/lib64/hw/vulkan.samsung.so";
	void *h = dlopen(lib, 2);
	if (!h) { printf("dlopen: %s\n", dlerror()); return 1; }
	struct hw_module_t *m = dlsym(h, "HMI");
	struct hwvulkan_device_t *vd;
	if (!m || m->methods->open(m, "vk0", (struct hw_device_t **)&vd)) { printf("HAL open failed\n"); return 1; }
	printf("step: HAL %s by %s\n", m->name, m->author);
	gipa = vd->GetInstanceProcAddr;
	PFN_vkCreateInstance CreateInstance = vd->CreateInstance;
#endif

	VkApplicationInfo ai = { VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1 };
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai };
	CK(CreateInstance(&ici, NULL, &inst));
	IF(EnumeratePhysicalDevices); IF(GetPhysicalDeviceProperties); IF(GetPhysicalDeviceMemoryProperties);
	IF(GetPhysicalDeviceQueueFamilyProperties); IF(CreateDevice); IF(GetDeviceProcAddr);
	uint32_t n = 1;
	VkPhysicalDevice pd;
	CK(EnumeratePhysicalDevices(inst, &n, &pd));
	VkPhysicalDeviceProperties pp;
	GetPhysicalDeviceProperties(pd, &pp);
	printf("step: %s vendor %04x device %04x driver %08x\n", pp.deviceName, pp.vendorID, pp.deviceID, pp.driverVersion);

	VkQueueFamilyProperties qf[8];
	n = 8;
	GetPhysicalDeviceQueueFamilyProperties(pd, &n, qf);
	uint32_t fam = 0;
	while (fam < n && !(qf[fam].queueFlags & VK_QUEUE_GRAPHICS_BIT))
		fam++;
	float prio = 1;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = fam,
					.queueCount = 1, .pQueuePriorities = &prio };
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci };
	VkDevice d;
	CK(CreateDevice(pd, &dci, NULL, &d));
	printf("step: device (queue family %u)\n", fam);
	PFN_vkGetDeviceProcAddr gdpa = GetDeviceProcAddr;
#define DF(n) PFN_vk##n n = (PFN_vk##n)gdpa(d, "vk" #n)
	DF(GetDeviceQueue); DF(CreateImage); DF(GetImageMemoryRequirements); DF(AllocateMemory); DF(BindImageMemory);
	DF(CreateImageView); DF(CreateBuffer); DF(GetBufferMemoryRequirements); DF(BindBufferMemory); DF(MapMemory);
	DF(CreateRenderPass); DF(CreateFramebuffer); DF(CreateShaderModule); DF(CreatePipelineLayout);
	DF(CreateGraphicsPipelines); DF(CreateCommandPool); DF(AllocateCommandBuffers); DF(BeginCommandBuffer);
	DF(CmdBeginRenderPass); DF(CmdBindPipeline); DF(CmdDraw); DF(CmdEndRenderPass); DF(CmdCopyImageToBuffer); DF(CmdFillBuffer);
	DF(EndCommandBuffer); DF(CreateFence); DF(QueueSubmit); DF(GetFenceStatus); DF(CmdPipelineBarrier);
	VkQueue q;
	GetDeviceQueue(d, fam, 0, &q);

	VkPhysicalDeviceMemoryProperties mp;
	GetPhysicalDeviceMemoryProperties(pd, &mp);
#define MEMTYPE(bits, want) ({ uint32_t t_ = 0; while (t_ < mp.memoryTypeCount && \
	(!((bits) & (1u << t_)) || (mp.memoryTypes[t_].propertyFlags & (want)) != (want))) t_++; t_; })

	VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM, .extent = { W, H, 1 }, .mipLevels = 1, .arrayLayers = 1,
		.samples = 1, .tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT };
	VkImage img;
	CK(CreateImage(d, &ii, NULL, &img));
	VkMemoryRequirements mr;
	GetImageMemoryRequirements(d, img, &mr);
	VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size,
				     .memoryTypeIndex = MEMTYPE(mr.memoryTypeBits, 0) };
	VkDeviceMemory im, bm;
	CK(AllocateMemory(d, &mai, NULL, &im));
	CK(BindImageMemory(d, img, im, 0));
	VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = img,
		.viewType = VK_IMAGE_VIEW_TYPE_2D, .format = ii.format,
		.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
	VkImageView iv;
	CK(CreateImageView(d, &vci, NULL, &iv));

	VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = W * H * 4,
				   .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT };
	VkBuffer buf;
	CK(CreateBuffer(d, &bci, NULL, &buf));
	GetBufferMemoryRequirements(d, buf, &mr);
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = MEMTYPE(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CK(AllocateMemory(d, &mai, NULL, &bm));
	CK(BindBufferMemory(d, buf, bm, 0));
	uint32_t *px;
	CK(MapMemory(d, bm, 0, W * H * 4, 0, (void **)&px));
	for (int i = 0; i < W * H; i++)
		px[i] = 0xdeadbeef;
	printf("step: memory (%u types)\n", mp.memoryTypeCount);

	VkAttachmentDescription ad = { .format = ii.format, .samples = 1, .loadOp = getenv("VKTRI_NOCLEAR") ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL };
	VkAttachmentReference ar = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sd = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				    .colorAttachmentCount = 1, .pColorAttachments = &ar };
	VkRenderPassCreateInfo rpci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 1,
					.pAttachments = &ad, .subpassCount = 1, .pSubpasses = &sd };
	VkRenderPass rp;
	CK(CreateRenderPass(d, &rpci, NULL, &rp));
	VkFramebufferCreateInfo fci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = rp,
		.attachmentCount = 1, .pAttachments = &iv, .width = W, .height = H, .layers = 1 };
	VkFramebuffer fb;
	CK(CreateFramebuffer(d, &fci, NULL, &fb));

	int vary = getenv("VKTRI_VARY") != NULL;	/* per-vertex colour varying */
	VkShaderModuleCreateInfo smi = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
					 .codeSize = vary ? vary_vert_spv_len : tri_vert_spv_len,
					 .pCode = (uint32_t *)(vary ? vary_vert_spv : tri_vert_spv) };
	VkShaderModule vs, fs;
	CK(CreateShaderModule(d, &smi, NULL, &vs));
	smi.codeSize = vary ? vary_frag_spv_len : tri_frag_spv_len;
	smi.pCode = (uint32_t *)(vary ? vary_frag_spv : tri_frag_spv);
	CK(CreateShaderModule(d, &smi, NULL, &fs));
	VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	VkPipelineLayout pl;
	CK(CreatePipelineLayout(d, &plci, NULL, &pl));
	VkPipelineShaderStageCreateInfo st[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main" },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main" } };
	VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
						       .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
	VkViewport vp = { 0, 0, W, H, 0, 1 };
	VkRect2D sc = { { 0, 0 }, { W, H } };
	VkPipelineViewportStateCreateInfo vps = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc };
	VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .lineWidth = 1 };
	VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
						    .rasterizationSamples = 1 };
	VkPipelineColorBlendAttachmentState cba = { .colorWriteMask = 0xf };
	VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
						   .attachmentCount = 1, .pAttachments = &cba };
	VkGraphicsPipelineCreateInfo gpci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .stageCount = 2,
		.pStages = st, .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vps,
		.pRasterizationState = &rs, .pMultisampleState = &ms, .pColorBlendState = &cb, .layout = pl,
		.renderPass = rp };
	VkPipeline pipe;
	CK(CreateGraphicsPipelines(d, VK_NULL_HANDLE, 1, &gpci, NULL, &pipe));
	printf("step: pipeline\n");

	VkCommandPoolCreateInfo cpci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = fam };
	VkCommandPool cp;
	CK(CreateCommandPool(d, &cpci, NULL, &cp));
	VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = cp,
					     .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
	VkCommandBuffer c;
	CK(AllocateCommandBuffers(d, &cbai, &c));
	// dispatchable handle from a HAL driver: the loader would write its table here
	VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	CK(BeginCommandBuffer(c, &bi));
	VkClearValue cv = { .color = { .float32 = { 0, 0, 1, 1 } } };
	VkRenderPassBeginInfo rpbi = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = rp, .framebuffer = fb,
				       .renderArea = sc, .clearValueCount = 1, .pClearValues = &cv };
	CmdBeginRenderPass(c, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
	CmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
	if (!getenv("VKTRI_NODRAW"))
		CmdDraw(c, 3, 1, 0, 0);
	CmdEndRenderPass(c);
	VkBufferImageCopy bic = { .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, .imageExtent = { W, H, 1 } };
	if (getenv("VKTRI_FILL"))	/* buffer-only compute path instead of the image copy */
		CmdFillBuffer(c, buf, 0, W * H * 4, 0x12345678);
	else
		CmdCopyImageToBuffer(c, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &bic);
	VkMemoryBarrier mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			       .dstAccessMask = VK_ACCESS_HOST_READ_BIT };
	CmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
	CK(EndCommandBuffer(c));

	VkFenceCreateInfo fnci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkFence f;
	CK(CreateFence(d, &fnci, NULL, &f));
	VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &c };
	CK(QueueSubmit(q, 1, &si, f));
	printf("step: submitted\n");
	int i;
	for (i = 0; i < 100 && GetFenceStatus(d, f) == VK_NOT_READY; i++)
		usleep(50000);
	if (i == 100) { printf("RESULT: fence timeout (draw hang)\n"); return 2; }

	int red = 0, blue = 0, other = 0;
	for (i = 0; i < W * H; i++) {
		if (px[i] == 0xff0000ff) red++;
		else if (px[i] == 0xffff0000) blue++;
		else other++;
	}
	int fill = 0;
	for (i = 0; i < W * H; i++)
		fill += px[i] == 0x12345678;
	if (fill)
		printf("fill: %d/%d dwords\n", fill, W * H);
	if (vary)	/* centroid colour: a mix of all three vertex colours */
		printf("vary: center %08x  near v0 %08x  near v1 %08x\n", px[H / 2 * W + W / 2],
		       px[(H - 9) * W + 9], px[(H - 9) * W + W - 10]);
	printf("RESULT: fence signaled; red %d blue %d other %d (center %08x corner %08x)\n",
	       red, blue, other, px[H / 2 * W + W / 2], px[0]);
	return 0;
}
