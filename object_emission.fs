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
        specular = light.specular * spec * texture(material.specular, TexCoords).rgb;
    }
    
    // emission
    vec3 emission = vec3(0.0);
    vec3 emissionIntensity = texture(material.emission, TexCoords).rgb;

    // Cek apakah diff kurang dari 0.1 dan semua komponen dari emissionIntensity lebih besar dari 0.1
    if (diff < 0.1 && all(greaterThan(emissionIntensity, vec3(0.1)))) {
        emission = emissionIntensity;
    }
    
    vec3 result = ambient + diffuse + specular + emission;
    FragColor = vec4(result, 1.0);
} 