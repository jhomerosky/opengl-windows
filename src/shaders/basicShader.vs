#version 330 core
layout(location = 0) in vec3 position;
//layout(location = 1) in vec3 color;
layout(location = 1) in vec3 vnormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform mat3 normal;
uniform vec3 modelColor;

out vec3 fragPosition;
out vec3 fragColor;
out vec3 fragNormal;
void main() {
    fragPosition = vec3(model * vec4(position, 1.0));
    gl_Position = proj * view * vec4(fragPosition, 1.0);
    
    fragColor = modelColor;
    fragNormal = normal * vnormal;
}