#version 330 core
in vec3 fragPosition;
in vec3 fragColor;
in vec3 fragNormal;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform bool hasNormals;

out vec4 color;
void main() {
    // ambient lighting
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;
    vec3 diffuse;
    vec3 specular;
    if (hasNormals) {
        // diffuse lighting
        vec3 norm = normalize(fragNormal);
        vec3 lightDirection = normalize(lightPos - fragPosition);
        float diff = max(dot(lightDirection, norm), 0.0f);
        diffuse = diff * lightColor;

        // specular lighting
        float specularStrength = 0.5;
        vec3 viewDirection = normalize(viewPos - fragPosition);
        vec3 reflectDirection = reflect(-lightDirection, norm);
        float spec = pow(max(dot(viewDirection, reflectDirection), 0.0), 32);
        specular = specularStrength * spec * lightColor;
    } else {
        diffuse = vec3(1.0);
        specular = vec3(1.0);
    }
    // set the final color
    color = vec4((ambient + diffuse + specular) * fragColor, 1.0);
}