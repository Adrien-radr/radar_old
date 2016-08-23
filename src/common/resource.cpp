#include "resource.h"
#include "json/cJSON.h"

#include <fstream>

bool Resource::CheckExtension(const std::string &file_path, const std::string &ext) {
    size_t pos = file_path.find_last_of('.');
	if (pos != std::string::npos) {
		return file_path.substr(pos + 1).find(ext) != std::string::npos;
	}
    return false;
}

bool Resource::ReadFile(std::string &buf, const std::string &file_path) {
	std::ifstream ifile(file_path);
	if (ifile) {
		ifile.seekg(0, std::ios::end);
		buf.resize(ifile.tellg());
		ifile.seekg(0, std::ios::beg);
		ifile.read(&buf[0], buf.size());
		ifile.close();
		return true;
	}
	return false;
}

bool Json::Open(const std::string &file_path) {
	if (opened) Close();

	if (Resource::ReadFile(contents, file_path)) {
		root = cJSON_Parse(contents.c_str());

		if (!root) {
			LogErr("JSON parse error [", file_path, "] before : \n", cJSON_GetErrorPtr());
			contents.clear();

			return false;
		}

		opened = true;
		return true;
	}
	return false;
}

void Json::Close() {
	if (opened) {
		opened = false;
		if (root) cJSON_Delete(root);
		root = NULL;
		contents.clear();
	}
}

int Json::ReadInt(cJSON *parent, const std::string &name, int default_val) {
	cJSON *item = cJSON_GetObjectItem(parent, name.c_str());
	if (item) return item->valueint;
	return default_val;
}

f32 Json::ReadFloat(cJSON *parent, const std::string &name, f32 default_val) {
	cJSON *item = cJSON_GetObjectItem(parent, name.c_str());
	if (item) return (f32)item->valuedouble;
	return default_val;
}

std::string Json::ReadString(cJSON *parent, const std::string &name,
	const std::string &default_val) {
	cJSON *item = cJSON_GetObjectItem(parent, name.c_str());
	if (item) return std::string(item->valuestring);
	return std::string(default_val);

}

vec3f Json::ReadVec3(cJSON *parent, const std::string &name, const vec3f &default_val) {
	cJSON *item = cJSON_GetObjectItem(parent, name.c_str());
	if(!item) return default_val;

	// should be a 3-size array
	u32 arr_size = cJSON_GetArraySize(item);
	if(arr_size != 3) {
		LogErr("JSON parse error, ", name, " is not a vec3.");
		return default_val;
	}

	return vec3f(	(f32) cJSON_GetArrayItem(item, 0)->valuedouble, 
					(f32) cJSON_GetArrayItem(item, 1)->valuedouble, 
					(f32) cJSON_GetArrayItem(item, 2)->valuedouble);
}
