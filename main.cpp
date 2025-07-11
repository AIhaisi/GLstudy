#include <iostream>
#include <vector>
#include <direct.h>
#include <algorithm>
#include <windows.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// 顶点着色器
const char* vertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)glsl";

// 片段着色器
const char* fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec3 color;
void main()
{
    FragColor = vec4(color, 1.0);
}
)glsl";

// 用于屏幕渲染的着色器
const char* screenVertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);  // 修复了这里的中文逗号
    TexCoords = aTexCoords;
}
)glsl";

const char* screenFragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform bool bloom;
uniform float exposure;

void main()
{
    const float gamma = 2.2;
    vec3 hdrColor = texture(scene, TexCoords).rgb;      
    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
    
    // 如果启用了泛光，添加泛光效果
    if(bloom)
        hdrColor += bloomColor; // 加法混合
    
    // 色调映射
    vec3 result = vec3(1.0) - exp(-hdrColor * exposure);
    // 伽马校正
    result = pow(result, vec3(1.0 / gamma));
    FragColor = vec4(result, 1.0);
}
)glsl";

// 用于提取亮度和模糊的着色器
const char* blurFragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D image;
uniform bool horizontal;
uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    vec2 tex_offset = 1.0 / textureSize(image, 0); // 获取纹理大小
    vec3 result = texture(image, TexCoords).rgb * weight[0];
    
    if(horizontal)
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(image, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            result += texture(image, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }
    }
    else
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(image, TexCoords + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            result += texture(image, TexCoords - vec2(极.0, tex_offset.y * i)).rgb * weight[i];
        }
    }
    FragColor = vec4(result, 1.0);
}
)glsl";

// 用于提取高光部分的着色器
const char* brightFragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;

void main()
{
    const vec3 luminanceWeight = vec3(0.2126, 0.7152, 0.0722);
    vec3 color = texture(scene, TexCoords).rgb;
    float brightness = dot(color, luminanceWeight);
    
    if(brightness > 1.0)
        FragColor = vec4(color, 1.0);
    else
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
)glsl";

// PBR顶点着色器
const char* pbrVertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    TexCoords = aTexCoords;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)glsl";

// PBR片段着色器
const char* pbrFragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

// 材质参数
uniform vec3 albedo;
uniform float metallic;
uniform float roughness;
uniform float ao;

// 光照参数
uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];
uniform vec3 viewPos;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);

    // 计算F0
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // 反射方程
    vec3 Lo = vec3(0.0);
    for(int i = 0; i < 4; ++i) 
    {
        // 计算每个光源
        vec3 L = normalize(lightPositions[i] - FragPos);
        vec3 H = normalize(V + L);
        float distance = length(lightPositions[i] - FragPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = lightColors[i] * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);    
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  
        
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;
            
        // 计算每个光源的贡献
        float NdotL = max(dot(N, L), 0.0);        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    // 环境光
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;

    // HDR色调映射
    color = color / (color + vec3(1.0));
    // Gamma校正
    color = pow(color, vec3(1.0/2.2)); 

    FragColor = vec4(color, 1.0);
}
)glsl";



// 立方体顶点数据
float cubeVertices[] = {
    // 后面
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,

    // 前面
    -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,

    // 左面
    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,

    // 右面
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,

     // 底面
     -0.5f, -0.5f, -0.5f,
      0.5f, -0.5f, -0.5f,
      0.5f, -0.5f,  0.5f,
      0.5f, -0.5f,  0.5f,
     -0.5f, -0.5f,  0.5f,
     -0.5f, -0.5f, -0.5f,

     // 顶面
     -0.5f,  0.5f, -0.5f,
      0.5f,  0.5f, -0.5f,
      0.5f,  0.5f,  0.5f,
      0.5f,  0.5f,  0.5f,
     -0.5f,  0.5f,  0.5f,
     -0.5f, 0.5f, -0.5f
};

float vertices[] = {
	// 位置             // 法线           // 纹理坐标
	-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  // 左下
	 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // 右下
	 0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f   // 顶部
};

// 用于全屏渲染的四边形
float quadVertices[] = {
    // 位置     // 纹理坐标
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};

// 摄像机参数
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 15.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

// 鼠标控制变量
float yaw = -90.0f; // 偏航角初始化为-90度，使初始方向指向正前方
float pitch = 0.0f;
float lastX = 400.0f;
float lastY = 300.0f;
bool firstMouse = true;
float fov = 45.0f;

// 帧时间控制
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct Mesh {
    std::vector<float> vertices;
    unsigned int VAO, VBO;
};

