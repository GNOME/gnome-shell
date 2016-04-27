
#include <GLES2/gl2.h>

#include <cogl/cogl-gles2.h>

void
glBindTexture (GLenum target, GLuint texture)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glBindTexture (target, texture);
}

void
glBlendFunc (GLenum sfactor, GLenum dfactor)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glBlendFunc (sfactor, dfactor);
}

void
glClear (GLbitfield mask)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glClear (mask);
}

void
glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glClearColor (red, green, blue, alpha);
}

void
glClearStencil (GLint s)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glClearStencil (s);
}

void
glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glColorMask (red, green, blue, alpha);
}

void
glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
		     GLint x, GLint y, GLsizei width, GLsizei height)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glCopyTexSubImage2D (target, level, xoffset, yoffset, x, y, width,
			       height);
}

void
glDeleteTextures (GLsizei n, const GLuint * textures)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDeleteTextures (n, textures);
}

void
glDepthFunc (GLenum func)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDepthFunc (func);
}

void
glDepthMask (GLboolean flag)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDepthMask (flag);
}

void
glDisable (GLenum cap)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDisable (cap);
}

void
glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDrawArrays (mode, first, count);
}

void
glDrawElements (GLenum mode, GLsizei count, GLenum type,
		const GLvoid * indices)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDrawElements (mode, count, type, indices);
}

void
glEnable (GLenum cap)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glEnable (cap);
}

void
glFinish (void)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glFinish ();
}

void
glFlush (void)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glFlush ();
}

void
glFrontFace (GLenum mode)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glFrontFace (mode);
}

void
glCullFace (GLenum mode)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glCullFace (mode);
}

void
glGenTextures (GLsizei n, GLuint * textures)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGenTextures (n, textures);
}

GLenum
glGetError (void)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glGetError ();
}

void
glGetIntegerv (GLenum pname, GLint * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetIntegerv (pname, params);
}

void
glGetBooleanv (GLenum pname, GLboolean * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetBooleanv (pname, params);
}

void
glGetFloatv (GLenum pname, GLfloat * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetFloatv (pname, params);
}

const GLubyte *
glGetString (GLenum name)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glGetString (name);
}

void
glHint (GLenum target, GLenum mode)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glHint (target, mode);
}

GLboolean
glIsTexture (GLuint texture)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glIsTexture (texture);
}

void
glPixelStorei (GLenum pname, GLint param)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glPixelStorei (pname, param);
}

void
glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
	      GLenum type, GLvoid * pixels)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glReadPixels (x, y, width, height, format, type, pixels);
}

void
glScissor (GLint x, GLint y, GLsizei width, GLsizei height)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glScissor (x, y, width, height);
}

void
glStencilFunc (GLenum func, GLint ref, GLuint mask)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glStencilFunc (func, ref, mask);
}

void
glStencilMask (GLuint mask)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glStencilMask (mask);
}

void
glStencilOp (GLenum fail, GLenum zfail, GLenum zpass)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glStencilOp (fail, zfail, zpass);
}

void
glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width,
	      GLsizei height, GLint border, GLenum format, GLenum type,
	      const GLvoid * pixels)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glTexImage2D (target, level, internalformat, width, height, border,
			format, type, pixels);
}

void
glTexParameterf (GLenum target, GLenum pname, GLfloat param)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glTexParameterf (target, pname, param);
}

void
glTexParameterfv (GLenum target, GLenum pname, const GLfloat * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glTexParameterfv (target, pname, params);
}

void
glTexParameteri (GLenum target, GLenum pname, GLint param)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glTexParameteri (target, pname, param);
}

void
glTexParameteriv (GLenum target, GLenum pname, const GLint * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glTexParameteriv (target, pname, params);
}

void
glGetTexParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetTexParameterfv (target, pname, params);
}

void
glGetTexParameteriv (GLenum target, GLenum pname, GLint * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetTexParameteriv (target, pname, params);
}

void
glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
		 GLsizei width, GLsizei height, GLenum format, GLenum type,
		 const GLvoid * pixels)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glTexSubImage2D (target, level, xoffset, yoffset, width, height,
			   format, type, pixels);
}

void
glCopyTexImage2D (GLenum target, GLint level, GLenum internalformat, GLint x,
		  GLint y, GLsizei width, GLsizei height, GLint border)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glCopyTexImage2D (target, level, internalformat,
			    x, y, width, height, border);
}

