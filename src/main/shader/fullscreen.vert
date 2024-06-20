#version 450
#extension GL_ARB_separate_shader_objects:enable

vec3 p[]={
    {-1,-1,0},
    {-1,1,0},
    {1,-1,0},
    {1,1,0}
};

void main()
{
    int id=gl_VertexIndex;
    if(id>=3) 
        id-=2;
    gl_Position=vec4(p[id],1);
}