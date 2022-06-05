// dRenderPass.h 

#ifndef DRENDERPASS_HDR
#define DRENDERPASS_HDR

#include <vector>
#include <stdexcept>
#include <map>
#include <string>
#include "glad.h"
#include "VecMat.h"

using std::vector;
using std::runtime_error;
using std::map;
using std::string;

struct RenderPass {
	GLuint program = 0;
	map<string, GLint> uniform_ids;
	RenderPass() { };
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
		getUniformIds();
	}
	// TODO
	void loadShaders(const char** vertShaderSrc, const char** tessCntrlShaderSrc, const char** tessEvalShaderSrc, const char** geomShaderSrc, const char** fragShaderSrc) { };
	// TODO
	void loadShaders(const char** computeShaderSrc) {  };
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
	void getUniformIds() {
		if (!program)
			throw runtime_error("Uniform IDs retrieved before shaders loaded!");
		int count;
		glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
		for (int i = 0; i < count; i++) {
			GLsizei nameLen;
			GLint size;
			GLenum type;
			GLchar name[32]{ '\0' };
			glGetActiveUniform(program, (GLuint)i, 32, &nameLen, &size, &type, name);
			string nameStr(name);
			uniform_ids[nameStr] = i;
		}
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
	template<typename T>
	void set(const char* key, T val) {
		if (GLint id = getId(key); id >= 0) uniform(id, reinterpret_cast<T*>(&val));
	}
	template<typename T>
	void set(const char* key, T* vals, GLsizei count) {
		if (GLint id = getId(key); id >= 0) uniform(id, reinterpret_cast<T*>(vals), count);
	}
private:
	GLint getId(const char* key) {
		map<string, GLint>::iterator it = uniform_ids.find(string(key));
		if (!active()) printf("Program %d : Can't set uniform '%s' as program not active!", program, key);
		else if (it == uniform_ids.end()) printf("Program %d : Can't find uniform '%s'!", program, key);
		else return it->second;
		return -1;
	}
	void uniform(GLint id, GLuint* val) { glUniform1ui(id, *val); }
	void uniform(GLint id, GLint* val) { glUniform1i(id, *val); }
	void uniform(GLint id, GLfloat* val) { glUniform1f(id, *val); }
	void uniform(GLint id, vec2* val) { glUniform2f(id, val->x, val->y); }
	void uniform(GLint id, vec3* val) { glUniform3f(id, val->x, val->y, val->z); }
	void uniform(GLint id, vec4* val) { glUniform4f(id, val->x, val->y, val->z, val->w); }
	void uniform(GLint id, int2* val) { glUniform2i(id, val->i1, val->i2); }
	void uniform(GLint id, int3* val) { glUniform3i(id, val->i1, val->i2, val->i3); }
	void uniform(GLint id, int4* val) { glUniform4i(id, val->i1, val->i2, val->i3, val->i4); }
	void uniform(GLint id, mat3* val) { glUniformMatrix3fv(id, 1, true, (&val)[0][0]); }
	void uniform(GLint id, mat4* val) { glUniformMatrix4fv(id, 1, true, (&val)[0][0]); }
	void uniform(GLint id, GLuint* vals, GLsizei count) { glUniform1uiv(id, count, vals); }
	void uniform(GLint id, GLint* vals, GLsizei count) { glUniform1iv(id, count, vals); }
	void uniform(GLint id, GLfloat* vals, GLsizei count) { glUniform1fv(id, count, vals); }
	void uniform(GLint id, vec2* vals, GLsizei count) { glUniform2fv(id, count, &(vals->x)); }
	void uniform(GLint id, vec3* vals, GLsizei count) { glUniform3fv(id, count, &(vals->x)); }
	void uniform(GLint id, vec4* vals, GLsizei count) { glUniform4fv(id, count, &(vals->x)); }
	void uniform(GLint id, int2* vals, GLsizei count) { glUniform2iv(id, count, &(vals->i1)); }
	void uniform(GLint id, int3* vals, GLsizei count) { glUniform3iv(id, count, &(vals->i1)); }
	void uniform(GLint id, int4* vals, GLsizei count) { glUniform4iv(id, count, &(vals->i1)); }
	void uniform(GLint id, mat3* vals, GLsizei count) { glUniformMatrix3fv(id, count, true, (&vals)[0][0]); }
	void uniform(GLint id, mat4* vals, GLsizei count) { glUniformMatrix4fv(id, count, true, (&vals)[0][0]); }
};

#endif