bool loadModel(const std::string& path, Mesh& mesh) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Assimp 加载模型失败: " << importer.GetErrorString() << std::endl;
        return false;
    }

    // 遍历所有网格
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* aiMesh = scene->mMeshes[i];
        for (unsigned int j = 0; j < aiMesh->mNumFaces; j++) {
            aiFace face = aiMesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++) {
                unsigned int index = face.mIndices[k];
                mesh.vertices.push_back(aiMesh->mVertices[index].x);
                mesh.vertices.push_back(aiMesh->mVertices[index].y);
                mesh.vertices.push_back(aiMesh->mVertices[index].z);
            }
        }
    }

    // 创建 VAO/VBO
    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);

    glBindVertexArray(mesh.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(float), mesh.vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    return true;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // 反转y坐标
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // 限制俯仰角
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    // 计算新的前向量
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    fov -= (float)yoffset;
    if (fov < 1.0f)
        fov = 1.0f;
    if (fov > 45.0f)
        fov = 45.0f;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    float cameraSpeed = 2.5f * deltaTime;

    // WASD移动控制
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

    // 空格键上升，左Shift键下降
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;

    // 按B键切换泛光效果
    static bool bloomKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !bloomKeyPressed)
    {
        bloomKeyPressed = true;
        // 这里我们将在渲染循环中处理
    }
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE)
    {
        bloomKeyPressed = false;
    }
}

// 创建着色器程序
unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource)
{
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

using namespace std;

std::string getExecutablePath() {
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, MAX_PATH);  // 获取完整路径
	return std::string(path);
}

std::string getProjectRoot(const std::string& exePath) {
	std::string path = exePath;

	// 替换所有反斜杠为斜杠（统一格式）
	std::replace(path.begin(), path.end(), '\\', '/');

	// 从路径末尾逐级向上查找 "GLstudy"
	size_t pos = path.rfind("GLstudy/");
	if (pos != std::string::npos) {
		return path.substr(0, pos + 8);  // 包含 "GLstudy/"
	}

	// 如果未找到，尝试手动回溯（适用于非标准构建目录）
	while (true) {
		size_t lastSlash = path.find_last_of('/');
		if (lastSlash == std::string::npos) break;
		path = path.substr(0, lastSlash);
		if (path.size() < 3) break;  // 避免无限循环

		// 检查当前目录名是否为 "GLstudy"
		size_t parentSlash = path.find_last_of('/');
		std::string dirName = path.substr(parentSlash + 1);
		if (dirName == "GLstudy") {
			return path + "/";
		}
	}

	throw std::runtime_error("无法定位项目根目录（GLstudy）");
}

std::string getResourcePath(const std::string& relativePath) {
	std::string exePath = getExecutablePath();
	std::string projectRoot = getProjectRoot(exePath);

	// 统一使用正斜杠（兼容Windows/Linux）
	std::string fullPath = projectRoot + "src/" + relativePath;
	std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
	return fullPath;
}


