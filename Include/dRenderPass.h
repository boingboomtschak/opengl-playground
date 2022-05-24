// dRenderPass.h 

#ifndef DRENDERPASS_HDR
#define DRENDERPASS_HDR

#include <vector>
#include <stdexcept>
#include "glad.h"
#include "VecMat.h"

using std::vector;
using std::runtime_error;

struct dRenderPass {
	GLuint program = 0;
	dRenderPass() { };
	void loadShaders(const char** vertShaderSrc, const char** fragShaderSrc) {
		// Compile shaders
		GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
		if (!vertShader) throw runtime_error("Failed to create vertex shader!");
		glShaderSource(vertShader, 1, vertShaderSrc, NULL);
		glCompileShader(vertShader);
		checkCompileStatus(vertShader, "VERTEX");
		GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
		if (!fragShader) throw runtime_error("Failed to create fragment shader!");
		glShaderSource(fragShader, 1, fragShaderSrc, NULL);
		glCompileShader(fragShader);
		checkCompileStatus(fragShader, "FRAGMENT");
		// Link shaders to program
		program = glCreateProgram();
		if (!program) throw runtime_error("Failed to create shader program!");
		glAttachShader(program, vertShader);
		glAttachShader(program, fragShader);
		glLinkProgram(program);
		checkLinkStatus(program);
	}
	void loadShaders(const char** vertShaderSrc, const char** tessCntrlShaderSrc, const char** tessEvalShaderSrc, const char** geomShaderSrc, const char** fragShaderSrc) { };
	void loadShaders(const char** computeShaderSrc) { };
	void cleanup() {
		if (program) glDeleteProgram(program);
	}
	bool active() {
		GLint prog = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
		return (prog == program);
	}
	void use() {
		if (!program) throw runtime_error("Render pass used before shaders loaded!");
		if (!active()) glUseProgram(program);
	}
	void set(const char* key, GLuint val) {
		GLint id = glGetUniformLocation(program, key);
		if (!active()) printf("Program %d : Can't set uniform '%s' as program not active!", program, key);
		else if (id < 0) printf("Program %d : Can't find uniform '%s'!", program, key);
		else glUniform1ui(id, val);
	}
	void set(const char* key, GLint val) { 
		GLint id = glGetUniformLocation(program, key);
		if (!active()) printf("Program %d : Can't set uniform '%s' as program not active!", program, key);
		else if (id < 0) printf("Program %d : Can't find uniform '%s'!", program, key);
		else glUniform1i(id, val);
	};
	void set(const char* key, GLfloat val) { 
		GLint id = glGetUniformLocation(program, key);
		if (!active()) printf("Program %d : Can't set uniform '%s' as program not active!", program, key);
		else if (id < 0) printf("Program %d : Can't find uniform '%s'!", program, key);
		else glUniform1f(id, val);
	};
	void set(const char* key, vec2 val) { 
		GLint id = glGetUniformLocation(program, key);
		if (!active()) printf("Program %d : Can't set uniform '%s' as program not active!", program, key);
		else if (id < 0) printf("Program %d : Can't find uniform '%s'!", program, key);
		else glUniform2f(id, val.x, val.y);
	};
	void set(const char* key, vec3 val) { 
		GLint id = glGetUniformLocation(program, key);
		if (!active()) printf("Program %d : Can't set uniform '%s' as program not active!", program, key);
		else if (id < 0) printf("Program %d : Can't find uniform '%s'!", program, key);
		else glUniform3f(id, val.x, val.y, val.z);
	};
	void set(const char* key, vec4 val) { 
		GLint id = glGetUniformLocation(program, key);
		if (!active()) printf("Program %d : Can't set uniform '%s' as program not active!", program, key);
		else if (id < 0) printf("Program %d : Can't find uniform '%s'!", program, key);
		else glUniform4f(id, val.x, val.y, val.z, val.w);
	};
	void set(const char* key, int2 val) { 
		GLint id = glGetUniformLocation(program, key);
		if (!active()) printf("Program %d : Can't set uniform '%s' as program not active!", program, key);
		else if (id < 0) printf("Program %d : Can't find uniform '%s'!", program, key);
		else glUniform2i(id, val.i1, val.i2);
	};
	void set(const char* key, int3 val) { 
		GLint id = glGetUniformLocation(program, key);
		if (!active()) printf("Program %d : Can't set uniform '%s' as program not active!", program, key);
		else if (id < 0) printf("Program %d : Can't find uniform '%s'!", program, key);
		else glUniform3i(id, val.i1, val.i2, val.i3);
	};
	void set(const char* key, int4 val) { 
		GLint id = glGetUniformLocation(program, key);
		if (!active()) printf("Program %d : Can't set uniform '%s' as program not active!", program, key);
		else if (id < 0) printf("Program %d : Can't find uniform '%s'!", program, key);
		else glUniform4i(id, val.i1, val.i2, val.i3, val.i4);
	};
	void set(const char* key, mat3 val) { 
		GLint id = glGetUniformLocation(program, key);
		if (id < 0) printf("Program %d : Can't find uniform '%s'!", program, key);
		else glUniformMatrix3fv(id, 1, true, &val[0][0]);
	};
	void set(const char* key, mat4 val) { 
		GLint id = glGetUniformLocation(program, key);
		if (!active()) printf("Program %d : Can't set uniform '%s' as program not active!", program, key);
		else if (id < 0) printf("Program %d : Can't find uniform '%s'!", program, key);
		else glUniformMatrix4fv(id, 1, true, &val[0][0]);
	};
	void set(const char* key, int count, mat4* vals) {
		GLint id = glGetUniformLocation(program, key);
		if (!active()) printf("Program %d : Can't set uniform '%s' as program not active!", program, key);
		else if (id < 0) printf("Program %d : Can't find uniform '%s'!", program, key);
		else glUniformMatrix4fv(id, count, true, vals[0][0]);
	}
	void checkCompileStatus(GLuint shader, const char* shaderType) {
		GLint res;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &res);
		if (res == GL_FALSE) {
			GLint logLength;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
			if (logLength > 0) {
				GLsizei length;
				char* log = new char[logLength];
				glGetShaderInfoLog(shader, logLength, &length, log);
				printf("Failed to compile %s shader : \n%s", shaderType, log);
				delete[] log;
			} else printf("Failed to compile %s shader : (no logs)\n", shaderType);
			throw runtime_error("Error compiling shader!");
		}
	}
	void checkLinkStatus(GLuint _program) {
		GLint status;
		glGetProgramiv(_program, GL_LINK_STATUS, &status);
		if (status == GL_FALSE) {
			GLint logLength;
			glGetProgramiv(_program, GL_INFO_LOG_LENGTH, &logLength);
			if (logLength > 0) {
				char* log = new char[logLength];
				GLsizei length;
				glGetProgramInfoLog(_program, logLength, &length, log);
				printf("Failed to link program : \n%s", log);
				delete[] log;
			} else printf("Failed to link program : (no logs)\n");
			throw runtime_error("Error linking program");
		}
	}
};

#endif