void
glViewport (GLint x, GLint y, GLsizei width, GLsizei height)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glViewport (x, y, width, height);
}

GLboolean
glIsEnabled (GLenum cap)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glIsEnabled (cap);
}

void
glLineWidth (GLfloat width)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glLineWidth (width);
}

void
glPolygonOffset (GLfloat factor, GLfloat units)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glPolygonOffset (factor, units);
}

void
glDepthRangef (GLfloat near_val, GLfloat far_val)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDepthRangef (near_val, far_val);
}

void
glClearDepthf (GLclampf depth)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glClearDepthf (depth);
}

void
glCompressedTexImage2D (GLenum target, GLint level, GLenum internalformat,
			GLsizei width, GLsizei height, GLint border,
			GLsizei imageSize, const GLvoid * data)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glCompressedTexImage2D (target, level, internalformat, width,
				  height, border, imageSize, data);
}

void
glCompressedTexSubImage2D (GLenum target, GLint level, GLint xoffset,
			   GLint yoffset, GLsizei width, GLsizei height,
			   GLenum format, GLsizei imageSize,
			   const GLvoid * data)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glCompressedTexSubImage2D (target, level,
				     xoffset, yoffset, width, height,
				     format, imageSize, data);
}

void
glSampleCoverage (GLclampf value, GLboolean invert)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glSampleCoverage (value, invert);
}

void
glGetBufferParameteriv (GLenum target, GLenum pname, GLint * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetBufferParameteriv (target, pname, params);
}

void
glGenBuffers (GLsizei n, GLuint * buffers)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGenBuffers (n, buffers);
}

void
glBindBuffer (GLenum target, GLuint buffer)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glBindBuffer (target, buffer);
}

void
glBufferData (GLenum target, GLsizeiptr size, const GLvoid * data,
	      GLenum usage)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glBufferData (target, size, data, usage);
}

void
glBufferSubData (GLenum target, GLintptr offset, GLsizeiptr size,
		 const GLvoid * data)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glBufferSubData (target, offset, size, data);
}

void
glDeleteBuffers (GLsizei n, const GLuint * buffers)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDeleteBuffers (n, buffers);
}

GLboolean
glIsBuffer (GLuint buffer)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glIsBuffer (buffer);
}

void
glActiveTexture (GLenum texture)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glActiveTexture (texture);
}

void
glGenRenderbuffers (GLsizei n, GLuint * renderbuffers)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGenRenderbuffers (n, renderbuffers);
}

void
glDeleteRenderbuffers (GLsizei n, const GLuint * renderbuffers)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDeleteRenderbuffers (n, renderbuffers);
}

void
glBindRenderbuffer (GLenum target, GLuint renderbuffer)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glBindRenderbuffer (target, renderbuffer);
}

void
glRenderbufferStorage (GLenum target, GLenum internalformat, GLsizei width,
		       GLsizei height)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glRenderbufferStorage (target, internalformat, width, height);
}

void
glGenFramebuffers (GLsizei n, GLuint * framebuffers)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGenFramebuffers (n, framebuffers);
}

void
glBindFramebuffer (GLenum target, GLuint framebuffer)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glBindFramebuffer (target, framebuffer);
}

void
glFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget,
			GLuint texture, GLint level)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glFramebufferTexture2D (target, attachment,
				  textarget, texture, level);
}

void
glFramebufferRenderbuffer (GLenum target, GLenum attachment,
			   GLenum renderbuffertarget, GLuint renderbuffer)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glFramebufferRenderbuffer (target, attachment,
				     renderbuffertarget, renderbuffer);
}

GLboolean
glIsRenderbuffer (GLuint renderbuffer)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glIsRenderbuffer (renderbuffer);
}

GLenum
glCheckFramebufferStatus (GLenum target)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glCheckFramebufferStatus (target);
}

void
glDeleteFramebuffers (GLsizei n, const GLuint * framebuffers)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDeleteFramebuffers (n, framebuffers);
}

void
glGenerateMipmap (GLenum target)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGenerateMipmap (target);
}

void
glGetFramebufferAttachmentParameteriv (GLenum target, GLenum attachment,
				       GLenum pname, GLint * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetFramebufferAttachmentParameteriv (target,
						 attachment, pname, params);
}

void
glGetRenderbufferParameteriv (GLenum target, GLenum pname, GLint * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetRenderbufferParameteriv (target, pname, params);
}