int main(){

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "6x6 Cubes with Bloom Effect", NULL, NULL);
    if (!window) {
        cout << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 设置鼠标回调
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // 隐藏光标并捕获它
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }


	std::string objPath = getResourcePath("source/objects/teapot/teapot.obj");
	std::cout << "资源路径: " << objPath << std::endl;
	Mesh teapot;
	if (!loadModel(objPath, teapot)) {
		return -1;
	}

    // 创建着色器程序
    unsigned int cubeShader = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    unsigned int screenShader = createShaderProgram(screenVertexShaderSource, screenFragmentShaderSource);
    unsigned int blurShader = createShaderProgram(screenVertexShaderSource, blurFragmentShaderSource);
    unsigned int brightShader = createShaderProgram(screenVertexShaderSource, brightFragmentShaderSource);
	// 创建PBR着色器程序
	unsigned int pbrShader = createShaderProgram(pbrVertexShaderSource, pbrFragmentShaderSource);

    // 设置立方体VAO/VBO
    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 设置屏幕四边形VAO/VBO
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

	// 设置三角形VAO/VBO
	unsigned int verticeVAO, verticeVBO;
	glGenVertexArrays(1, &verticeVAO);
	glGenBuffers(1, &verticeVBO);

	glBindVertexArray(verticeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, verticeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// 位置属性
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// 法线属性
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// 纹理坐标属性
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
    // 配置帧缓冲
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    // 创建两个颜色缓冲 (一个用于正常渲染，一个用于高光提取)
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 800, 600, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // 将纹理附加到帧缓冲
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }

    // 创建渲染缓冲对象用于深度和模板测试
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 800, 600);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    // 告诉OpenGL我们要使用哪个颜色附件进行渲染
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    // 检查帧缓冲是否完整
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        cout << "Framebuffer not complete!" << endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 用于模糊的ping-pong帧缓冲
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 800, 600, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            cout << "Framebuffer not complete!" << endl;
    }

    // 启用深度测试
    glEnable(GL_DEPTH_TEST);

    // 泛光效果开关
    bool bloom = true;
    float exposure = 1.0f;

    // 主渲染循环
    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        // 1. 渲染场景到浮点帧缓冲
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glViewport(0, 0, 800, 600);  // 确保视口大小正确
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);  // 使用深色背景

        glUseProgram(cubeShader);

        // 设置视图和投影矩阵
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(fov), 800.0f / 600.0f, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(cubeShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(cubeShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // 渲染
        glUseProgram(cubeShader);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -3.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.5f));
     /*   model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));*/

        glUniformMatrix4fv(glGetUniformLocation(cubeShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(cubeShader, "color"), 1, glm::value_ptr(glm::vec3(0.8f, 0.3f, 0.2f)));

		glBindVertexArray(teapot.VAO);
		glDrawArrays(GL_TRIANGLES, 0, teapot.vertices.size() / 3);

        // 绘制6x6共36个立方体
        glBindVertexArray(cubeVAO);
        for (int i = 0; i < 1; ++i)
        {
            for (int j = 0; j < 6; ++j)
            {
                // 修改立方体位置，使它们在3D空间中分布
                glm::vec3 cubePos = glm::vec3(
                    i * 2.0f - 5.0f,  // x: -5到5
                    j * 1.5f - 4.0f,  // y: -4到4
                    (i + j) * -1.5f   // z: 深度变化
                );

                // 创建模型矩阵
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, cubePos);
                // 添加一些旋转使立方体看起来更有立体感
                model = glm::rotate(model, (float)glfwGetTime() * glm::radians(20.0f * (i + j)),
                    glm::vec3(0.5f, 1.0f, 0.0f));

                // 设置颜色（使一些立方体更亮以产生泛光效果）
                glm::vec3 cubeColor = glm::vec3(
                    i * 0.15f + 0.2f,
                    j * 0.15f + 0.2f,
                    0.5f + i * 0.1f
                );
                if ((i + j) % 3 == 0) // 使部分立方体更亮
                    cubeColor *= 3.0f;

                // 传递模型矩阵和颜色到着色器
                glUniformMatrix4fv(glGetUniformLocation(cubeShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
                glUniform3fv(glGetUniformLocation(cubeShader, "color"), 1, glm::value_ptr(cubeColor));

                // 绘制立方体
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }

		// 设置光照
		glm::vec3 lightPositions[] = {
			glm::vec3(-10.0f,  10.0f, 10.0f),
			glm::vec3(10.0f,  10.0f, 10.0f),
			glm::vec3(-10.0f, -10.0f, 10.0f),
			glm::vec3(10.0f, -10.0f, 10.0f)
		};
		glm::vec3 lightColors[] = {
			glm::vec3(300.0f, 300.0f, 300.0f),
			glm::vec3(300.0f, 300.0f, 300.0f),
			glm::vec3(300.0f, 300.0f, 300.0f),
			glm::vec3(300.0f, 300.0f, 300.0f)
		};
		glUniform3fv(glGetUniformLocation(pbrShader, "lightPositions"), 4, glm::value_ptr(lightPositions[0]));
		glUniform3fv(glGetUniformLocation(pbrShader, "lightColors"), 4, glm::value_ptr(lightColors[0]));
		glUniform3fv(glGetUniformLocation(pbrShader, "viewPos"), 1, glm::value_ptr(cameraPos));

		// 设置视图和投影矩阵
		glUniformMatrix4fv(glGetUniformLocation(pbrShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(pbrShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));


        // 2. 提取高光部分到第二个颜色缓冲
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[0]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(brightShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[1]); // 使用第二个颜色缓冲
        glUniform1i(glGetUniformLocation(brightShader, "scene"), 0);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 3. 对高光部分应用高斯模糊
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10; // 模糊迭代次数
        glUseProgram(blurShader);
        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            glUniform1i(glGetUniformLocation(blurShader, "horizontal"), horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? pingpongColorbuffers[0] : pingpongColorbuffers[!horizontal]);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 4. 现在渲染带有泛光效果的HDR颜色缓冲到默认帧缓冲
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(screenShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]); // 正常渲染的颜色缓冲
        glUniform1i(glGetUniformLocation(screenShader, "scene"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]); // 模糊后的高光颜色缓冲
        glUniform1i(glGetUniformLocation(screenShader, "bloomBlur"), 1);
        glUniform1f(glGetUniformLocation(screenShader, "exposure"), exposure);
        glUniform1i(glGetUniformLocation(screenShader, "bloom"), bloom);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteProgram(cubeShader);
    glDeleteProgram(screenShader);
    glDeleteProgram(blurShader);
    glDeleteProgram(brightShader);
    glfwTerminate();
    return 0;
}