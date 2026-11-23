#ifndef MESH_HPP
# define MESH_HPP

# include <iostream>
# include <string.h>
# include <sstream>
# include <fstream>
# include <math.h>
# include <chrono>
# include <iostream>
# include <string.h>
# include <sstream>
# include <fstream>
# include <cstddef>
# include <vector>
# include <map>


#include <interfaces/Interface_Input.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <classes/vertexData.hpp>



typedef struct s_triangle
{
	unsigned int v[3];
}				t_triangle;

struct mesh_material_range
{
	std::string materialName;
	std::string texturePath;
	std::size_t indexOffset = 0;
	std::size_t indexCount = 0;
};

class mesh
{
public:

	std::vector<glm::vec3> base_vertexes;
	std::vector<glm::vec2> base_texture_coordinates;
	std::vector<std::vector<unsigned int>> base_faces;
	std::vector<std::vector<int>> base_face_texture_coordinates;
	std::vector<std::string> base_face_materials;

	std::vector<glm::vec3> vertexes;
	std::vector<glm::vec2> textureCoordinates;
	std::vector<t_triangle> triangles;

	std::vector<std::vector<t_triangle>> base_triangles;
	std::vector<std::vector<unsigned int>> vertByFace;

	t_vertexData dataStruct = {nullptr, 0};
	std::vector<u_int> shapeAssemblyData;
	std::vector<float> texturedVertexData;
	std::string texturePath;
	std::map<std::string, std::string> materialTexturePaths;
	std::vector<mesh_material_range> materialRanges;
	bool hasTextureCoordinates = false;
	bool hasTexture = false;

	~mesh();
	mesh();
	mesh(const mesh &copy);
	mesh(std::string file_name);

	void	parseFromFile(std::string file_name);
	void	buildVertexData();


	std::vector<t_triangle> facesToTriangles();
	void	facesToTrianglesDupVertexes();
	void	addInfoToVertexesDup();




	void 					EcoDupVertex();
	t_triangle 				getSemioptiVerticesTriangle(t_triangle &triangle, std::size_t pos,  std::vector<glm::vec3> *new_vertexes,
												std::vector<t_triangle> *base_face_triangles, std::vector<t_triangle> *new_face_triangles);
	std::vector<t_triangle> createTriangleFromFace(std::vector<unsigned int> face);
	void 					addInfoToVertexesEco();


	void 	center_around_orgin();
	void	min_max_bounds(glm::vec3& min_bound, glm::vec3& max_bound);

	mesh&	operator=(const mesh &copy);
};



#endif