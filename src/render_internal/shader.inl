

namespace UBO {
	Handle Build(const Desc &desc) {
		Data ubo;

		if(desc.size > 0 && (desc.sType == ST_DYNAMIC || (desc.sType == ST_STATIC && desc.data))) {
			GLint last_ubo = renderer->curr_GL_ubo;

			glGenBuffers(1, &ubo.id);
			glBindBuffer(GL_UNIFORM_BUFFER, ubo.id);
			glBufferData(GL_UNIFORM_BUFFER, desc.size, desc.data, desc.sType == ST_STATIC ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
			glBindBuffer(GL_UNIFORM_BUFFER, last_ubo);

			int ubo_i = (int)renderer->ubos.size();
			renderer->ubos.push_back(ubo);

			return ubo_i;
		} else {
			return -1;
		}
	}

	void Update(Handle h, const Desc &desc) {
		if(Exists(h) && desc.sType == ST_DYNAMIC) {
			GLint last_ubo = renderer->curr_GL_ubo;
			Data &ubo = renderer->ubos[h];
			glBindBuffer(GL_UNIFORM_BUFFER, ubo.id);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, desc.size, desc.data);
			glBindBuffer(GL_UNIFORM_BUFFER, last_ubo);
		}
	}

	void Destroy(Handle h) {
		if(Exists(h)) {
			Data &ubo = renderer->ubos[h];
			glDeleteBuffers(1, &ubo.id);
			ubo.id = 0;
		}
	}

	bool Exists(Handle h) {
		return h >= 0 && h< (int)renderer->ubos.size() && renderer->ubos[h].id > 0;
	}

	void Bind(Shader::UniformBlock loc, Handle h) {
		GLint ubo = (h >= 0 && h < (int)renderer->ubos.size()) ? h : -1;

		if(renderer->curr_GL_ubo != ubo) {
			renderer->curr_GL_ubo = ubo;
			glBindBufferBase(GL_UNIFORM_BUFFER, loc, ubo >= 0 ? renderer->ubos[ubo].id : 0);
		}
	}
}

namespace Shader {
	static GLuint build_new_shader(const char *src, GLenum type) {
		GLuint shader = glCreateShader(type);

		glShaderSource(shader, 1, &src, NULL);
		glCompileShader(shader);

		GLint status;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

		if (!status) {
			GLint len;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

			GLchar *log = (GLchar*)malloc(len);
			glGetShaderInfoLog(shader, len, NULL, log);

			LogErr("\nShader compilation error :\n"
				"------------------------------------------\n",
				log,
				"------------------------------------------\n");

			free(log);
			shader = 0;
		}
		return shader;
	}

	Handle Build(const Desc &desc) {
		std::string v_src, f_src;
		GLuint v_shader = 0, f_shader = 0;
		bool from_file = false;

		// find out if building from source or from file
		// if given both(which makes no sense, dont do that), load from file
		if (!desc.vertex_file.empty() && !desc.fragment_file.empty()) {
			if (!Resource::ReadFile(v_src, desc.vertex_file)) {
				LogErr("Failed to read Vertex Shader file ", desc.vertex_file, ".");
				return -1;
			}

			if (!Resource::ReadFile(f_src, desc.fragment_file)) {
				LogErr("Failed to read Vertex Shader file ", desc.fragment_file, ".");
				return -1;
			}
			from_file = true;
		}
		else if (!desc.vertex_src.empty() && !desc.fragment_src.empty()) {
			v_src = desc.vertex_src;
			f_src = desc.fragment_src;
		}
		else {
			LogErr("Call shader_build with desc->vertex_file & desc->fragment_file OR "
				"with desc->vertex_src & desc->fragment_src. None were given.");
			return -1;
		}

		Shader::Data shader;

		shader.id = glCreateProgram();

		v_shader = build_new_shader(v_src.c_str(), GL_VERTEX_SHADER);
		if (!v_shader) {
			if(from_file)
				LogErr("Failed to build '", desc.vertex_file.c_str(), "' Vertex shader.");
			else
				LogErr("Failed to build Vertex shader from source.");
			glDeleteProgram(shader.id);
			return -1;
		}

		f_shader = build_new_shader(f_src.c_str(), GL_FRAGMENT_SHADER);
		if (!f_shader) {
			if(from_file)
				LogErr("Failed to build '", desc.fragment_file.c_str(), "' Fragment shader.");
			else
				LogErr("Failed to build Fragment shader from source.");
			glDeleteShader(v_shader);
			glDeleteProgram(shader.id);
			return -1;
		}

		glAttachShader(shader.id, v_shader);
		glAttachShader(shader.id, f_shader);

		glDeleteShader(v_shader);
		glDeleteShader(f_shader);

		// Set the Attribute Locations
		for (u32 i = 0; desc.attribs[i].used && i < SHADER_MAX_ATTRIBUTES; ++i) {
			glBindAttribLocation(shader.id, desc.attribs[i].location,
									desc.attribs[i].name.c_str());
		}


		glLinkProgram(shader.id);

		GLint status;
		glGetProgramiv(shader.id, GL_LINK_STATUS, &status);

		if (!status) {
			GLint len;
			glGetProgramiv(shader.id, GL_INFO_LOG_LENGTH, &len);

			GLchar *log = (GLchar*)malloc(len);
			glGetProgramInfoLog(shader.id, len, NULL, log);

			LogErr("Shader Program link error : \n"
				"-----------------------------------------------------\n",
				log,
				"-----------------------------------------------------");

			free(log);
			glDeleteProgram(shader.id);
			return -1;
		}

		// Find out Uniform Locations
		memset(shader.uniform_locations, 0, sizeof(int)*UNIFORM_N);
		memset(shader.uniformblock_locations, 0, sizeof(int)*UNIFORMBLOCK_N);
		for (u32 i = 0; i < desc.uniforms.size(); ++i) {
			GLint loc = glGetUniformLocation(shader.id, desc.uniforms[i].name.c_str());
			if (loc < 0) {
				LogErr("While parsing Shader ", desc.vertex_file, ", Uniform variable '",
					desc.uniforms[i].name, "' gave no location!");
			}
			else {
				// get the uniform location descriptor from the name
				shader.uniform_locations[desc.uniforms[i].desc] = loc;
			}
		}

		for(u32 i = 0; i < desc.uniformblocks.size(); ++i) {
			GLuint loc = glGetUniformBlockIndex(shader.id, desc.uniformblocks[i].name.c_str());
			if(loc == GL_INVALID_INDEX) {
				LogErr("While parsing Shader ", desc.vertex_file, ", UniformBlock variable '",
					desc.uniformblocks[i].name, "' gave no location!");
			} else {
				shader.uniformblock_locations[desc.uniformblocks[i].desc] = loc;
				glUniformBlockBinding(shader.id, loc, desc.uniformblocks[i].desc);
			}
		}

		// Shader successfully loaded. Increment global counter if no slot is given
		int slot;
		int nShaders = (int)renderer->shaders.size();

		if(desc.shaderSlot >= 0) {
			if(desc.shaderSlot < nShaders) {
				slot = desc.shaderSlot;
				renderer->shaders[slot] = shader;
			}
		} else {
			slot = nShaders;
			renderer->shaders.push_back(shader);
		}

		return slot;
	}

