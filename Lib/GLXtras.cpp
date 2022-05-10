// GLXtras.cpp - GLSL support (c) 2019-2022 Jules Bloomenthal

#include "GLXtras.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <vector>

// Print OpenGL, GLSL Details

const char* getErrorString(GLenum err) {
	switch(err) {
		case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
		case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
		case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
		default: return "UNKNOWN_GL_ERROR_CODE";
	}
}

int PrintGLErrors(const char *title) {
	char buf[1000];
	int nErrors = 0;
	buf[0] = 0;
	for (;;) {
		GLenum n = glGetError();
		if (n == GL_NO_ERROR)
			break;
		sprintf(buf+strlen(buf), "%s%s", !nErrors++? "" : ", ", getErrorString(n));
			// do not call Debug() while looping through errors, so accumulate in buf
	}
	if (nErrors) {
		if (title) printf("%s (GL errors): %s\n", title, buf);
		else printf("GL errors: %s\n", buf);
	}
	return nErrors;
}

void PrintVersionInfo() {
	const GLubyte *renderer    = glGetString(GL_RENDERER);
	const GLubyte *vendor      = glGetString(GL_VENDOR);
	const GLubyte *version     = glGetString(GL_VERSION);
	const GLubyte *glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
	printf("GL vendor: %s\n", vendor);
	printf("GL renderer: %s\n", renderer);
	printf("GL version: %s\n", version);
	printf("GLSL version: %s\n", glslVersion);
	// GLint major, minor;
	// glGetIntegerv(GL_MAJOR_VERSION, &major);
	// glGetIntegerv(GL_MINOR_VERSION, &minor);
	// printf("GL version (integer): %d.%d\n", major, minor);
}

void PrintExtensions() {
	const GLubyte *extensions = glGetString(GL_EXTENSIONS);
	const char *skip = "(, \t\n";
	char buf[100];
	printf("\nGL extensions:\n");
	if (extensions)
		for (char *c = (char *) extensions; *c; ) {
				c += strspn(c, skip);
				size_t nchars = strcspn(c, skip);
				strncpy(buf, c, nchars);
				buf[nchars] = 0;
				printf("  %s\n", buf);
				c += nchars;
		}
}

void PrintProgramLog(int programID) {
	GLint logLen;
	glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &logLen);
	if (logLen > 0) {
		char *log = new char[logLen];
		GLsizei written;
		glGetProgramInfoLog(programID, logLen, &written, log);
		printf("Program log:\n%s", log);
		delete [] log;
	}
}

void PrintProgramAttributes(int programID) {
	GLint maxLength, nAttribs;
	glGetProgramiv(programID, GL_ACTIVE_ATTRIBUTES, &nAttribs);
	glGetProgramiv(programID, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength);
	char *name = new char[maxLength];
	GLint written, size;
	GLenum type;
	for (int i = 0; i < nAttribs; i++) {
		glGetActiveAttrib(programID, i, maxLength, &written, &size, &type, name);
		GLint location = glGetAttribLocation(programID, name);
		printf("    %-5i  |  %s\n", location, name);
	}
	delete [] name;
}

void PrintProgramUniforms(int programID) {
	GLenum type;
	GLchar name[201];
	GLint nUniforms, length, size;
	glGetProgramiv(programID, GL_ACTIVE_UNIFORMS, &nUniforms);
	printf("  uniforms\n");
	for (int i = 0; i < nUniforms; i++) {
		glGetActiveUniform(programID, i,  200,  &length, &size, &type, name);
		printf("    %s\n", name);
	}
}

// Compilation

GLuint CompileShaderViaFile(const char *filename, GLint type) {
		FILE* fp = fopen(filename, "r");
		if (fp == NULL) {
			printf("can't open file\n");
			return 0;
		}
		fseek(fp, 0L, SEEK_END);
		long maxSize = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		char *buf = new char[maxSize+1], c;
		int nchars = 0;
		while ((c = fgetc(fp)) != EOF)
			buf[nchars++] = c;
		buf[nchars] = 0;
		fclose(fp);
		GLuint s = CompileShaderViaCode((const char **) &buf, type);
		delete [] buf;
		return s;
}

GLuint CompileShaderViaCode(const char **code, GLint type) {
	GLuint shader = glCreateShader(type);
	if (!shader) {
		PrintGLErrors();
		return false;
	}
	glShaderSource(shader, 1, code, NULL);
	glCompileShader(shader);
	// check compile status
	GLint result;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		// report logged errors
		GLint logLen;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
		if (logLen > 0) {
			GLsizei written;
			char *log = new char[logLen];
			glGetShaderInfoLog(shader, logLen, &written, log);
			printf("compilation failed: %s", log);
			delete [] log;
		}
		else printf("shader compilation failed\n");
		return 0;
	}
	return shader;
}

