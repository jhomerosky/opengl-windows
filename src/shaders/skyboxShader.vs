#version 330 core
layout(location = 0) in vec3 position;

uniform mat4 projview;

out vec3 fragTexCoord;
void main() {
    fragTexCoord = position;
    vec4 pos = projview * vec4(position, 1.0);
    gl_Position = pos.xyww;
}