GLboolean
glIsFramebuffer (GLuint framebuffer)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glIsFramebuffer (framebuffer);
}

void
glBlendEquation (GLenum mode)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glBlendEquation (mode);
}

void
glBlendColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glBlendColor (red, green, blue, alpha);
}

void
glBlendFuncSeparate (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
		     GLenum dstAlpha)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glBlendFuncSeparate (srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void
glBlendEquationSeparate (GLenum modeRGB, GLenum modeAlpha)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glBlendEquationSeparate (modeRGB, modeAlpha);
}

void
glReleaseShaderCompiler (void)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glReleaseShaderCompiler ();
}

void
glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype,
			    GLint * range, GLint * precision)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetShaderPrecisionFormat (shadertype, precisiontype,
				      range, precision);
}

void
glShaderBinary (GLsizei n, const GLuint * shaders, GLenum binaryformat,
		const GLvoid * binary, GLsizei length)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glShaderBinary (n, shaders, binaryformat, binary, length);
}

void
glStencilFuncSeparate (GLenum face, GLenum func, GLint ref, GLuint mask)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glStencilFuncSeparate (face, func, ref, mask);
}

void
glStencilMaskSeparate (GLenum face, GLuint mask)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glStencilMaskSeparate (face, mask);
}

void
glStencilOpSeparate (GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glStencilOpSeparate (face, fail, zfail, zpass);
}

GLuint
glCreateProgram (void)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glCreateProgram ();
}

GLuint
glCreateShader (GLenum shaderType)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glCreateShader (shaderType);
}

void
glShaderSource (GLuint shader, GLsizei count,
		const GLchar * const *string, const GLint * length)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glShaderSource (shader, count, string, length);
}

void
glCompileShader (GLuint shader)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glCompileShader (shader);
}

void
glDeleteShader (GLuint shader)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDeleteShader (shader);
}

void
glAttachShader (GLuint program, GLuint shader)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glAttachShader (program, shader);
}

void
glLinkProgram (GLuint program)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glLinkProgram (program);
}

void
glUseProgram (GLuint program)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUseProgram (program);
}

GLint
glGetUniformLocation (GLuint program, const char *name)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glGetUniformLocation (program, name);
}

void
glDeleteProgram (GLuint program)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDeleteProgram (program);
}

void
glGetShaderInfoLog (GLuint shader, GLsizei maxLength, GLsizei * length,
		    char *infoLog)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetShaderInfoLog (shader, maxLength, length, infoLog);
}

void
glGetShaderiv (GLuint shader, GLenum pname, GLint * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetShaderiv (shader, pname, params);
}

void
glVertexAttribPointer (GLuint index, GLint size, GLenum type,
		       GLboolean normalized, GLsizei stride,
		       const GLvoid * pointer)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glVertexAttribPointer (index, size, type, normalized, stride,
				 pointer);
}

void
glEnableVertexAttribArray (GLuint index)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glEnableVertexAttribArray (index);
}

void
glDisableVertexAttribArray (GLuint index)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDisableVertexAttribArray (index);
}

void
glUniform1f (GLint location, GLfloat v0)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform1f (location, v0);
}

void
glUniform2f (GLint location, GLfloat v0, GLfloat v1)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform2f (location, v0, v1);
}

void
glUniform3f (GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform3f (location, v0, v1, v2);
}

void
glUniform4f (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform4f (location, v0, v1, v2, v3);
}

void
glUniform1fv (GLint location, GLsizei count, const GLfloat * value)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform1fv (location, count, value);
}

void
glUniform2fv (GLint location, GLsizei count, const GLfloat * value)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform2fv (location, count, value);
}

void
glUniform3fv (GLint location, GLsizei count, const GLfloat * value)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform3fv (location, count, value);
}

void
glUniform4fv (GLint location, GLsizei count, const GLfloat * value)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform4fv (location, count, value);
}

void
glUniform1i (GLint location, GLint v0)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform1i (location, v0);
}

void
glUniform2i (GLint location, GLint v0, GLint v1)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform2i (location, v0, v1);
}

void
glUniform3i (GLint location, GLint v0, GLint v1, GLint v2)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform3i (location, v0, v1, v2);
}

void
glUniform4i (GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform4i (location, v0, v1, v2, v3);
}

void
glUniform1iv (GLint location, GLsizei count, const GLint * value)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform1iv (location, count, value);
}

