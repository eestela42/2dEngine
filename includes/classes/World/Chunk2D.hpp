#pragma once

#include <classes/World/AChunk.hpp>

class Chunk2D : public AChunk {
	private:
		/*the point that will be used by the graphic API to draw the chunk*/
		std::vector<Vertex_texture>	vertices;
		/*the indices that will be used by the graphic API to draw the chunk*/
		std::vector<unsigned int> 	indices;
		/*the map of the chunk, it will be used to generate the vertices and indices*/
		std::vector<u_char> map;

	public:
		Chunk2D(int x, int y, int z);
		~Chunk2D();

		void deleter() override;


		u_int VBO, EBO, VAO;

		void privGenerate(u_char *data) override;
		void privCompile() override;
		bool privChangeBlock(int x, int y, int z, u_char type) override;

		t_vbo_data 					privGetPtrVertices() override;
		std::vector<unsigned int>*	privGetPtrIndices() override;

		bool						privIsFilled(int x, int y, int z) override;
		u_char						privBlockType(int x, int y, int z) override;



		void	createFace(int x, int y, int z, int tab_x, int tab_y, int tab_z, int block_type, int dir);

};