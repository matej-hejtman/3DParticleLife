#include "ParticleLife/ShaderProgram.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pl {
namespace {

[[nodiscard]] std::string readText(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open shader: " + path.string());
    }
    std::ostringstream ss;
    ss << input.rdbuf();
    return ss.str();
}

[[nodiscard]] GLuint compile(GLenum stage, const std::filesystem::path& path) {
    const std::string source = readText(path);
    const char* sourcePtr = source.c_str();
    const GLuint shader = glCreateShader(stage);
    glShaderSource(shader, 1, &sourcePtr, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(length), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Shader compile failed: " + path.string() + "\n" + log);
    }
    return shader;
}

[[nodiscard]] GLuint link(const std::vector<GLuint>& shaders) {
    const GLuint program = glCreateProgram();
    for (GLuint shader : shaders) {
        glAttachShader(program, shader);
    }
    glLinkProgram(program);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(length), '\0');
        glGetProgramInfoLog(program, length, nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error("Program link failed:\n" + log);
    }

    for (GLuint shader : shaders) {
        glDetachShader(program, shader);
        glDeleteShader(shader);
    }
    return program;
}

}

ShaderProgram::~ShaderProgram() {
    if (m_id != 0) {
        glDeleteProgram(m_id);
    }
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : m_id(std::exchange(other.m_id, 0)) {}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) {
            glDeleteProgram(m_id);
        }
        m_id = std::exchange(other.m_id, 0);
    }
    return *this;
}

ShaderProgram ShaderProgram::compute(const std::filesystem::path& path) {
    return ShaderProgram(link({compile(GL_COMPUTE_SHADER, path)}));
}

ShaderProgram ShaderProgram::graphics(const std::filesystem::path& vertexPath,
                                      const std::filesystem::path& fragmentPath) {
    return ShaderProgram(link({compile(GL_VERTEX_SHADER, vertexPath),
                               compile(GL_FRAGMENT_SHADER, fragmentPath)}));
}

void ShaderProgram::use() const {
    glUseProgram(m_id);
}

GLint ShaderProgram::uniform(std::string_view name) const {
    const std::string key(name);
    if (const auto it = m_uniforms.find(key); it != m_uniforms.end()) {
        return it->second;
    }
    const GLint location = glGetUniformLocation(m_id, key.c_str());
    m_uniforms.emplace(key, location);
    return location;
}

void ShaderProgram::setInt(GLint location, GLint value) const {
    if (location >= 0) {
        glUniform1i(location, value);
    }
}

void ShaderProgram::setUInt(GLint location, GLuint value) const {
    if (location >= 0) {
        glUniform1ui(location, value);
    }
}

void ShaderProgram::setFloat(GLint location, GLfloat value) const {
    if (location >= 0) {
        glUniform1f(location, value);
    }
}

void ShaderProgram::setMat4(GLint location, const float* value) const {
    if (location >= 0) {
        glUniformMatrix4fv(location, 1, GL_FALSE, value);
    }
}

void ShaderProgram::setInt(std::string_view name, GLint value) const {
    setInt(uniform(name), value);
}

void ShaderProgram::setUInt(std::string_view name, GLuint value) const {
    setUInt(uniform(name), value);
}

void ShaderProgram::setFloat(std::string_view name, GLfloat value) const {
    setFloat(uniform(name), value);
}

void ShaderProgram::setMat4(std::string_view name, const float* value) const {
    setMat4(uniform(name), value);
}

}
