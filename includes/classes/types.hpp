#ifndef TYPES_HPP
#define TYPES_HPP

#include <glm/glm.hpp>
#include <vector>

struct Vertex
{
    glm::vec3   Position;
	int 		light;
};

struct Vertex_texture
{
    glm::vec3   Position;
	int 		light;
	glm::vec2	texCoord;
	int 		blockType;
};

struct t_vbo_data
{
	void *data = NULL;
	u_int size = 0;
};

struct info_VAO
{
	std::vector<unsigned char>	vertices;
	std::vector<unsigned int>	indices;
	glm::ivec2 					pos;
};

struct vao_operation
{
	enum class Type
	{
		Add,
		Remove
	};

	Type		type;
	info_VAO	*info;
	glm::ivec2	pos;
};

struct VAO_data
{
	u_int 						VAO;
};


#endif