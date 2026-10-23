#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float progress;
    vec2 resolution;
};
void main(){
    vec2 uv=qt_TexCoord0;
    float r=resolution.y*.5;
    vec2 p=uv*resolution;
    vec2 q=abs(p-resolution*.5)-(resolution*.5-vec2(r));
    float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-r;
    float mask=(1.0-smoothstep(-.8,.8,d))*(1.0-smoothstep(progress-.002,progress+.002,uv.x));
    vec3 color=mix(vec3(0.0,.95,1.0),vec3(.39,.40,.95),uv.x);
    float wave=.8+.2*sin(uv.x*19.0-time*2.2+sin(uv.y*3.14));
    color=color*wave+vec3(.24)*pow(1.0-uv.y,5.0);
    fragColor=vec4(color*mask,mask)*qt_Opacity;
}