// Linking

GLuint LinkProgramViaCode(const char **vertexCode, const char **pixelCode) {
	GLuint vshader = CompileShaderViaCode(vertexCode, GL_VERTEX_SHADER);
	GLuint pshader = CompileShaderViaCode(pixelCode, GL_FRAGMENT_SHADER);
	GLuint p = LinkProgram(vshader, pshader);
	glDetachShader(p, vshader);
	glDetachShader(p, pshader);
	glDeleteShader(vshader);
	glDeleteShader(pshader);
	return p;
}

GLuint LinkProgramViaCode(const char **vertexCode,
						  const char **tessellationControlCode,
						  const char **tessellationEvalCode,
						  const char **geometryCode,
						  const char **pixelCode) {
	GLuint vshader = CompileShaderViaCode(vertexCode, GL_VERTEX_SHADER);
	GLuint tcshader = 0;
#ifdef GL_TESS_EVALUATION_SHADER
	if (tessellationControlCode)
		tcshader = CompileShaderViaCode(tessellationControlCode, GL_TESS_CONTROL_SHADER);
#endif
	GLuint teshader = 0;
#ifdef GL_TESS_EVALUATION_SHADER
	if (tessellationEvalCode)
		teshader = CompileShaderViaCode(tessellationEvalCode, GL_TESS_EVALUATION_SHADER);
#endif
	GLuint gshader = geometryCode? CompileShaderViaCode(geometryCode, GL_GEOMETRY_SHADER) : 0;
	GLuint pshader = CompileShaderViaCode(pixelCode, GL_FRAGMENT_SHADER);
	return LinkProgram(vshader, tcshader, teshader, gshader, pshader);
}

// Apple devices are stuck at 4.1 core, so GL_COMPUTE_SHADER undefined
#ifndef __APPLE__
void LinkProgramViaCode(GLuint computeProgram, const char **computeCode) {
	GLuint computeShader = CompileShaderViaCode(computeCode, GL_COMPUTE_SHADER);
	glAttachShader(computeProgram, computeShader);
	glLinkProgram(computeProgram);
	glDetachShader(computeProgram, computeShader);
	glDeleteShader(computeShader);
	GLint status;
	glGetProgramiv(computeProgram, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) PrintProgramLog(computeProgram);
}

GLuint LinkProgramViaCode(const char **computeCode) {
	GLuint computeProgram = glCreateProgram();
	LinkProgramViaCode(computeProgram, computeCode);
	return computeProgram;
}

GLuint LinkProgramViaFile(const char *computeShaderFile) {
	GLuint cshader = CompileShaderViaFile(computeShaderFile, GL_COMPUTE_SHADER);
	if (!cshader)
		return 0;
	GLuint program = glCreateProgram();
	glAttachShader(program, cshader);
	glLinkProgram(program);
	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) PrintProgramLog(program);
	return program;
}
#endif

GLuint LinkProgram(GLuint vshader, GLuint pshader) {
	return LinkProgram(vshader, 0, 0, 0, pshader);
}

GLuint LinkProgram(GLuint vshader, GLuint tcshader, GLuint teshader, GLuint gshader, GLuint pshader) {
	GLuint program = 0;
	// create shader program
	if (vshader && pshader)
		program = glCreateProgram();
	if (program > 0) {
		// attach shaders to program
		glAttachShader(program, vshader);
		if (tcshader > 0) glAttachShader(program, tcshader);
		if (teshader > 0) glAttachShader(program, teshader);
		if (gshader > 0) glAttachShader(program, gshader);
		glAttachShader(program, pshader);
		// link and verify
		glLinkProgram(program);
		GLint status;
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		if (status == GL_FALSE) PrintProgramLog(program);
	}
	return program;
}

GLuint LinkProgramViaFile(const char *vertexShaderFile, const char *pixelShaderFile) {
	GLuint vshader = CompileShaderViaFile(vertexShaderFile, GL_VERTEX_SHADER);
	GLuint fshader = CompileShaderViaFile(pixelShaderFile, GL_FRAGMENT_SHADER);
	return LinkProgram(vshader, fshader);
}

// Miscellany

int CurrentProgram() {
	int program = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	return program;
}

