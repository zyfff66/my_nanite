#include <mesh.h>
#include <nanite_mesh.h>

#include <vk.h>
#include <window.h>
#include<camera.h>
#include <timer.h>
#include <assert.h>

// #include<iostream>

using namespace std;
using namespace vk;

const u32 width=1920;
const u32 height=1080;

// #define debug_fps

struct ConstContext
{
    u32 num_swapchain_image;
};

struct FrameContext
{
    mat4 vp_mat;
    mat4 v_mat;
    u32 view_mode;
};

u32 to_uint(f32 x)
{
    return *((u32*)&x);
}

void to_data(const NaniteMesh& nanite_mesh, vector<u32>& data,u32& num_clusters)
{
    data.clear();

    data.push_back(nanite_mesh.clusters.size());// num cluster
    data.push_back(0);
    data.push_back(0);
    data.push_back(0);

    for(auto& cluster:nanite_mesh.clusters)
    {
        data.push_back(cluster.verts.size()); //num verts
        data.push_back(0);// vert offset
        data.push_back(cluster.indexes.size()/3); //num tris
        data.push_back(0);// offset

        // lod bounds
        data.push_back(to_uint(cluster.lod_bounds.center.x));
        data.push_back(to_uint(cluster.lod_bounds.center.y));
        data.push_back(to_uint(cluster.lod_bounds.center.z));
        data.push_back(to_uint(cluster.lod_bounds.radius));

        // parent lod bounds
        Sphere parent_lod_bounds=nanite_mesh.cluster_groups[cluster.group_id].lod_bounds;
        data.push_back(to_uint(parent_lod_bounds.center.x));
        data.push_back(to_uint(parent_lod_bounds.center.y));
        data.push_back(to_uint(parent_lod_bounds.center.z));
        data.push_back(to_uint(parent_lod_bounds.radius));

        // lod error
        f32 max_parent_lod_error=nanite_mesh.cluster_groups[cluster.group_id].max_parent_lod_error;
        data.push_back(to_uint(cluster.lod_error));
        data.push_back(to_uint(max_parent_lod_error));

        // group_id
        data .push_back(cluster.group_id);

        // mip_level
        data.push_back(cluster.mip_level);
    }

    u32 i=0;
    for(auto& cluster:nanite_mesh.clusters)
    {
        u32 offset=4+16*i;

        data[offset+1]=data.size();//vert offset
        for(vec3 p:cluster.verts)
        {
            data.push_back(to_uint(p.x));
            data.push_back(to_uint(p.y));
            data.push_back(to_uint(p.z));
        }

        data[offset+3]=data.size();// tri offset
        for(u32 j=0;j<cluster.indexes.size()/3;j++)
        {
            u32 i0=cluster.indexes[j*3];
            u32 i1=cluster.indexes[j*3+1];
            u32 i2=cluster.indexes[j*3+2];
            assert(i0<256&&i1<256&&i2<256);

            u32 packed_tri=(i0|(i1<<8)|(i2<<16));
            data.push_back(packed_tri);
        }
        i++;
    }
    num_clusters=data[0];

}

