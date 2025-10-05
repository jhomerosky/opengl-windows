#version 330 core
layout(location = 0) in vec3 position;

uniform mat4 proj;
uniform mat4 view;

out vec3 fragTexCoord;
void main() {
    fragTexCoord = position;
    vec4 pos = proj * view * vec4(position, 1.0);
    gl_Position = pos.xyww;
}