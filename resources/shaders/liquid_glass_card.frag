#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 resolution;
    float radius;
};
void main(){
    vec2 p=qt_TexCoord0*resolution;
    vec2 q=abs(p-resolution*.5)-(resolution*.5-vec2(radius));
    float d=length(max(q,0.0))+min(max(q.x,q.y),0.0)-radius;
    float mask=1.0-smoothstep(-.8,.8,d);
    float rim=(1.0-smoothstep(.2,1.6,abs(d+1.0)))*mask;
    float diagonal=clamp((qt_TexCoord0.x+qt_TexCoord0.y)*.5,0.0,1.0);
    vec3 edge=mix(vec3(.75,.85,1.0),vec3(.1,.65,.85),diagonal);
    float lighting=mix(.38,.10,smoothstep(0.0,.7,diagonal));
    vec3 tint=mix(vec3(.075,.095,.14),vec3(.035,.047,.075),qt_TexCoord0.y);
    float alpha=.83*mask;
    fragColor=vec4(tint*alpha+edge*rim*lighting,alpha)*qt_Opacity;
}
