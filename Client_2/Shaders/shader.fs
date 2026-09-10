#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec4 to_light;

uniform sampler2D texture1;

void main() {
   vec4 textureColor = texture(texture1, TexCoord);
    vec4 lightColor = to_light;

    float maxIntensity = 1.0;
    lightColor.rgb = min(lightColor.rgb, vec3(maxIntensity));

    FragColor = textureColor * lightColor;

}
