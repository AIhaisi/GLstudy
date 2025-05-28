#include <iostream>
#include <vector>
#include <filesystem>
#include <direct.h>
#include <windows.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace std;


// ������ɫ��
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

// Ƭ����ɫ��
const char* fragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec3 color;
void main()
{
    FragColor = vec4(color, 1.0);
}
)glsl";

// ������Ļ��Ⱦ����ɫ��
const char* screenVertexShaderSource = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);  // �޸�����������Ķ���
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
    
    // ��������˷��⣬��ӷ���Ч��
    if(bloom)
        hdrColor += bloomColor; // �ӷ����
    
    // ɫ��ӳ��
    vec3 result = vec3(1.0) - exp(-hdrColor * exposure);
    // ٤��У��
    result = pow(result, vec3(1.0 / gamma));
    FragColor = vec4(result, 1.0);
}
)glsl";

// ������ȡ���Ⱥ�ģ������ɫ��
const char* blurFragmentShaderSource = R"glsl(
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D image;
uniform bool horizontal;
uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    vec2 tex_offset = 1.0 / textureSize(image, 0); // ��ȡ�����С
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
            result += texture(image, TexCoords - vec2(��.0, tex_offset.y * i)).rgb * weight[i];
        }
    }
    FragColor = vec4(result, 1.0);
}
)glsl";

// ������ȡ�߹ⲿ�ֵ���ɫ��
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

// �����嶥������
float cubeVertices[] = {
    // ����
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,

    // ǰ��
    -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,

    // ����
    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,

    // ����
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,

     // ����
     -0.5f, -0.5f, -0.5f,
      0.5f, -0.5f, -0.5f,
      0.5f, -0.5f,  0.5f,
      0.5f, -0.5f,  0.5f,
     -0.5f, -0.5f,  0.5f,
     -0.5f, -0.5f, -0.5f,

     // ����
     -0.5f,  0.5f, -0.5f,
      0.5f,  0.5f, -0.5f,
      0.5f,  0.5f,  0.5f,
      0.5f,  0.5f,  0.5f,
     -0.5f,  0.5f,  0.5f,
     -0.5f, 0.5f, -0.5f
};

// ����ȫ����Ⱦ���ı���
float quadVertices[] = {
    // λ��     // ��������
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};

// ���������
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 15.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

// �����Ʊ���
float yaw = -90.0f; // ƫ���ǳ�ʼ��Ϊ-90�ȣ�ʹ��ʼ����ָ����ǰ��
float pitch = 0.0f;
float lastX = 400.0f;
float lastY = 300.0f;
bool firstMouse = true;
float fov = 45.0f;

// ֡ʱ�����
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
        std::cerr << "Assimp ����ģ��ʧ��: " << importer.GetErrorString() << std::endl;
        return false;
    }

    // ������������
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

    // ���� VAO/VBO
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
    float yoffset = lastY - ypos; // ��תy����
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // ���Ƹ�����
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    // �����µ�ǰ����
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

    // WASD�ƶ�����
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

    // �ո����������Shift���½�
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraUp;

    // ��B���л�����Ч��
    static bool bloomKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !bloomKeyPressed)
    {
        bloomKeyPressed = true;
        // �������ǽ�����Ⱦѭ���д���
    }
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE)
    {
        bloomKeyPressed = false;
    }
}

// ������ɫ������
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

