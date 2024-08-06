#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 frag_color;
layout(location = 0) out vec4 out_color;

void main() {
	out_color = vec4(frag_color, 1.0);
}