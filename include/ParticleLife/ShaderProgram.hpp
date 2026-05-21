#pragma once

#include <filesystem>
#include <glad/glad.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace pl {

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    static ShaderProgram compute(const std::filesystem::path& path);
    static ShaderProgram graphics(const std::filesystem::path& vertexPath,
                                  const std::filesystem::path& fragmentPath);

    void use() const;
    [[nodiscard]] GLint uniform(std::string_view name) const;
    void setInt(GLint location, GLint value) const;
    void setUInt(GLint location, GLuint value) const;
    void setFloat(GLint location, GLfloat value) const;
    void setMat4(GLint location, const float* value) const;
    void setInt(std::string_view name, GLint value) const;
    void setUInt(std::string_view name, GLuint value) const;
    void setFloat(std::string_view name, GLfloat value) const;
    void setMat4(std::string_view name, const float* value) const;

    [[nodiscard]] GLuint id() const { return m_id; }

private:
    explicit ShaderProgram(GLuint id) : m_id(id) {}
    GLuint m_id = 0;
    mutable std::unordered_map<std::string, GLint> m_uniforms;
};

}
