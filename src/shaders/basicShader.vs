#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 vnormal;
layout(location = 2) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform mat3 normal;
uniform vec3 modelColor;

out vec3 fragPosition;
out vec3 fragColor;
out vec3 fragNormal;
out vec2 fragTexCoord;
void main() {
    fragPosition = vec3(model * vec4(position, 1.0));
    gl_Position = proj * view * vec4(fragPosition, 1.0);
    
    fragColor = modelColor;
    fragTexCoord = texCoord;
    fragNormal = normal * vnormal;
}