#include <classes/World/Chunk2D.hpp>


/*
Initialize a simple 2D chunk and allocate one block entry for every X/Y position.
The Z axis is represented by the height-one block and ground mesh rather than a 3D data array.
*/
Chunk2D::Chunk2D(int x, int y, int z) : AChunk(x, y, z)
{
	map.resize(AChunk::sizeX* AChunk::sizeY, 0);
}

/*
Destroy the 2D chunk. Its block map is owned by the vector and is released automatically.
*/
Chunk2D::~Chunk2D()
{
}

/*
Decrease the shared-holder count and delete the chunk only on the final reference.
This allows neighbour or instanciator cleanup to share ownership safely.
*/
void Chunk2D::deleter()
{
	if (this->sharedHolder == 1)
		delete this;
	else
		this->sharedHolder--;
}


/*
Fill the 2D block map with a simple random stone pattern.
The raw generated buffer is freed after the map has taken ownership of the block data.
THIS IS SUPPOSED TO TAKE A RAW DATA ARRAY FROM THE GENERATOR, BUT FOR THIS simple 2D IMPLEMENTATION WE IGNORE IT.
*/
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
	if (data)
		free(data);
}

/*
Build the GPU-ready vertex and index data for every block in the 2D chunk.
It also adds a separate ground quad below the block faces.
*/
void Chunk2D::privCompile()
{
	vertices.clear();
	indices.clear();
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

/*
Change one block in the 2D map and report whether its value changed.
*/
bool Chunk2D::privChangeBlock(int x, int y, int z, u_char type)
{
	if (x < 0 || x >= static_cast<int>(AChunk::sizeX)
		|| y < 0 || y >= static_cast<int>(AChunk::sizeY))
		return false;

	u_char &block = map[x + y * AChunk::sizeX];
	if (block == type)
		return false;
	block = type;
	return true;
}

/*
Return a pointer and byte size for the compiled vertex array.
The data is consumed by the VAO and must remain valid while the chunk exists.
*/
t_vbo_data 					Chunk2D::privGetPtrVertices()
{
	t_vbo_data data;
	data.data = vertices.data();
	data.size = vertices.size() * sizeof(Vertex_texture);
	return data;
}

/*
Return the chunk's compiled index array to the caller.
*/
std::vector<unsigned int>*	Chunk2D::privGetPtrIndices()
{
	return &indices;
}

/*
This simple implementation treats every position as filled.
It is kept in the interface so callers can use the same chunk API for 2D and 3D chunks.
*/
bool						Chunk2D::privIsFilled(int x, int y, int z)
{
	if (x < 0 || x >= static_cast<int>(AChunk::sizeX)
		|| y < 0 || y >= static_cast<int>(AChunk::sizeY))
		return false;
	return map[x + y * sizeX] != 0;
}

/*
Return the block type at the requested horizontal map position.
*/
u_char						Chunk2D::privBlockType(int x, int y, int z)
{
	if (x < 0 || x >= static_cast<int>(AChunk::sizeX)
		|| y < 0 || y >= static_cast<int>(AChunk::sizeY))
		return AIR;
	return map[x + y * AChunk::sizeX];
}



/*
Append one block face to the chunk mesh.
The dir value selects top, bottom, or one of the four side orientations,
and matching indices form the face's two triangles.
*/
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


			/*
			Exmple :
				here we add the  4 vertices (points) of the face to the vertices vector.
				The data we send is the one needed by the shader
			*/
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
	/*
	here we add the 6 indices of the two triangles that form the face to the indices vector.
	we use indices so we can re-use the same vertices for multiple triangles,
	which saves memory and improves performance. (a vertice = 3 floats, an index = 1 int)
	*/
	indices.push_back(start);
	indices.push_back(start + 1);
	indices.push_back(start + 2);
	indices.push_back(start + 3);
	indices.push_back(start + 2);
	indices.push_back(start);
}

