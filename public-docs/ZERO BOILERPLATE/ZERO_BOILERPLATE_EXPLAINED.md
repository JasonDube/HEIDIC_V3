# Zero-Boilerplate Explained

## What is "Boilerplate"?

**Boilerplate code** is **repetitive, verbose code** that you have to write over and over again, but doesn't really add meaning to your program. It's like paperwork - necessary but tedious.

Think of it like this:
- **Template** = Good! A reusable pattern you fill in with your specific data
- **Boilerplate** = Bad! Writing the same verbose code 100 times because the language/system doesn't help you

---

## Example: Loading a Texture (The Boilerplate Way)

### Current Way (WITH Boilerplate) - You Write ~50 Lines Per Texture:

```cpp
// This is what you have to write NOW in C++/Vulkan:
void load_texture(const char* path, VkDevice device, VkPhysicalDevice physicalDevice, 
                  VkCommandPool commandPool, VkQueue queue, VkImage* image, 
                  VkDeviceMemory* memory, VkImageView* imageView, VkSampler* sampler) {
    
    // 1. Load file from disk
    int width, height, channels;
    unsigned char* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture");
    }
    
    // 2. Calculate image size
    VkDeviceSize imageSize = width * height * 4;
    
    // 3. Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device, physicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);
    
    // 4. Copy pixels to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);
    stbi_image_free(pixels);
    
    // 5. Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = static_cast<uint32_t>(width);
    imageInfo.extent.height = static_cast<uint32_t>(height);
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateImage(device, &imageInfo, nullptr, image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image");
    }
    
    // 6. Allocate image memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, *image, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(device, &allocInfo, nullptr, memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory");
    }
    
    vkBindImageMemory(device, *image, *memory, 0);
    
    // 7. Transition image layout
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = *image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    // 8. Copy buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, *image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    // 9. Transition to shader-readable layout
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    endSingleTimeCommands(device, commandPool, queue, commandBuffer);
    
    // 10. Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = *image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(device, &viewInfo, nullptr, imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image view");
    }
    
    // 11. Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    
    if (vkCreateSampler(device, &samplerInfo, nullptr, sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler");
    }
    
    // 12. Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

// And then you call it:
VkImage brickTexture;
VkDeviceMemory brickTextureMemory;
VkImageView brickTextureView;
VkSampler brickTextureSampler;

load_texture("textures/brick.png", device, physicalDevice, commandPool, queue,
             &brickTexture, &brickTextureMemory, &brickTextureView, &brickTextureSampler);

// Want another texture? Write it all again!
VkImage grassTexture;
VkDeviceMemory grassTextureMemory;
VkImageView grassTextureView;
VkSampler grassTextureSampler;

load_texture("textures/grass.png", device, physicalDevice, commandPool, queue,
             &grassTexture, &grassTextureMemory, &grassTextureView, &grassTextureSampler);

// ... repeat for every single texture you need
```

**That's ~200 lines of code just to load 2 textures!** And you have to:
- Pass 5+ Vulkan objects to every function
- Manage memory manually
- Handle cleanup
- Write the same code for EVERY texture
- Remember all the Vulkan API details

---

### Future Way (ZERO Boilerplate) - You Write 1 Line Per Texture:

```heidic
// This is what you'd write WITH the resource system:
resource Texture = "textures/brick.png";
resource Texture = "textures/grass.png";
resource Texture = "textures/dirt.png";

fn main(): void {
    // Use them directly - all the Vulkan stuff is automatic!
    draw_texture(brickTexture);
    draw_texture(grassTexture);
    draw_texture(dirtTexture);
}
```

**That's 3 lines total for 3 textures!** 

The HEIDIC compiler generates ALL that boilerplate code automatically. You just say "I want this texture" and the compiler handles:
- File loading
- Vulkan image creation
- Memory allocation
- GPU upload
- Image view creation
- Sampler creation
- Cleanup on shutdown
- Hot-reload support

---

## Real-World Analogy

Think of it like **ordering food**:

### Boilerplate Way (Like Cooking from Scratch):
```
1. Go to grocery store
2. Find ingredients
3. Pay for ingredients
4. Drive home
5. Wash vegetables
6. Chop vegetables
7. Preheat oven
8. Mix ingredients
9. Put in oven
10. Set timer
11. Check temperature
12. Remove from oven
13. Let cool
14. Serve
15. Clean up dishes
16. Wash cutting board
17. Put away ingredients
```

### Zero-Boilerplate Way (Like Ordering Delivery):
```
"One pizza, please."
```

Both get you a pizza, but one way is **50 steps** and the other is **1 line**.

---

## Why "Zero Boilerplate" is Good

**Boilerplate** is bad because:
- ❌ You have to write the same code 100 times
- ❌ It's easy to make mistakes (forget cleanup, wrong memory flags, etc.)
- ❌ It hides your actual logic in noise
- ❌ It's tedious and error-prone

**Zero-boilerplate** means:
- ✅ You write **what you want** (the texture path)
- ✅ The compiler generates **how to do it** (all the Vulkan code)
- ✅ Less code = fewer bugs
- ✅ Focus on game logic, not API details

---

## What About Templates?

**Templates** are different! Templates are **good** - they're reusable patterns:

```heidic
// This is a template (GOOD):
fn load_resource<T>(path: string): T {
    // Reusable code that works for any type T
}

// This is boilerplate (BAD):
VkImageCreateInfo imageInfo{};
imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
imageInfo.imageType = VK_IMAGE_TYPE_2D;
// ... 20 more lines of repetitive setup
```

Zero-boilerplate doesn't remove templates - it **eliminates repetitive setup code** by having the compiler generate it automatically.

---

## Summary

- **Boilerplate** = Tedious, repetitive code you write over and over
- **Zero-boilerplate** = The compiler writes it for you
- **Result** = You write 1 line, compiler generates 200 lines
- **Benefit** = Focus on game logic, not API details

The resource system turns **"50 lines of Vulkan API calls"** into **"1 line: `resource Texture = "path.png";`"**

That's what "zero-boilerplate" means - you get the same functionality with **dramatically less code to write**.

