#version 450
#extension GL_ARB_separate_shader_objects:enable
#extension GL_EXT_nonuniform_qualifier:enable

layout(set=1,binding=0) uniform sampler2D level_deps[];

layout(push_constant) uniform constant
{
    uint lst_level;
}pc;

ivec2 mip1_to_mip0(ivec2 p)
{
    // 映射到[1920,1080]
    return p*ivec2(1920,1080)/ivec2(1024,1024);
}

void main()
{
    ivec2 p=ivec2(gl_FragCoord);
    // 将p转为上一层次
    if(pc.lst_level==0)
        p=mip1_to_mip0(p);
    else
        p=2*p;
    // 从上一层次的深度纹理中读取四个相邻像素的深度值
    float x=texture(level_deps[pc.lst_level],p+0.5).x;
    float y=texture(level_deps[pc.lst_level],p+0.5+ivec2(1,0)).x;
    float z=texture(level_deps[pc.lst_level],p+0.5+ivec2(1,1)).x;
    float w=texture(level_deps[pc.lst_level],p+0.5+ivec2(0,1)).x;

    float min_z=min(min(x,y),min(z,w));
    gl_FragDepth=min_z;
}