int main(){
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, MAX_PATH);  // ��ȡ����·��
	std::cout << "��ִ���ļ�·��: " << path << std::endl;

	// ��ȡĿ¼����
	std::string dir(path);
	size_t last_slash = dir.find_last_of("\\/");
	if (last_slash != std::string::npos) {
		dir = dir.substr(0, last_slash);
	}
	std::cout << "����Ŀ¼: " << dir << std::endl;

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

    // �������ص�
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // ���ع�겢������
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }
    Mesh teapot;
    if (!loadModel("D:/Visual Studio/Project/GLstudy/src/source/objects/teapot/teapot.obj", teapot)) {
        return -1;
    }


    // ������ɫ������
    unsigned int cubeShader = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    unsigned int screenShader = createShaderProgram(screenVertexShaderSource, screenFragmentShaderSource);
    unsigned int blurShader = createShaderProgram(screenVertexShaderSource, blurFragmentShaderSource);
    unsigned int brightShader = createShaderProgram(screenVertexShaderSource, brightFragmentShaderSource);

    // ����������VAO/VBO
    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // ������Ļ�ı���VAO/VBO
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
    // ����֡����
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    // ����������ɫ���� (һ������������Ⱦ��һ�����ڸ߹���ȡ)
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
        // �������ӵ�֡����
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }

    // ������Ⱦ�������������Ⱥ�ģ�����
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 800, 600);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    // ����OpenGL����Ҫʹ���ĸ���ɫ����������Ⱦ
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    // ���֡�����Ƿ�����
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        cout << "Framebuffer not complete!" << endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ����ģ����ping-pong֡����
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

    // ������Ȳ���
    glEnable(GL_DEPTH_TEST);

    // ����Ч������
    bool bloom = true;
    float exposure = 1.0f;

    // ����Ⱦѭ��
    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        // 1. ��Ⱦ����������֡����
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glViewport(0, 0, 800, 600);  // ȷ���ӿڴ�С��ȷ
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);  // ʹ����ɫ����

        glUseProgram(cubeShader);

        // ������ͼ��ͶӰ����
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(fov), 800.0f / 600.0f, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(cubeShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(cubeShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // ��Ⱦ
        glUseProgram(cubeShader);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, -3.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.5f));
     /*   model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(0.0f, 1.0f, 0.0f));*/

        glUniformMatrix4fv(glGetUniformLocation(cubeShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(cubeShader, "color"), 1, glm::value_ptr(glm::vec3(0.8f, 0.3f, 0.2f)));

        glBindVertexArray(teapot.VAO);
        glDrawArrays(GL_TRIANGLES, 0, teapot.vertices.size() / 3);

        // ����6x6��36��������
        glBindVertexArray(cubeVAO);
        for (int i = 0; i < 6; ++i)
        {
            for (int j = 0; j < 6; ++j)
            {
                // �޸�������λ�ã�ʹ������3D�ռ��зֲ�
                glm::vec3 cubePos = glm::vec3(
                    i * 2.0f - 5.0f,  // x: -5��5
                    j * 1.5f - 4.0f,  // y: -4��4
                    (i + j) * -1.5f   // z: ��ȱ仯
                );

                // ����ģ�;���
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, cubePos);
                // ���һЩ��תʹ�����忴�������������
                model = glm::rotate(model, (float)glfwGetTime() * glm::radians(20.0f * (i + j)),
                    glm::vec3(0.5f, 1.0f, 0.0f));

                // ������ɫ��ʹһЩ����������Բ�������Ч����
                glm::vec3 cubeColor = glm::vec3(
                    i * 0.15f + 0.2f,
                    j * 0.15f + 0.2f,
                    0.5f + i * 0.1f
                );
                if ((i + j) % 3 == 0) // ʹ�������������
                    cubeColor *= 3.0f;

                // ����ģ�;������ɫ����ɫ��
                glUniformMatrix4fv(glGetUniformLocation(cubeShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
                glUniform3fv(glGetUniformLocation(cubeShader, "color"), 1, glm::value_ptr(cubeColor));

                // ����������
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }

        // 2. ��ȡ�߹ⲿ�ֵ��ڶ�����ɫ����
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[0]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(brightShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[1]); // ʹ�õڶ�����ɫ����
        glUniform1i(glGetUniformLocation(brightShader, "scene"), 0);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 3. �Ը߹ⲿ��Ӧ�ø�˹ģ��
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10; // ģ����������
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

        // 4. ������Ⱦ���з���Ч����HDR��ɫ���嵽Ĭ��֡����
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(screenShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]); // ������Ⱦ����ɫ����
        glUniform1i(glGetUniformLocation(screenShader, "scene"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]); // ģ����ĸ߹���ɫ����
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