int main()
{
    u32 num_clusters=0;
    vector<u32> data;    
# if 1
    Mesh mesh;
    mesh.load("../../asset/dragon.stl");
    NaniteMesh nanite_mesh;
    nanite_mesh.build(mesh);
    to_data(nanite_mesh,data,num_clusters);
#if 1
    nanite_mesh.save(data,"nanite_mesh.txt");
#endif
    // return 0;
#else
    NaniteMesh::load("nanite_mesh.txt",data,num_clusters);
#endif


    vk::init();
    vk_win::Window window=vk_win::Window::create(width,height,"my_nanite");
    window.build_vk_surface(vk::instance());
    vk::init_surface(window.surface,window.width,window.height);

    auto data_buffer=Buffer::create_buffer_with_data(vk::BufferUsage::StorageBuffer,vk::MemoryUsage::Upload,data).unwrap();

    vector<ConstContext> const_contexts={{vk::num_swapchain_image()}};
    auto const_context_buffer=Buffer::create_buffer_with_data(vk::BufferUsage::StorageBuffer,vk::MemoryUsage::Upload,const_contexts).unwrap();

    vector<Buffer> frame_context_buffers(vk::num_swapchain_image());
    for(auto& buffer:frame_context_buffers)
    {
        buffer=Buffer::create_buffer(BufferUsage::StorageBuffer,MemoryUsage::Upload,sizeof(FrameContext)).unwrap();
    }

    vector<u32> id(vk::num_swapchain_image());
    for(u32 i=0;i<vk::num_swapchain_image();i++) 
        id[i]=i;
    
    vector<Buffer> indirect_buffer(vk::num_swapchain_image());
    for(auto& buffer:indirect_buffer)
    {
        buffer=Buffer::create_buffer(BufferUsage::StorageBuffer|BufferUsage::IndirectBuffer,MemoryUsage::Upload,4*sizeof(u32)).unwrap();
    }

    vector<Buffer> visiable_clusters_buffer(vk::num_swapchain_image());
    for(auto& buffer:visiable_clusters_buffer)
    {
        buffer=Buffer::create_buffer(
            BufferUsage::StorageBuffer,
            MemoryUsage::Download,
            // MemoryUsage::GpuOnly,
            (1<<20)
            ).unwrap();
    }

    // 内存分配和绑定
    vk::write_bindless_set(
        0,
        const_context_buffer
    );
    vk::write_bindless_set(
        1,
        indirect_buffer.data(),
        indirect_buffer.size()
    );
    vk::write_bindless_set(
        1+vk::num_swapchain_image(),
        visiable_clusters_buffer.data(),
        visiable_clusters_buffer.size()
    );
    vk::write_bindless_set(
        1+2*vk::num_swapchain_image(),
        frame_context_buffers.data(),
        frame_context_buffers.size()
    );
    vk::write_bindless_set(
        1+3*vk::num_swapchain_image(),
        data_buffer
    );

    auto g_ppl=GraphicsPipeline::new_()
        .render_pass(RenderingCreateInfo{
            .color_attachment_formats={vk::swapchain_image_format()},
            .depth_attachment_format=Format::D32_SFLOAT
        })
        .push_constant_size(4)
        .input_assembly_state(PrimitiveTopology::TriangleList)
        .vertex_shader(ShaderModule::from_file("shader/vertex_shader.spv").unwrap())
        .viewport_state(ViewportState::Default({Viewport::dimension(width,height)}))
        .depth_stencil_state(DepthStencilState::reverse_z_test())
        .fragment_shader(ShaderModule::from_file("shader/fragment_shader.spv").unwrap())
        .build().unwrap();

    auto c_ppl=ComputePipeline::new_()
        .push_constant_size(4)
        .compute_shader(ShaderModule::from_file("shader/compute_shader.spv").unwrap())
        .build().unwrap();

    auto cmd_allocator=CommandBufferAllocator::new_().unwrap();
    vector<CommandBuffer> cmds(vk::num_swapchain_image());

    u32 swapchain_idx=0;

    auto depth_buffer=Image::AttachmentImage(
        width,
        height,
        Format::D32_SFLOAT,
        ImageUsage::DepthStencilAttachment
    ).unwrap();

    for(auto& cmd:cmds){
        cmd=CommandBuffer::new_(cmd_allocator).unwrap()
            .begin(CommandBufferUsage::SimultaneousUse).unwrap()
            .bind_descriptor_sets(PipelineBindPoint::Compute,c_ppl.pipeline_layout,0,vk::bindless_buffer_set())
            .push_constant(c_ppl.pipeline_layout,4,&id[swapchain_idx])
            .pipeline_barrier(Dependency{
                .buffer_barriers={BufferBarrier{
                    .buffer=indirect_buffer[swapchain_idx],
                    .dst_stage=PipelineStage::COMPUTE_SHADER,
                    .dst_access=AccessFlag::SHADER_WRITE
                }}
            })
            .bind_compute_pipeline(c_ppl.handle)
            .dispatch(num_clusters/32+1,1,1)// x维度为num_clusters/32+1的工作组数量
            .pipeline_barrier(Dependency{
                .buffer_barriers={BufferBarrier{
                    .buffer=indirect_buffer[swapchain_idx],
                    .dst_stage=PipelineStage::DRAW_INDIRECT,
                    .dst_access=AccessFlag::INDIRECT_COMMAND_READ
                }},
                .image_barriers={ImageBarrier{
                    .image=vk::swapchain_image(swapchain_idx),
                    .dst_stage=PipelineStage::COLOR_ATTACHMENT_OUTPUT,
                    .dst_access=AccessFlag::COLOR_ATTACHMENT_WRITE,
                    .new_layout=ImageLayout::AttachmentOptimal,
                }}
            })
            .begin_rendering(RenderingInfo{
                .render_area_extent={.x=window.width,.y=window.height},
                .color_attachments={RenderingAttachmentInfo{
                    .image_view=swapchain_image(swapchain_idx).image_view,
                    .load_op=LoadOp::Clear,
                }},
                .depth_attachment=Some(RenderingAttachmentInfo{
                    .image_view=depth_buffer.image_view,
                    .load_op=LoadOp::Clear,
                })
            })
            .bind_descriptor_sets(PipelineBindPoint::Graphics,g_ppl.pipeline_layout,0,vk::bindless_buffer_set())
            .bind_graphics_pipeline(g_ppl.handle)
            .draw_indirect(indirect_buffer[swapchain_idx])
            .end_rendering()
            .pipeline_barrier(Dependency{
                .image_barriers={ImageBarrier{
                    .image=vk::swapchain_image(swapchain_idx),
                    .src_stage=PipelineStage::COLOR_ATTACHMENT_OUTPUT,
                    .src_access=AccessFlag::COLOR_ATTACHMENT_WRITE,
                    .old_layout=ImageLayout::AttachmentOptimal,
                    .new_layout=ImageLayout::PresentSrc
                }}
            })
            .build().unwrap();

        swapchain_idx++;
    }

    struct FrameSync
    {
        Semaphore acq_img=vk::create_semaphore().unwrap();
        Semaphore cmd_exec=vk::create_semaphore().unwrap();
        Fence cpu_wait_cmd=vk::create_fence(true).unwrap();
    }sync[3];
    u32 frame_idx=0,frame_cnt=0;

    Camera camera;
    camera.position=vec3{0,0,10};
    camera.yaw=-90;
    camera.pitch=0;

    window.set_cursor_disabled();
    bool is_cursor_disabled=true;
    dvec2 lst_cursor_pos;
    u32 view_mode=0;

    Timer timer;
    timer.reset();

#ifdef debug_fps
    Timer timer_fps;
    timer_fps.reset();
#endif

    while(!window.should_close()){
        window.poll_events();

        f32 tick_time=timer.us()/1000.f;

#ifdef debug_fps
        printf("frame:%d,fps:%f\n",frame_cnt,1000.f/tick_time);
#endif
        timer.reset();

        // if(window.is_key_down('W')) camera.move_front(tick_time);
        // if(window.is_key_down('S')) camera.move_front(-tick_time);
        // if(window.is_key_down('A')) camera.move_right(-tick_time);
        // if(window.is_key_down('D')) camera.move_right(tick_time);
        if(window.is_key_down('W')) camera.move_front(-tick_time);
        if(window.is_key_down('S')) camera.move_front(tick_time);
        if(window.is_key_down('A')) camera.move_right(tick_time);
        if(window.is_key_down('D')) camera.move_right(-tick_time);

        if(window.is_key_begin_press('J')) view_mode=(view_mode+1)%4;
        if(window.is_key_begin_press('K')) view_mode=(view_mode+4-1)%4;

        if(window.is_key_begin_press('B')){
            window.set_cursor_normal();
            lst_cursor_pos=window.get_cursor_pos();
            is_cursor_disabled=false;
        }
        if(window.is_key_begin_release('B')){
            window.set_cursor_disabled();
            is_cursor_disabled=true;
        }

        dvec2 cursor_pos=window.get_cursor_pos();
        if(frame_cnt==0) lst_cursor_pos=cursor_pos;
        if(is_cursor_disabled)
            camera.rotate_view(cursor_pos.x-lst_cursor_pos.x,cursor_pos.y-lst_cursor_pos.y);
        lst_cursor_pos=cursor_pos;

        mat4 v_mat=camera.view_mat();
        mat4 p_mat=camera.projection_mat(Camera::radians(40),(f32)window.width/window.height);
        mat4 vp_mat=mul(p_mat,v_mat);

        vk::acquire_next_image(Some(sync[frame_idx].acq_img),None(),swapchain_idx);
        vk::wait_for_fence(sync[frame_idx].cpu_wait_cmd);
        vk::reset_fence(sync[frame_idx].cpu_wait_cmd);

        FrameContext frame_context{vp_mat,v_mat,view_mode};
        frame_context_buffers[swapchain_idx].update(&frame_context,sizeof(FrameContext));

        vk::queue_submit(
            SubmitInfo{
                .waiting={sync[frame_idx].acq_img},
                .command_buffers={cmds[swapchain_idx]},
                .signal={sync[frame_idx].cmd_exec}
            },
            sync[frame_idx].cpu_wait_cmd
        );

        vk::queue_present(PresentInfo{
            .waiting={sync[frame_idx].cmd_exec},
            .swapchain_image_idx=swapchain_idx
        });

        frame_idx=(frame_idx+1)%3;
        frame_cnt++;

        // break;
    }
#ifdef debug_fps
    f32 all_time=timer_fps.us();
    printf("avg_fps:%f",1000.f/(all_time/1000.f/frame_cnt));
#endif

    vk::cleanup();

    return 0;

}