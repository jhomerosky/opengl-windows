#version 330 core
in vec3 fragTexCoord;

uniform samplerCube skybox;

out vec4 color;
void main() {
    color = texture(skybox, fragTexCoord);
}