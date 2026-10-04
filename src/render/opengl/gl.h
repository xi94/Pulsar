#pragma once

#include <cstddef>

using GLenum     = unsigned int;
using GLboolean  = unsigned char;
using GLbitfield = unsigned int;
using GLint      = int;
using GLuint     = unsigned int;
using GLsizei    = int;
using GLfloat    = float;
using GLchar     = char;
using GLubyte    = unsigned char;
using GLintptr   = std::ptrdiff_t;
using GLsizeiptr = std::ptrdiff_t;

#define GL_FALSE                0
#define GL_TRUE                 1
#define GL_ONE                  1
#define GL_TRIANGLES            0x0004
#define GL_SRC_ALPHA            0x0302
#define GL_ONE_MINUS_SRC_ALPHA  0x0303
#define GL_BLEND                0x0BE2
#define GL_SCISSOR_TEST         0x0C11
#define GL_TEXTURE_2D           0x0DE1
#define GL_UNSIGNED_BYTE        0x1401
#define GL_UNSIGNED_INT         0x1405
#define GL_FLOAT                0x1406
#define GL_RGBA                 0x1908
#define GL_RENDERER             0x1F01
#define GL_VERSION              0x1F02
#define GL_NEAREST              0x2600
#define GL_LINEAR               0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER   0x2800
#define GL_TEXTURE_MIN_FILTER   0x2801
#define GL_TEXTURE_WRAP_S       0x2802
#define GL_TEXTURE_WRAP_T       0x2803
#define GL_COLOR_BUFFER_BIT     0x4000
#define GL_RGBA8                0x8058
#define GL_CLAMP_TO_EDGE        0x812F
#define GL_TEXTURE_MAX_LEVEL    0x813D
#define GL_ARRAY_BUFFER         0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW          0x88E0
#define GL_UNIFORM_BUFFER       0x8A11
#define GL_FRAGMENT_SHADER      0x8B30
#define GL_VERTEX_SHADER        0x8B31
#define GL_COMPILE_STATUS       0x8B81
#define GL_LINK_STATUS          0x8B82
#define GL_INFO_LOG_LENGTH      0x8B84
#define GL_READ_FRAMEBUFFER     0x8CA8
#define GL_DRAW_FRAMEBUFFER     0x8CA9
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_ATTACHMENT0    0x8CE0
#define GL_FRAMEBUFFER          0x8D40
#define GL_RENDERBUFFER         0x8D41
#define GL_MAX_SAMPLES          0x8D57
#define GL_INVALID_INDEX        0xFFFFFFFFu

