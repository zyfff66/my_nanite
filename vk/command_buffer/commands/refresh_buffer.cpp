#include "refresh_buffer.h"
#include <vulkan/vulkan.h>

namespace vk{

    void refresh_buffer(u64 cmd,Buffer buffer,u32 size)
    {
        vkCmdFillBuffer((VkCommandBuffer)cmd,(VkBuffer)buffer.handle, 0, size, 0);
    }

    void copy_buffer(u64 cmd,Buffer src_buffer,Buffer dst_buffer,u64 src_offset, u64 dst_offset,u64 size)
    {
        VkBufferCopy copy_region={};
        copy_region.size=size;
        copy_region.dstOffset=dst_offset;
        copy_region.srcOffset=src_offset;

        vkCmdCopyBuffer((VkCommandBuffer)cmd, (VkBuffer)src_buffer.handle, (VkBuffer)dst_buffer.handle, 1, &copy_region);
    }

}