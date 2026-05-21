#include "ParticleLife/GlBuffer.hpp"

#include <utility>

namespace pl {

GlBuffer::GlBuffer() {
    glGenBuffers(1, &m_id);
}

GlBuffer::~GlBuffer() {
    if (m_id != 0) {
        glDeleteBuffers(1, &m_id);
    }
}

GlBuffer::GlBuffer(GlBuffer&& other) noexcept
    : m_id(std::exchange(other.m_id, 0)), m_size(std::exchange(other.m_size, 0)) {}

GlBuffer& GlBuffer::operator=(GlBuffer&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) {
            glDeleteBuffers(1, &m_id);
        }
        m_id = std::exchange(other.m_id, 0);
        m_size = std::exchange(other.m_size, 0);
    }
    return *this;
}

void GlBuffer::allocate(GLsizeiptr size, const void* data, GLenum usage) {
    m_size = size;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_id);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, usage);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void GlBuffer::update(GLintptr offset, GLsizeiptr size, const void* data) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_id);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void GlBuffer::bindBase(GLenum target, GLuint binding) const {
    glBindBufferBase(target, binding, m_id);
}

}
