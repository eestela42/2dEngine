#include <classes/World/Chunk2D.hpp>


Chunk2D::Chunk2D(int x, int y, int z) : AChunk(x, y, z)
{
	map.resize(AChunk::sizeX* AChunk::sizeY, 0);
}

Chunk2D::~Chunk2D()
{
	// delete data if needed
}

void Chunk2D::deleter()
{
	if (this->sharedHolder == 1)
		delete this;
	else
		this->sharedHolder--;
}


void Chunk2D::privGenerate(u_char *data)
{
	for (int x = 0; x < AChunk::sizeX; x++)
	{
		for (int y = 0; y < AChunk::sizeY; y++)
		{
			if (rand() % 100 < 20)
				map[x + y * AChunk::sizeX] = STONE;
		}
	}
	this->setData(map.data());
}

void Chunk2D::privCompile()
{
	u_char *blocks = getData();
	glm::ivec2 pos = getPos();
	int offset_x = pos.x * AChunk::sizeX;
	int offset_y = pos.y * AChunk::sizeY;
	int x = 1;
	int y = 1;
	int z = 1;
	for (int x = 0; x < sizeX; x++)	{
	for (int y = 0; y < sizeY; y++)	{
				int block_type = blocks[x + y * sizeX];
				if (block_type == 0)
					continue;
				createFace(offset_x + x, offset_y + y, 0, x, y, z, block_type, 0);
				createFace(offset_x + x, offset_y + y, 0, x, y, z, block_type, 1);
				createFace(offset_x + x, offset_y + y, 0, x, y, z, block_type, 2);
				createFace(offset_x + x, offset_y + y, 0, x, y, z, block_type, 3);
				createFace(offset_x + x, offset_y + y, 0, x, y, z, block_type, 4);
				createFace(offset_x + x, offset_y + y, 0, x, y, z, block_type, 5);
	}
	}

	const float groundZ = 0.f;
	const glm::vec2 uvBottomLeft(0.0f, 0.0f);
	const glm::vec2 uvBottomRight(16.0f, 0.0f);
	const glm::vec2 uvTopRight(16.0f, 16.0f);
	const glm::vec2 uvTopLeft(0.0f, 16.f);


	int groundStart = vertices.size();
	vertices.push_back({glm::vec3(offset_x, offset_y, groundZ), 1,
						uvBottomLeft, 1});
	vertices.push_back({glm::vec3(offset_x + float(AChunk::sizeX), offset_y, groundZ), 1,
						uvBottomRight, 1});
	vertices.push_back({glm::vec3(offset_x + float(AChunk::sizeX), offset_y + float(AChunk::sizeY), groundZ), 1,
						uvTopRight, 1});
	vertices.push_back({glm::vec3(offset_x, offset_y + float(AChunk::sizeY), groundZ), 1,
						uvTopLeft, 1});
	indices.push_back(groundStart);
	indices.push_back(groundStart + 1);
	indices.push_back(groundStart + 2);
	indices.push_back(groundStart + 3);
	indices.push_back(groundStart + 2);
	indices.push_back(groundStart);

}

bool Chunk2D::privChangeBlock(int x, int y, int z, u_char type)
{
	return false;
}

t_vbo_data 					Chunk2D::privGetPtrVertices()
{
	t_vbo_data data;
	data.data = vertices.data();
	data.size = vertices.size() * sizeof(Vertex_texture);
	return data;
}
std::vector<unsigned int>*	Chunk2D::privGetPtrIndices()
{
	return &indices;
}

bool						Chunk2D::privIsFilled(int x, int y, int z)
{
	return true;
}
u_char						Chunk2D::privBlockType(int x, int y, int z)
{
	return 1;
}



