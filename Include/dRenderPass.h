// dRenderPass.h 

#ifndef DRENDERPASS_HDR
#define DRENDERPASS_HDR

#include <vector>
#include <unordered_map>
#include <string>
#include <stdexcept>
#include "glad.h"
#include "VecMat.h"

using std::vector;
using std::unordered_map;
using std::string;
using std::runtime_error;

enum dUniformType { UTint, UTfloat, UTvec2, UTvec3, UTvec4, UTint2, UTint3, UTint4, UTmat3, UTmat4 };

struct dUniform {
	dUniformType type;
	int _int;
	float _float;
	vec2 _vec2;
	vec3 _vec3;
	vec4 _vec4;
	int2 _int2;
	int3 _int3;
	int4 _int4;
	mat3 _mat3;
	mat4 _mat4;
};

struct dRenderPass {
	GLuint program = 0;
	unordered_map<string, dUniform> uniforms;
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
	GLint getId(string key) { };
	void set(string key, int val) { 
		dUniform u;
		u.type = dUniformType::UTint;
		u._int = val;
		uniforms[key] = u;
	};
	void set(string key, float val) { 
		dUniform u;
		u.type = dUniformType::UTfloat;
		u._float = val;
		uniforms[key] = u;
	};
	void set(string key, vec2 val) { 
		dUniform u;
		u.type = dUniformType::UTvec2;
		u._vec2 = val;
		uniforms[key] = u;
	};
	void set(string key, vec3 val) { 
		dUniform u;
		u.type = dUniformType::UTvec3;
		u._vec3 = val;
		uniforms[key] = u;
	};
	void set(string key, vec4 val) { 
		dUniform u;
		u.type = dUniformType::UTvec4;
		u._vec4 = val;
		uniforms[key] = u;
	};
	void set(string key, int2 val) { 
		dUniform u;
		u.type = dUniformType::UTint2;
		u._int2 = val;
		uniforms[key] = u;
	};
	void set(string key, int3 val) { 
		dUniform u;
		u.type = dUniformType::UTint3;
		u._int3 = val;
		uniforms[key] = u;
	};
	void set(string key, int4 val) { 
		dUniform u;
		u.type = dUniformType::UTint4;
		u._int4 = val;
		uniforms[key] = u;
	};
	void set(string key, mat3 val) { 
		dUniform u;
		u.type = dUniformType::UTmat3;
		u._mat3 = val;
		uniforms[key] = u;
	};
	void set(string key, mat4 val) { 
		dUniform u;
		u.type = dUniformType::UTmat4;
		u._mat4 = val;
		uniforms[key] = u;
	};
	void clear(string key) { uniforms.erase(key); };
	void empty() { uniforms.erase(uniforms.begin(), uniforms.end()); }
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