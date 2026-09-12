#version 330 core
out vec4 FragColor;

struct Material {
    sampler2D diffuse;
    sampler2D specular;    
    sampler2D emission;
    float shininess;
}; 

struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

in vec3 FragPos;  
in vec2 TexCoords;
in vec3 Normal;  
  
uniform vec3 viewPos;
uniform Material material;
uniform Light light;
uniform int debugMode; // 0=penuh, 1=diffuse saja, 2=emission saja, 3=peta diff

void main()
{
    // ambient
    vec3 ambient = light.ambient * texture(material.diffuse, TexCoords).rgb;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * texture(material.diffuse, TexCoords).rgb;  
    
    // specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = vec3(0.0);
    
    if (diff > 0.0) {
        // Masker skalar dari channel merah: pantulan selalu putih netral,
        // apapun tint yang ada di file specular map
        float specMask = texture(material.specular, TexCoords).r;
        specular = light.specular * spec * specMask;
    }
    
    // emission (lampu kota): hanya di sisi malam, fade halus saat senja
    vec3 emissionIntensity = texture(material.emission, TexCoords).rgb;
    float nightFactor = 1.0 - smoothstep(0.0, 0.25, diff);
    vec3 emission = emissionIntensity * nightFactor;
    
    vec3 result = ambient + diffuse + specular + emission;
    if (debugMode == 1) result = diffuse;
    else if (debugMode == 2) result = emission;
    else if (debugMode == 3) result = vec3(diff);
    FragColor = vec4(result, 1.0);
} 