	void Destroy(Handle h) {
		if (Exists(h)) {
			glDeleteProgram(renderer->shaders[h].id);
			renderer->shaders[h].id = 0;
		}
	}

	void Bind(Handle h) {
		GLint program = (h >= 0 && h < (int)renderer->shaders.size()) ? h : -1;

		if (renderer->curr_GL_program != program) {
			renderer->curr_GL_program = program;
			glUseProgram(program >= 0 ? (int)renderer->shaders[program].id : -1);
		}
	}

	void SendVec2(Uniform target, vec2f value) {
		glUniform2fv(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
			1, (GLfloat const *)value);
	}

	void SendVec3(Uniform target, vec3f value) {
		glUniform3fv(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
			1, (GLfloat const *)value);
	}

	void SendVec4(Uniform target, vec4f value) {
		glUniform4fv(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
			1, (GLfloat const *)value);
	}

	//void shader_send_mat3(Uniform target, mat3f value) {
		//glUniformMatrix3fv(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
			//1, GL_FALSE, (GLfloat const *)value);
	//}

	void SendMat4(Uniform target, mat4f value) {
		glUniformMatrix4fv(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
			1, GL_FALSE, (GLfloat const *)value);
	}

	void SendInt(Uniform target, int value) {
		glUniform1i(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
			value);
	}

	void SendFloat(Uniform target, f32 value) {
		glUniform1f(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
			value);
	}

	void SendVec2(const std::string &var_name, vec2f value) {
		glUniform2fv(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
			var_name.c_str()), 1, (GLfloat const *)value);
	}

	void SendVec3(const std::string &var_name, vec3f value) {
		glUniform3fv(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
			var_name.c_str()), 1, (GLfloat const *)value);
	}

	void SendVec4(const std::string &var_name, vec4f value) {
		glUniform4fv(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
			var_name.c_str()), 1, (GLfloat const *)value);
	}

	//void SendMat3(const std::string &var_name, mat3f value) {
		//glUniformMatrix3fv(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
			//var_name), 1, GL_FALSE, (GLfloat const *)value);
	//}

	void SendMat4(const std::string &var_name, mat4f value) {
		glUniformMatrix4fv(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
			var_name.c_str()), 1, GL_FALSE, (GLfloat const *)value);
	}

	void SendInt(const std::string &var_name, int value) {
		glUniform1i(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
			var_name.c_str()), value);
	}

	void SendFloat(const std::string &var_name, f32 value) {
		glUniform1f(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
			var_name.c_str()), value);
	}

	bool Exists(Handle h) {
		return h >= 0 && h< (int)renderer->shaders.size() && renderer->shaders[h].id > 0;
	}
}