void
glUniform2iv (GLint location, GLsizei count, const GLint * value)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform2iv (location, count, value);
}

void
glUniform3iv (GLint location, GLsizei count, const GLint * value)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform3iv (location, count, value);
}

void
glUniform4iv (GLint location, GLsizei count, const GLint * value)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniform4iv (location, count, value);
}

void
glUniformMatrix2fv (GLint location, GLsizei count, GLboolean transpose,
		    const GLfloat * value)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniformMatrix2fv (location, count, transpose, value);
}

void
glUniformMatrix3fv (GLint location, GLsizei count, GLboolean transpose,
		    const GLfloat * value)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniformMatrix3fv (location, count, transpose, value);
}

void
glUniformMatrix4fv (GLint location, GLsizei count, GLboolean transpose,
		    const GLfloat * value)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glUniformMatrix4fv (location, count, transpose, value);
}

void
glGetUniformfv (GLuint program, GLint location, GLfloat * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetUniformfv (program, location, params);
}

void
glGetUniformiv (GLuint program, GLint location, GLint * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetUniformiv (program, location, params);
}

void
glGetProgramiv (GLuint program, GLenum pname, GLint * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetProgramiv (program, pname, params);
}

void
glGetProgramInfoLog (GLuint program, GLsizei bufSize, GLsizei * length,
		     char *infoLog)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetProgramInfoLog (program, bufSize, length, infoLog);
}

void
glVertexAttrib1f (GLuint indx, GLfloat x)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glVertexAttrib1f (indx, x);
}

void
glVertexAttrib1fv (GLuint indx, const GLfloat * values)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glVertexAttrib1fv (indx, values);
}

void
glVertexAttrib2f (GLuint indx, GLfloat x, GLfloat y)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glVertexAttrib2f (indx, x, y);
}

void
glVertexAttrib2fv (GLuint indx, const GLfloat * values)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glVertexAttrib2fv (indx, values);
}

void
glVertexAttrib3f (GLuint indx, GLfloat x, GLfloat y, GLfloat z)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glVertexAttrib3f (indx, x, y, z);
}

void
glVertexAttrib3fv (GLuint indx, const GLfloat * values)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glVertexAttrib3fv (indx, values);
}

void
glVertexAttrib4f (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glVertexAttrib4f (index, x, y, z, w);
}

void
glVertexAttrib4fv (GLuint indx, const GLfloat * values)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glVertexAttrib4fv (indx, values);
}

void
glGetVertexAttribfv (GLuint index, GLenum pname, GLfloat * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetVertexAttribfv (index, pname, params);
}

void
glGetVertexAttribiv (GLuint index, GLenum pname, GLint * params)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetVertexAttribiv (index, pname, params);
}

void
glGetVertexAttribPointerv (GLuint index, GLenum pname, GLvoid ** pointer)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetVertexAttribPointerv (index, pname, pointer);
}

GLint
glGetAttribLocation (GLuint program, const char *name)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glGetAttribLocation (program, name);
}

void
glBindAttribLocation (GLuint program, GLuint index, const GLchar * name)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glBindAttribLocation (program, index, name);
}

void
glGetActiveAttrib (GLuint program, GLuint index, GLsizei bufsize,
		   GLsizei * length, GLint * size, GLenum * type,
		   GLchar * name)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetActiveAttrib (program, index, bufsize, length, size, type,
			     name);
}

void
glGetActiveUniform (GLuint program, GLuint index, GLsizei bufsize,
		    GLsizei * length, GLint * size, GLenum * type,
		    GLchar * name)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetActiveUniform (program, index, bufsize, length, size, type,
			      name);
}

void
glDetachShader (GLuint program, GLuint shader)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glDetachShader (program, shader);
}

void
glGetAttachedShaders (GLuint program, GLsizei maxcount, GLsizei * count,
		      GLuint * shaders)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetAttachedShaders (program, maxcount, count, shaders);
}

void
glGetShaderSource (GLuint shader, GLsizei bufsize, GLsizei * length,
		   GLchar * source)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glGetShaderSource (shader, bufsize, length, source);
}

GLboolean
glIsShader (GLuint shader)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glIsShader (shader);
}

GLboolean
glIsProgram (GLuint program)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  return vtable->glIsProgram (program);
}

void
glValidateProgram (GLuint program)
{
  CoglGLES2Vtable *vtable = cogl_gles2_get_current_vtable ();
  vtable->glValidateProgram (program);
}
