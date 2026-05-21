#pragma once

#include <cstddef>
#include <glad/glad.h>

namespace pl {

class GlBuffer {
public:
    GlBuffer();
    ~GlBuffer();

    GlBuffer(const GlBuffer&) = delete;
    GlBuffer& operator=(const GlBuffer&) = delete;
    GlBuffer(GlBuffer&& other) noexcept;
    GlBuffer& operator=(GlBuffer&& other) noexcept;

    void allocate(GLsizeiptr size, const void* data, GLenum usage);
    void update(GLintptr offset, GLsizeiptr size, const void* data);
    void bindBase(GLenum target, GLuint binding) const;

    [[nodiscard]] GLuint id() const { return m_id; }
    [[nodiscard]] GLsizeiptr size() const { return m_size; }

private:
    GLuint m_id = 0;
    GLsizeiptr m_size = 0;
};

}
