#include <mesh.h>
#include <nanite_mesh.h>

#include <vk.h>
#include <window.h>
#include <camera.h>
#include <timer.h>
#include <assert.h>

using namespace std;
using namespace vk;
using namespace vk_win;

const u32 width=1920;
const u32 height=1080;

#define debug_fps

struct ConstContext
{
    u32 num_swapchain_image;
};

struct FrameContext
{
    mat4 vp_mat;
    mat4 v_mat;
    mat4 p_mat;
    mat4 vp_mat2;
    u32 view_mode;
};

u32 high_bit(u32 x)
{
    u32 res=0,t=16,y=0;
    y=-((x>>t)!=0),res+=y&t,x>>=y&t,t>>=1;
    y=-((x>>t)!=0),res+=y&t,x>>=y&t,t>>=1;
    y=-((x>>t)!=0),res+=y&t,x>>=y&t,t>>=1;
    y=-((x>>t)!=0),res+=y&t,x>>=y&t,t>>=1;
    y=(x>>t)!=0,res+=y;
    return res;
}

u32 to_int(f32 x)
{
    return *((u32*)&x);
}

f32 to_float(u32 x)
{
    return *((f32*)&x);
}

void to_data(const NaniteMesh& nanite_mesh, vector<u32>& data,u32& num_clusters,u32& num_groups)
{
    data.clear();
    data.push_back(nanite_mesh.clusters.size());// num_cluster
    data.push_back(nanite_mesh.cluster_groups.size());// num_groups
    data.push_back(0);// group_data_ofs
    data.push_back(0);

    for(auto& cluster:nanite_mesh.clusters)
    {
        data.push_back(cluster.verts.size());// num_vert
        data.push_back(0);// v_data_ofs
        data.push_back(cluster.indexes.size()/3);// num_tri
        data.push_back(0);// t_data_ofs

        // bounds
        data.push_back(to_int(cluster.sphere_bounds.center.x));
        data.push_back(to_int(cluster.sphere_bounds.center.y));
        data.push_back(to_int(cluster.sphere_bounds.center.z));
        data.push_back(to_int(cluster.sphere_bounds.radius));

        // lod bounds
        data.push_back(to_int(cluster.lod_bounds.center.x));
        data.push_back(to_int(cluster.lod_bounds.center.y));
        data.push_back(to_int(cluster.lod_bounds.center.z));
        data.push_back(to_int(cluster.lod_bounds.radius));

        // parent lod bounds
        Sphere parent_lod_bounds=nanite_mesh.cluster_groups[cluster.group_id].lod_bounds;
        data.push_back(to_int(parent_lod_bounds.center.x));
        data.push_back(to_int(parent_lod_bounds.center.y));
        data.push_back(to_int(parent_lod_bounds.center.z));
        data.push_back(to_int(parent_lod_bounds.radius));

        f32 max_parent_lod_error=nanite_mesh.cluster_groups[cluster.group_id].max_parent_lod_error;
        data.push_back(to_int(cluster.lod_error));
        data.push_back(to_int(max_parent_lod_error));
        data.push_back(cluster.group_id);
        data.push_back(cluster.mip_level);
    }

    // group 数据
    data[2]=data.size();// group_data_ofs
    for(auto& group:nanite_mesh.cluster_groups)
    {
        data.push_back(group.clusters.size());// num_cluster of group
        data.push_back(0);// cluster data ofs
        data.push_back(to_int(group.max_parent_lod_error));
        data.push_back(0);

        // lod bounds
        data.push_back(to_int(group.lod_bounds.center.x));
        data.push_back(to_int(group.lod_bounds.center.y));
        data.push_back(to_int(group.lod_bounds.center.z));
        data.push_back(to_int(group.lod_bounds.radius));
    }

    u32 i=0;
    for(auto& cluster:nanite_mesh.clusters)
    {
        u32 ofs=4+20*i;
        data[ofs+1]=data.size();
        for(vec3 p:cluster.verts)
        {
            data.push_back(to_int(p.x));
            data.push_back(to_int(p.y));
            data.push_back(to_int(p.z));
        }

        data[ofs+3]=data.size();
        for(int j=0;j<cluster.indexes.size()/3;j++)
        {
            u32 j0=cluster.indexes[j*3];
            u32 j1=cluster.indexes[j*3+1];
            u32 j2=cluster.indexes[j*3+2];
            assert(j0<256&&j1<256&&j2<256);

            u32 tri=(j0|(j1<<8)|(j2<<16));
            data.push_back(tri);
        }
        i++;
    }

    i=0;
    for(auto& group:nanite_mesh.cluster_groups)
    {
        u32 ofs=data[2]+8*i;
        data[ofs+1]=data.size();
        for(u32 x:group.clusters)
        {
            data.push_back(x);
        }
        i++;
    }
    num_clusters=data[0];
    num_groups=data[1];
}
int main()
{
    u32 num_clusters=0;
    u32 num_groups=0;
    vector<u32> data;  
#if 0
    Mesh mesh;
    mesh.load("../../asset/dragon.stl");
    NaniteMesh nanite_mesh;
    nanite_mesh.build(mesh);
    to_data(nanite_mesh,data,num_clusters,num_groups);
#if 1
    nanite_mesh.save(data,"nanite_mesh.txt");
#endif  
    return 0;
#else
    NaniteMesh::load("nanite_mesh.txt",data,num_clusters);
    num_groups=data[1];
#endif
    vk::init();
    Window window=Window::create(width,height,"my_nanite");
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

    vector<Buffer> indirect_buffer(vk::num_swapchain_image());
    for(auto& buffer:indirect_buffer)
    {
        buffer=Buffer::create_buffer(BufferUsage::StorageBuffer|BufferUsage::IndirectBuffer,MemoryUsage::Download,4*sizeof(u32)).unwrap();
    }

    vector<Buffer> visiable_clusters_buffer(vk::num_swapchain_image());
    for(auto& buffer:visiable_clusters_buffer)
    {
        buffer=Buffer::create_buffer(
            BufferUsage::StorageBuffer,
            // MemoryUsage::Download,
            MemoryUsage::GpuOnly,
            (1<<20)
            ).unwrap();
    }

    vector<vec3> instance_data;
    for(u32 i=0;i<15;i++)
    {
        for(u32 j=0;j<15;j++)
        {
            for(u32 k=0;k<4;k++)
                instance_data.push_back({i*5.0f,k*7.0f,j*5.0f});
        }
    }

    auto instance_buffer=Buffer::create_buffer_with_data(BufferUsage::StorageBuffer,MemoryUsage::Upload,instance_data).unwrap();

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
    vk::write_bindless_set(
        2+3*vk::num_swapchain_image(),
        instance_buffer
    );

    auto view2_img=vk::create_image2d(ImageDesc2D{
        .width=width,
        .height=height,
        .format=Format::B8G8R8A8_UNORM,
        .usage=ImageUsage::ColorAttachment|ImageUsage::TransferSrc,
        .mip_levels=1
    }).unwrap();
 
    u32 hzb_mip_levels=high_bit(1024)+1;
    auto hzb_img=vk::create_image2d(ImageDesc2D{
        .width=1024,
        .height=1024,
        .format=Format::D32_SFLOAT,
        .usage=ImageUsage::DepthStencilAttachment|ImageUsage::Sampled,
        .mip_levels=hzb_mip_levels
    }).unwrap();

    auto dep_img=vk::create_image2d(ImageDesc2D{
        .width=width,
        .height=height,
        .format=Format::D32_SFLOAT,
        .usage=ImageUsage::DepthStencilAttachment|ImageUsage::Sampled,
        .mip_levels=1,
    }).unwrap();

    vector<ImageView> dep_level_view(hzb_mip_levels+1);
    u32 i=0;
    for(auto& view:dep_level_view)
    {
        if(i==0)
        {
            view=dep_img.view(ImageViewDesc{
                .format=dep_img.format,
                .mip_level=0,
                .level_count=1
            }).unwrap();
        }
        else
        {
            view=hzb_img.view(ImageViewDesc{
                .format=hzb_img.format,
                .mip_level=i-1,
                .level_count=1
            }).unwrap();
        }
        i++;
    }

    Sampler unnorm_sampler=vk::create_sampler(SamplerDesc{.unnorm_coord=true}).unwrap();
    Sampler hzb_sampler=vk::create_sampler(SamplerDesc{.unnorm_coord=false}).unwrap();

    vector<CombinedImageSampler> img_sampler;
    for(auto& v:dep_level_view)
    {
        img_sampler.push_back({&unnorm_sampler,&v});
    }

    vk::write_bindless_set(0,img_sampler.data(),img_sampler.size());

    ImageView hzb_view=hzb_img.view(ImageViewDesc
    {
        .format=hzb_img.format,
        .mip_level=0,
        .level_count=hzb_img.mip_levels
    }).unwrap();

    vk::write_bindless_set(img_sampler.size(),CombinedImageSampler{&hzb_sampler,&hzb_view});

    auto g_ppl=GraphicsPipeline::new_()
        .render_pass(RenderingCreateInfo
        {
            .color_attachment_formats={vk::swapchain_image_format()},
            .depth_attachment_format=Format::D32_SFLOAT
        })
        .push_constant_size(8)
        .input_assembly_state(PrimitiveTopology::TriangleList)
        .vertex_shader(ShaderModule::from_file("shader/vert.spv").unwrap())
        .depth_stencil_state(DepthStencilState::reverse_z_test())
        .fragment_shader(ShaderModule::from_file("shader/frag.spv").unwrap())
        .dynamic_state({DynamicState::Viewport})
        .build().unwrap();

    auto c_ppl=ComputePipeline::new_()
        .push_constant_size(4)
        .compute_shader(ShaderModule::from_file("shader/culling.spv").unwrap())
        .build().unwrap();
    
    auto hzb_ppl=GraphicsPipeline::new_()
        .render_pass(RenderingCreateInfo{
            .depth_attachment_format=Format::D32_SFLOAT
        })
        .push_constant_size(4)
        .input_assembly_state(PrimitiveTopology::TriangleList)
        .vertex_shader(ShaderModule::from_file("shader/fullscreen_vert.spv").unwrap())
        .depth_stencil_state(DepthStencilState::always())
        .fragment_shader(ShaderModule::from_file("shader/hzb_frag.spv").unwrap())
        .dynamic_state({DynamicState::Viewport})
        .build().unwrap();

    auto cmd_allocator=CommandBufferAllocator::new_().unwrap();
    vector<CommandBuffer> cmds(vk::num_swapchain_image());
    u32 swapchain_idx=0;

    for(auto& cmd:cmds){
        u32 ctx[2]={swapchain_idx,0};
        cmd=CommandBuffer::new_(cmd_allocator).unwrap();
        cmd.begin(CommandBufferUsage::SimultaneousUse).unwrap()
            .bind_descriptor_sets(PipelineBindPoint::Compute,c_ppl.pipeline_layout,0,vk::bindless_buffer_set())
            .bind_descriptor_sets(PipelineBindPoint::Compute,c_ppl.pipeline_layout,1,vk::bindless_image_set())
            .push_constant(c_ppl.pipeline_layout,4,&swapchain_idx)
            .pipeline_barrier(Dependency{
                .buffer_barriers={BufferBarrier{
                    .buffer=indirect_buffer[swapchain_idx],
                    .dst_stage=PipelineStage::COMPUTE_SHADER,
                    .dst_access=AccessFlag::SHADER_WRITE
                }},
            })
            .bind_compute_pipeline(c_ppl.handle)
            .dispatch(num_groups*instance_data.size()/32+1,1,1)
            .pipeline_barrier(Dependency{
                .buffer_barriers={BufferBarrier{
                    .buffer=indirect_buffer[swapchain_idx],
                    .dst_stage=PipelineStage::DRAW_INDIRECT,
                    .dst_access=AccessFlag::INDIRECT_COMMAND_READ
                }},
                .image_barriers={
                    ImageBarrier{
                        .image=vk::swapchain_image(swapchain_idx),
                        .dst_stage=PipelineStage::COLOR_ATTACHMENT_OUTPUT,
                        .dst_access=AccessFlag::COLOR_ATTACHMENT_WRITE,
                        .new_layout=ImageLayout::AttachmentOptimal,
                    },
                    ImageBarrier{
                        .image=dep_img,
                        .dst_stage=PipelineStage::EARLY_FRAGMENT_TESTS,
                        .dst_access=AccessFlag::DEPTH_STENCIL_ATTACHMENT_WRITE,
                        .new_layout=ImageLayout::AttachmentOptimal,
                    }
                }
            })
            .begin_rendering(RenderingInfo{
                .render_area_extent={.x=window.width,.y=window.height},
                .color_attachments={RenderingAttachmentInfo{
                    .image_view=swapchain_image(swapchain_idx).image_view,
                    .load_op=LoadOp::Clear,
                }},
                .depth_attachment=Some(RenderingAttachmentInfo{
                    .image_view=dep_level_view[0].handle,
                    .load_op=LoadOp::Clear,
                })
            })
            .push_constant(g_ppl.pipeline_layout,8,ctx)
            .bind_descriptor_sets(PipelineBindPoint::Graphics,g_ppl.pipeline_layout,0,vk::bindless_buffer_set())
            .bind_graphics_pipeline(g_ppl.handle)
            .set_viewport({Viewport::dimension(width,height)})
            .draw_indirect(indirect_buffer[swapchain_idx])
            .end_rendering()
            .pipeline_barrier(Dependency{
                .image_barriers={
                    ImageBarrier{
                        .image=vk::swapchain_image(swapchain_idx),
                        .src_stage=PipelineStage::COLOR_ATTACHMENT_OUTPUT,
                        .src_access=AccessFlag::COLOR_ATTACHMENT_WRITE,
                        .dst_stage=PipelineStage::TRANSFER,
                        .dst_access=AccessFlag::TRANSFER_WRITE,
                        .old_layout=ImageLayout::AttachmentOptimal,
                        .new_layout=ImageLayout::TransferDstOptimal
                    },
                    ImageBarrier{
                        .image=dep_img,
                        .src_stage=PipelineStage::LATE_FRAGMENT_TESTS,
                        .src_access=AccessFlag::DEPTH_STENCIL_ATTACHMENT_WRITE,
                        .dst_stage=PipelineStage::FRAGMENT_SHADER,
                        .dst_access=AccessFlag::SHADER_READ,
                        .old_layout=ImageLayout::AttachmentOptimal,
                        .new_layout=ImageLayout::ShaderReadOnlyOptimal,
                    }
                }
            });

        cmd.bind_descriptor_sets(PipelineBindPoint::Graphics,hzb_ppl.pipeline_layout,1,vk::bindless_image_set());

        for(u32 i=0,cw=1024,ch=1024;i<hzb_mip_levels;i++,cw>>=1,ch>>=1)
        {
            cmd.pipeline_barrier(Dependency{
                    .image_barriers={ImageBarrier{
                        .image=hzb_img,
                        .dst_stage=PipelineStage::EARLY_FRAGMENT_TESTS,
                        .dst_access=AccessFlag::DEPTH_STENCIL_ATTACHMENT_WRITE,
                        .new_layout=ImageLayout::AttachmentOptimal,
                        .sub_range=Some(ImageSubRange{
                            .base_mip_level=i,
                            .level_count=1
                        })
                    }}
                })
                .begin_rendering(RenderingInfo{
                    .render_area_extent={.x=cw,.y=ch},
                    .depth_attachment=Some(RenderingAttachmentInfo{
                        .image_view=dep_level_view[i+1].handle,
                        .load_op=LoadOp::Clear,
                    })
                })
                .bind_graphics_pipeline(hzb_ppl.handle)
                .set_viewport({Viewport::dimension(cw,ch)})
                .push_constant(hzb_ppl.pipeline_layout,4,&i)
                .draw(6,1,0,0)
                .end_rendering()
                .pipeline_barrier(Dependency{
                    .image_barriers={ImageBarrier{
                        .image=hzb_img,
                        .src_stage=PipelineStage::LATE_FRAGMENT_TESTS,
                        .src_access=AccessFlag::DEPTH_STENCIL_ATTACHMENT_WRITE,
                        .dst_stage=PipelineStage::FRAGMENT_SHADER,
                        .dst_access=AccessFlag::SHADER_READ,
                        .old_layout=ImageLayout::AttachmentOptimal,
                        .new_layout=ImageLayout::ShaderReadOnlyOptimal,
                        .sub_range=Some(ImageSubRange{
                            .base_mip_level=i,
                            .level_count=1
                        })
                    }}
                });
            
        }
        cmd.pipeline_barrier(Dependency{
            .image_barriers={ImageBarrier{
                .image=hzb_img,
            }}
        });
        ctx[1]=1;
        cmd.pipeline_barrier(Dependency{
                .image_barriers={
                    ImageBarrier{
                        .image=view2_img,
                        .dst_stage=PipelineStage::COLOR_ATTACHMENT_OUTPUT,
                        .dst_access=AccessFlag::COLOR_ATTACHMENT_WRITE,
                        .new_layout=ImageLayout::AttachmentOptimal,
                    },
                    ImageBarrier{
                        .image=dep_img,
                        .src_stage=PipelineStage::FRAGMENT_SHADER,
                        .src_access=AccessFlag::SHADER_READ,
                        .dst_stage=PipelineStage::EARLY_FRAGMENT_TESTS,
                        .dst_access=AccessFlag::DEPTH_STENCIL_ATTACHMENT_WRITE,
                        .old_layout=ImageLayout::ShaderReadOnlyOptimal,
                        .new_layout=ImageLayout::AttachmentOptimal,
                    }
                }
            })
            .begin_rendering(RenderingInfo{
                .render_area_extent={.x=window.width,.y=window.height},
                .color_attachments={RenderingAttachmentInfo{
                    .image_view=view2_img.image_view,
                    .load_op=LoadOp::Clear,
                }},
                .depth_attachment=Some(RenderingAttachmentInfo{
                    .image_view=dep_level_view[0].handle,
                    .load_op=LoadOp::Clear,
                })
            })
            .push_constant(g_ppl.pipeline_layout,8,ctx)
            .bind_descriptor_sets(PipelineBindPoint::Graphics,g_ppl.pipeline_layout,0,vk::bindless_buffer_set())
            .bind_graphics_pipeline(g_ppl.handle)
            .set_viewport({Viewport::dimension(width,height)})
            .draw_indirect(indirect_buffer[swapchain_idx])
            .end_rendering()
            .pipeline_barrier(Dependency{
                .image_barriers={
                    ImageBarrier{
                        .image=view2_img,
                        .src_stage=PipelineStage::COLOR_ATTACHMENT_OUTPUT,
                        .src_access=AccessFlag::COLOR_ATTACHMENT_WRITE,
                        .dst_stage=PipelineStage::TRANSFER,
                        .dst_access=AccessFlag::TRANSFER_READ,
                        .old_layout=ImageLayout::AttachmentOptimal,
                        .new_layout=ImageLayout::TransferSrcOptimal
                    },
                }
            })
            .blit_image(
                view2_img,
                vk::swapchain_image(swapchain_idx),
                ivec4{0,0,width,height},
                ivec4{0,height*3/4,width/4,height}
            )
            .pipeline_barrier(Dependency{
                .image_barriers={
                    ImageBarrier{
                        .image=vk::swapchain_image(swapchain_idx),
                        .src_stage=PipelineStage::TRANSFER,
                        .src_access=AccessFlag::TRANSFER_WRITE,
                        .old_layout=ImageLayout::TransferDstOptimal,
                        .new_layout=ImageLayout::PresentSrc
                    },
                }
            });

        cmd.build().unwrap();
        
        swapchain_idx++;
    }

    struct FrameSync
    {
        Semaphore acq_img=vk::create_semaphore().unwrap();
        Semaphore cmd_exec=vk::create_semaphore().unwrap();
        Fence cpu_wait_cmd=vk::create_fence(true).unwrap();
    }sync[3];
    
    u32 frame_idx=0,frame_cnt=0;

    Camera camera{
        .position=vec3{0,0,-10},
        .pitch=0,
        .yaw=90,
    };

    Camera camera2{
        .position=vec3{50,100,50},
        .pitch=-89,
        .yaw=0,
    };

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
        timer.reset();

        // if(window.is_key_down('W')) camera.move_front(tick_time);
        // if(window.is_key_down('S')) camera.move_front(-tick_time);
        // if(window.is_key_down('A')) camera.move_right(-tick_time);
        // if(window.is_key_down('D')) camera.move_right(tick_time);

        if(window.is_key_down('W')) camera.move_front(-tick_time);
        if(window.is_key_down('S')) camera.move_front(tick_time);
        if(window.is_key_down('A')) camera.move_right(tick_time);
        if(window.is_key_down('D')) camera.move_right(-tick_time);

        if(window.is_key_begin_press('J')) view_mode=(view_mode+4-1)%4;
        if(window.is_key_begin_press('K')) view_mode=(view_mode+1)%4;

        if(window.is_key_begin_press('B'))
        {
            window.set_cursor_normal();
            lst_cursor_pos=window.get_cursor_pos();
            is_cursor_disabled=false;
        }
        if(window.is_key_begin_release('B'))
        {
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

        mat4 v_mat2=camera2.view_mat();
        mat4 p_mat2=camera2.projection_mat(Camera::radians(40),(f32)window.width/window.height);
        mat4 vp_mat2=mul(p_mat2,v_mat2);

        vk::acquire_next_image(Some(sync[frame_idx].acq_img),None(),swapchain_idx);
        vk::wait_for_fence(sync[frame_idx].cpu_wait_cmd);
        vk::reset_fence(sync[frame_idx].cpu_wait_cmd);

        FrameContext frame_context{vp_mat,v_mat,p_mat,vp_mat2,view_mode};
        frame_context_buffers[swapchain_idx].update(&frame_context,sizeof(FrameContext));

        u32 c[4]={128*3,0,0,0};
        indirect_buffer[swapchain_idx].update(c,sizeof(u32)*4);

        vk::queue_submit(
            SubmitInfo{
                .waiting={sync[frame_idx].acq_img},
                .command_buffers={cmds[swapchain_idx]},
                .signal={sync[frame_idx].cmd_exec}
            },
            sync[frame_idx].cpu_wait_cmd
        );
        vk::queue_present(
            PresentInfo{
            .waiting={sync[frame_idx].cmd_exec},
            .swapchain_image_idx=swapchain_idx
        });

        frame_idx=(frame_idx+1)%3;
        frame_cnt++;
    }

#ifdef debug_fps
    f32 all_time=timer_fps.us();
    printf("avg_fps:%f",1000.f/(all_time/1000.f/frame_cnt));
#endif
    vk::cleanup();
    return 0;
}   