void	Chunk2D::createFace(int x, int y, int z,  int tab_x, int tab_y, int tab_z, int block_type, int dir)
{
	int start = vertices.size();
	const glm::vec2 uvBottomLeft(0.0f, 0.0f);
	const glm::vec2 uvBottomRight(1.f, 0.0f);
	const glm::vec2 uvTopRight(1.f, 1.f);
	const glm::vec2 uvTopLeft(0.0f, 1.f);
	switch (dir)
	{
		case 5 :
			// if (tab_z > 0 && getData()[tab_z - 1 + tab_y * sizeZ + tab_x * sizeY * sizeZ] != 0)
			// 	return ;
			vertices.push_back({glm::vec3(x, y, z), 0, uvBottomLeft, block_type});
			vertices.push_back({glm::vec3(x + 1.0f, y, z), 0, uvBottomRight, block_type});
			vertices.push_back({glm::vec3(x + 1.0f, y + 1.0f, z), 0, uvTopRight, block_type});
			vertices.push_back({glm::vec3(x, y + 1.0f, z), 0, uvTopLeft, block_type});
			break;
		case 4 :
			// if (tab_z < sizeZ - 1  && getData()[tab_z + 1 + tab_y * sizeZ + tab_x * sizeY * sizeZ] != 0)
			// 	return ;
			vertices.push_back({glm::vec3(x, y, z + 1.0f), 1, uvBottomLeft, block_type});
			vertices.push_back({glm::vec3(x + 1.0f, y, z + 1.0f), 1, uvBottomRight, block_type});
			vertices.push_back({glm::vec3(x + 1.0f, y + 1.0f, z + 1.0f), 1, uvTopRight, block_type});
			vertices.push_back({glm::vec3(x, y + 1.0f, z + 1.0f), 1, uvTopLeft, block_type});
			break;
		case 2 :
			// if (tab_x < sizeX - 1 && getData()[tab_z + tab_y * sizeZ + (tab_x + 1) * sizeY * sizeZ] != 0)
			// 	return ;
			vertices.push_back({glm::vec3(x + 1.0f, y, z), 0, uvBottomLeft, block_type});
			vertices.push_back({glm::vec3(x + 1.0f, y, z + 1.0f), 0, uvBottomRight, block_type});
			vertices.push_back({glm::vec3(x + 1.0f, y + 1.0f, z + 1.0f), 0, uvTopRight, block_type});
			vertices.push_back({glm::vec3(x + 1.0f, y + 1.0f, z), 0, uvTopLeft, block_type});
			break;
		case 3 :
			// if (tab_x > 0 && getData()[tab_z + tab_y * sizeZ + (tab_x - 1) * sizeY * sizeZ] != 0)
			// 	return ;
			vertices.push_back({glm::vec3(x, y, z), 1, uvBottomLeft, block_type});
			vertices.push_back({glm::vec3(x, y, z + 1.0f), 1, uvBottomRight, block_type});
			vertices.push_back({glm::vec3(x, y + 1.0f, z + 1.0f), 1, uvTopRight, block_type});
			vertices.push_back({glm::vec3(x, y + 1.0f, z), 1, uvTopLeft, block_type});
			break;
		case 0 :
			// if (tab_y < sizeY - 1 && getData()[tab_z + (tab_y + 1) * sizeZ + tab_x * sizeY * sizeZ] != 0)
			// 		return ;
			
			vertices.push_back({glm::vec3(x, y + 1.0f, z), 0, uvBottomLeft, block_type});
			vertices.push_back({glm::vec3(x + 1.0f, y + 1.0f, z), 0, uvBottomRight, block_type});
			vertices.push_back({glm::vec3(x + 1.0f, y + 1.0f, z + 1.0f), 0, uvTopRight, block_type});
			vertices.push_back({glm::vec3(x, y + 1.0f, z + 1.0f), 0, uvTopLeft, block_type});
			break;
		case 1 :
			// if (tab_y > 0 && getData()[tab_z + (tab_y - 1) * sizeZ + tab_x * sizeY * sizeZ] != 0)
			// 	return ;
			
			vertices.push_back({glm::vec3(x, y, z), 1, uvBottomLeft, block_type});
			vertices.push_back({glm::vec3(x + 1.0f, y, z), 1, uvBottomRight, block_type});
			vertices.push_back({glm::vec3(x + 1.0f, y, z + 1.0f), 1, uvTopRight, block_type});
			vertices.push_back({glm::vec3(x, y, z + 1.0f), 1, uvTopLeft, block_type});
			break;
	}
	
	indices.push_back(start);
	indices.push_back(start + 1);
	indices.push_back(start + 2);
	indices.push_back(start + 3);
	indices.push_back(start + 2);
	indices.push_back(start);
}