void DeleteProgram(int program) {
	GLint nShaders = 0;
	GLuint shaderNames[10];
	glGetProgramiv(program, GL_ATTACHED_SHADERS, &nShaders);
	glGetAttachedShaders(program, nShaders, NULL, shaderNames);
	for (int i = 0; i < nShaders; i++)
		glDeleteShader(shaderNames[i]);
	glDeleteProgram(program);
}

// Binary Read/Write

void WriteProgramBinary(GLuint program, const char *filename) {
	GLenum binaryFormat = 0;
	GLsizei sizeBinary = 0, sizeEnum = sizeof(GLenum);
	glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &sizeBinary);
	std::vector<char> data(sizeBinary);
	glGetProgramBinary(program, sizeBinary, NULL, &binaryFormat, &data[0]);
	FILE *out = fopen(filename, "wb");
	fwrite(&binaryFormat, sizeEnum, 1, out);
	fwrite(&data[0], 1, sizeBinary, out);
	fclose(out);
}

bool ReadProgramBinary(GLuint program, const char *filename) {
	FILE *in = fopen(filename, "rb");
	if (in != NULL) {
		fseek(in, 0, SEEK_END);
		long filesize = ftell(in);
		int sizeEnum = sizeof(GLenum), sizeBinary = (int)(filesize-sizeEnum);
		std::vector<char> data(sizeBinary);
		GLenum binaryFormat;
		fseek(in, 0, 0);
		fread((char *) &binaryFormat, sizeEnum, 1, in);
		fread((char *) &data[0], 1, sizeBinary, in);
		fclose(in);
		glProgramBinary(program, binaryFormat, &data[0], sizeBinary);
		return true;
	}
	return false;
}

GLuint ReadProgramBinary(const char *filename) {
	GLuint program = glCreateProgram();
	if (!ReadProgramBinary(program, filename)) {
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

// Uniform Access

bool Bad(bool report, const char *name) {
	if (report)
		printf("can't find named uniform: %s\n", name);
	return false;
}

bool SetUniform(int program, const char *name, bool val, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform1ui(id, val);
	return true;
}

bool SetUniform(int program, const char *name, int val, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform1i(id, val);
	return true;
}

/* bool SetUniform(int program, const char *name, GLuint val, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform1ui(id, val);
	return true;
} */

bool SetUniformv(int program, const char *name, int count, int *v, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform1iv(id, count, v);
	return true;
}

bool SetUniform(int program, const char *name, float val, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform1f(id, val);
	return true;
}

bool SetUniformv(int program, const char *name, int count, float *v, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform1fv(id, count, v);
	return true;
}

bool SetUniform(int program, const char *name, vec2 v, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform2f(id, v.x, v.y);
	return true;
}

bool SetUniform(int program, const char *name, vec3 v, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform3f(id, v.x, v.y, v.z);
	return true;
}

bool SetUniform(int program, const char *name, vec4 v, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform4f(id, v.x, v.y, v.z, v.w);
	return true;
}

bool SetUniform(int program, const char *name, vec3 *v, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform3fv(id, 1, (float *) v);
	return true;
}

bool SetUniform(int program, const char *name, vec4 *v, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform4fv(id, 1, (float *) v);
	return true;
}

bool SetUniform3(int program, const char *name, float *v, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform3fv(id, 1, v);
	return true;
}

bool SetUniform3v(int program, const char *name, int count, float *v, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform3fv(id, count, v);
	return true;
}

bool SetUniform4v(int program, const char *name, int count, float *v, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniform4fv(id, count, v);
	return true;
}

bool SetUniform(int program, const char *name, mat4 m, bool report) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(report, name);
	glUniformMatrix4fv(id, 1, true, (float *) &m[0][0]);
	return true;
}

// Attribute Access

void DisableVertexAttribute(int program, const char *name) {
	GLint id = glGetAttribLocation(program, name);
	if (id >= 0)
		glDisableVertexAttribArray(id);
}

int EnableVertexAttribute(int program, const char *name) {
	GLint id = glGetAttribLocation(program, name);
	if (id >= 0)
		glEnableVertexAttribArray(id);
	return id;
}

void VertexAttribPointer(int program, const char *name, GLint ncomponents, GLsizei stride, const GLvoid *offset) {
	GLint id = EnableVertexAttribute(program, name);
	if (id < 0)
		printf("cant find attribute %s\n", name);
	glVertexAttribPointer(id, ncomponents, GL_FLOAT, GL_FALSE, stride, offset);
}
