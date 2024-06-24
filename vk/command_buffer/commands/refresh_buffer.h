#pragma once
#include "../../buffer/buffer.h"

namespace vk{
    void refresh_buffer(u64 cmd,Buffer buffer,u32 size);
    void copy_buffer(u64 cmd,Buffer src_buffer,Buffer dst_buffer,u64 src_offset, u64 dst_offset,u64 size);
}