#define PULSAR_GL_FUNCTIONS(X)                                                                                                                                 \
	X(void, attach_shader, glAttachShader, GLuint, GLuint)                                                                                                     \
	X(void, bind_buffer, glBindBuffer, GLenum, GLuint)                                                                                                         \
	X(void, bind_buffer_base, glBindBufferBase, GLenum, GLuint, GLuint)                                                                                        \
	X(void, bind_framebuffer, glBindFramebuffer, GLenum, GLuint)                                                                                               \
	X(void, bind_renderbuffer, glBindRenderbuffer, GLenum, GLuint)                                                                                             \
	X(void, bind_sampler, glBindSampler, GLuint, GLuint)                                                                                                       \
	X(void, bind_texture, glBindTexture, GLenum, GLuint)                                                                                                       \
	X(void, bind_vertex_array, glBindVertexArray, GLuint)                                                                                                      \
	X(void, blend_func_separate, glBlendFuncSeparate, GLenum, GLenum, GLenum, GLenum)                                                                          \
	X(void, blit_framebuffer, glBlitFramebuffer, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)                                   \
	X(void, buffer_data, glBufferData, GLenum, GLsizeiptr, const void*, GLenum)                                                                                \
	X(void, buffer_sub_data, glBufferSubData, GLenum, GLintptr, GLsizeiptr, const void*)                                                                       \
	X(GLenum, check_framebuffer_status, glCheckFramebufferStatus, GLenum)                                                                                      \
	X(void, clear, glClear, GLbitfield)                                                                                                                        \
	X(void, clear_color, glClearColor, GLfloat, GLfloat, GLfloat, GLfloat)                                                                                     \
	X(void, compile_shader, glCompileShader, GLuint)                                                                                                           \
	X(GLuint, create_program, glCreateProgram)                                                                                                                 \
	X(GLuint, create_shader, glCreateShader, GLenum)                                                                                                           \
	X(void, delete_buffers, glDeleteBuffers, GLsizei, const GLuint*)                                                                                           \
	X(void, delete_framebuffers, glDeleteFramebuffers, GLsizei, const GLuint*)                                                                                 \
	X(void, delete_program, glDeleteProgram, GLuint)                                                                                                           \
	X(void, delete_renderbuffers, glDeleteRenderbuffers, GLsizei, const GLuint*)                                                                               \
	X(void, delete_samplers, glDeleteSamplers, GLsizei, const GLuint*)                                                                                         \
	X(void, delete_shader, glDeleteShader, GLuint)                                                                                                             \
	X(void, delete_textures, glDeleteTextures, GLsizei, const GLuint*)                                                                                         \
	X(void, delete_vertex_arrays, glDeleteVertexArrays, GLsizei, const GLuint*)                                                                                \
	X(void, disable, glDisable, GLenum)                                                                                                                        \
	X(void, draw_elements, glDrawElements, GLenum, GLsizei, GLenum, const void*)                                                                               \
	X(void, enable, glEnable, GLenum)                                                                                                                          \
	X(void, enable_vertex_attrib_array, glEnableVertexAttribArray, GLuint)                                                                                     \
	X(void, framebuffer_renderbuffer, glFramebufferRenderbuffer, GLenum, GLenum, GLenum, GLuint)                                                               \
	X(void, gen_buffers, glGenBuffers, GLsizei, GLuint*)                                                                                                       \
	X(void, gen_framebuffers, glGenFramebuffers, GLsizei, GLuint*)                                                                                             \
	X(void, gen_renderbuffers, glGenRenderbuffers, GLsizei, GLuint*)                                                                                           \
	X(void, gen_samplers, glGenSamplers, GLsizei, GLuint*)                                                                                                     \
	X(void, gen_textures, glGenTextures, GLsizei, GLuint*)                                                                                                     \
	X(void, gen_vertex_arrays, glGenVertexArrays, GLsizei, GLuint*)                                                                                            \
	X(void, get_integerv, glGetIntegerv, GLenum, GLint*)                                                                                                       \
	X(void, get_program_info_log, glGetProgramInfoLog, GLuint, GLsizei, GLsizei*, GLchar*)                                                                     \
	X(void, get_programiv, glGetProgramiv, GLuint, GLenum, GLint*)                                                                                             \
	X(void, get_shader_info_log, glGetShaderInfoLog, GLuint, GLsizei, GLsizei*, GLchar*)                                                                       \
	X(void, get_shaderiv, glGetShaderiv, GLuint, GLenum, GLint*)                                                                                               \
	X(const GLubyte*, get_string, glGetString, GLenum)                                                                                                         \
	X(GLuint, get_uniform_block_index, glGetUniformBlockIndex, GLuint, const GLchar*)                                                                          \
	X(void, link_program, glLinkProgram, GLuint)                                                                                                               \
	X(void, renderbuffer_storage_multisample, glRenderbufferStorageMultisample, GLenum, GLsizei, GLenum, GLsizei, GLsizei)                                     \
	X(void, sampler_parameteri, glSamplerParameteri, GLuint, GLenum, GLint)                                                                                    \
	X(void, scissor, glScissor, GLint, GLint, GLsizei, GLsizei)                                                                                                \
	X(void, shader_source, glShaderSource, GLuint, GLsizei, const GLchar* const*, const GLint*)                                                                \
	X(void, tex_image_2d, glTexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)                                            \
	X(void, tex_parameteri, glTexParameteri, GLenum, GLenum, GLint)                                                                                            \
	X(void, tex_sub_image_2d, glTexSubImage2D, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)                                     \
	X(void, uniform_block_binding, glUniformBlockBinding, GLuint, GLuint, GLuint)                                                                              \
	X(void, use_program, glUseProgram, GLuint)                                                                                                                 \
	X(void, vertex_attrib_pointer, glVertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)                                              \
	X(void, viewport, glViewport, GLint, GLint, GLsizei, GLsizei)

struct Gl {
#define PULSAR_GL_DECLARE(return_type, name, symbol, ...) return_type (*name)(__VA_ARGS__) = nullptr;
	PULSAR_GL_FUNCTIONS(PULSAR_GL_DECLARE)
#undef PULSAR_GL_DECLARE
};
