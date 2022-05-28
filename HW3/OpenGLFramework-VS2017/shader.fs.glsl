#version 330

in vec2 texCoord;
in vec3 vertex_color;
in vec3 vertex_normal;
in vec3 FragPos;

out vec4 fragColor;

uniform mat4 mv;
uniform mat4 um4v;
uniform int lightIdx;
uniform vec3 Ka;
uniform vec3 Kd;
uniform vec3 Ks;
uniform float shininess;
uniform vec3 La = vec3(0.15f, 0.15f, 0.15f);
uniform vec3 Ld1;
uniform vec3 Ld2;
uniform vec3 Ld3;
uniform vec3 lightPos1;
uniform vec3 lightPos2;
uniform vec3 lightPos3;
uniform float spotCutoff;
uniform int perpix;

// [TODO] passing texture from main.cpp
// Hint: sampler2D
uniform sampler2D diffuseTexture;

//three lights are different in Attenuation
vec3 directlight(){
	//normalize vector can calculate cosine(theta)
	vec3 N = normalize(vertex_normal);
	vec4 lightInView =  um4v * vec4(lightPos1,1.0);
	vec3 L = normalize(lightInView.xyz - FragPos);
	vec3 V = normalize(-FragPos);	//camera vector
	vec3 H = normalize(L+V);
	vec3 ambient = La*Ka;
	vec3 diffuse = Kd * max(dot(N, L), 0);
	vec3 specular = Ks * pow(max(dot(N, H), 0), shininess);
	vec3 color = ambient +Ld1*( diffuse + specular);
	return color;
}
vec3 pointlight(){
	vec3 N = normalize(vertex_normal);
	vec4 lightInView =  um4v * vec4(lightPos2,1.0);
	vec3 L = normalize(lightInView.xyz - FragPos);
	vec3 V = normalize(-FragPos);	//camera vector
	vec3 H = normalize(L+V);
	float dist = length(lightPos2 - FragPos);
	float cal = 0.01 + 0.8 * dist + 0.1 * dist * dist;
	float factor = 1/(cal);
	vec3 ambient = La * Ka;
	vec3 diffuse =  Kd * max(dot(N, L), 0);
	vec3 specular = Ks * pow(max(dot(N, H), 0), shininess);
	vec3 color = ambient + factor*Ld2*(diffuse + specular);
	return color;
}
vec3 spotlight(){
	vec3 N = normalize(vertex_normal);
	vec4 lightInView =  um4v * vec4(lightPos3,1.0);
	vec3 L = normalize(lightInView.xyz - FragPos);
	vec3 V = normalize(-FragPos);	//camera vector
	vec3 H = normalize(L+V);
	vec4 spotdirection = vec4(0.0f, 0.0f,-1.0f,1.0f);
	vec3 direction = normalize((transpose(inverse(um4v))*spotdirection).xyz);
	float spot = dot(-L, direction);

	float dist = length(lightInView.xyz - FragPos);
	float cal = 0.05 + 0.3 * dist + 0.6 * dist * dist;
	float factor = 1/cal;

	vec3 ambient = La * Ka;
	vec3 diffuse =  Kd * max(dot(N, L), 0);
	vec3 specular = Ks * pow(max(dot(N, H), 0), shininess);
	//theta in the cutoff should be lighted
	if(spotCutoff > degrees(acos(spot))){
		vec3 color=ambient+factor* pow(max(spot, 0), 50) * Ld3*(diffuse + specular);
		return color;
	}
	else{
		vec3 color=ambient+factor*0*(diffuse + specular);
		return color;
	}
}


void main() {
	//fragColor = vec4(texCoord.xy, 0, 1);

	// [TODO] sampleing from texture
	// Hint: texture

	//Assignment2
	vec3 frag_color;
	if (perpix==0){
		fragColor = vec4(vertex_color, 1.0f);
	}
	else{
		if (lightIdx==0)
			frag_color+=directlight();
		else if (lightIdx==1)
			frag_color+=pointlight();
		else
			frag_color+=spotlight();
		fragColor = vec4(frag_color, 1.0f);
	}
	//light*texture_color
	fragColor=fragColor*vec4(texture(diffuseTexture, texCoord.xy).rgb, 1);
}
