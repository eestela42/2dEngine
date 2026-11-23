#include <classes/Game/mesh.hpp>

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace
{
	std::string trim(const std::string &value)
	{
		const std::size_t first = value.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
			return "";
		const std::size_t last = value.find_last_not_of(" \t\r\n");
		return value.substr(first, last - first + 1);
	}

	int resolveObjIndex(const std::string &value, std::size_t count, std::size_t lineNumber,
		const char *kind, bool optional)
	{
		int index = 0;
		const auto parsed = std::from_chars(value.data(), value.data() + value.size(), index);
		const bool validIndex = parsed.ec == std::errc() && parsed.ptr == value.data() + value.size() && index != 0;
		if (!validIndex)
		{
			if (optional)
			{
				std::cerr << "Ignoring invalid OBJ " << kind << " index on line " << lineNumber << std::endl;
				return -1;
			}
			throw std::runtime_error("Invalid OBJ " + std::string(kind) + " index on line " + std::to_string(lineNumber));
		}

		const long long resolved = index > 0
			? static_cast<long long>(index) - 1
			: static_cast<long long>(count) + index;
		if (resolved < 0 || resolved >= static_cast<long long>(count))
		{
			if (optional)
			{
				std::cerr << "Ignoring out-of-range OBJ " << kind << " index on line " << lineNumber << std::endl;
				return -1;
			}
			throw std::runtime_error("OBJ " + std::string(kind) + " index out of range on line " + std::to_string(lineNumber));
		}
		return static_cast<int>(resolved);
	}
}


mesh::~mesh()
{}

mesh::mesh()
{}

mesh::mesh(const mesh &copy)
{
	*this = copy;
}

mesh::mesh(std::string file_name)
{
	std::cout << std::endl;

	std::cout << "--start extracting info from file" << std::endl;

	parseFromFile(file_name);

	std::cout << "--end extracting info from file" << std::endl;
	
	std::cout << std::endl;

	std::cout << "--start managing raw info from file" << std::endl;

	EcoDupVertex();
	center_around_orgin();

	std::cout << "--end managing raw info from file" << std::endl;

	std::cout << std::endl;

	std::cout << "base_vertexes : 	" << base_vertexes.size() << std::endl;
	std::cout << "base_faces : 		" << base_faces.size() << std::endl;
	std::cout << "vertexes : 		" << vertexes.size() << std::endl;
	std::cout << "triangles : 		" << triangles.size() << std::endl;

	std::cout << std::endl;

	for (std::size_t i = 0; i < triangles.size(); i++)
	{
		shapeAssemblyData.push_back(triangles[i].v[0]);
		shapeAssemblyData.push_back(triangles[i].v[1]);
		shapeAssemblyData.push_back(triangles[i].v[2]);
	}
	buildVertexData();
}

void	mesh::parseFromFile(std::string file_name)
{
	std::ifstream file(file_name);
	if (!file.is_open())
		throw std::runtime_error("Error opening OBJ file: " + file_name);

	std::vector<std::filesystem::path> materialLibraries;
	std::string line;
	std::string currentMaterial;
	std::size_t lineNumber = 0;
	while (std::getline(file, line))
	{
		++lineNumber;
		std::istringstream lineStream(line);
		std::string record;
		if (!(lineStream >> record) || record[0] == '#')
			continue;

		if (record == "v")
		{
			glm::vec3 vertex;
			if (!(lineStream >> vertex.x >> vertex.y >> vertex.z))
				throw std::runtime_error("Invalid OBJ vertex on line " + std::to_string(lineNumber));
			base_vertexes.push_back(vertex);
		}
		else if (record == "vt")
		{
			glm::vec2 coordinate;
			if (!(lineStream >> coordinate.x >> coordinate.y))
				throw std::runtime_error("Invalid OBJ texture coordinate on line " + std::to_string(lineNumber));
			base_texture_coordinates.push_back(coordinate);
		}
		else if (record == "f")
		{
			std::vector<unsigned int> face;
			std::vector<int> faceTextureCoordinates;
			std::string token;
			while (lineStream >> token)
			{
				if (token[0] == '#')
					break;

				const std::size_t firstSlash = token.find('/');
				const std::size_t secondSlash = firstSlash == std::string::npos
					? std::string::npos : token.find('/', firstSlash + 1);
				const std::string positionIndex = token.substr(0, firstSlash);
				const int resolvedPosition = resolveObjIndex(
					positionIndex, base_vertexes.size(), lineNumber, "vertex", false);
				face.push_back(static_cast<unsigned int>(resolvedPosition + 1));

				int textureIndex = -1;
				if (firstSlash != std::string::npos)
				{
					const std::size_t textureEnd = secondSlash == std::string::npos ? token.size() : secondSlash;
					const std::string textureValue = token.substr(firstSlash + 1, textureEnd - firstSlash - 1);
					if (!textureValue.empty() && !base_texture_coordinates.empty())
						textureIndex = resolveObjIndex(
							textureValue, base_texture_coordinates.size(), lineNumber, "texture", true);
				}
				faceTextureCoordinates.push_back(textureIndex);
			}

			if (face.size() >= 3)
			{
				base_faces.push_back(face);
				base_face_texture_coordinates.push_back(faceTextureCoordinates);
				base_face_materials.push_back(currentMaterial);
			}
			else
				std::cerr << "Ignoring OBJ face with fewer than three vertices on line " << lineNumber << std::endl;
		}
		else if (record == "usemtl")
		{
			std::getline(lineStream, currentMaterial);
			currentMaterial = trim(currentMaterial);
			const std::size_t commentPosition = currentMaterial.find(" #");
			if (commentPosition != std::string::npos)
				currentMaterial = trim(currentMaterial.substr(0, commentPosition));
		}
		else if (record == "mtllib")
		{
			std::string materialName;
			while (lineStream >> materialName && materialName[0] != '#')
				materialLibraries.push_back(std::filesystem::path(materialName));
		}
	}

	for (const std::vector<int> &faceCoordinates : base_face_texture_coordinates)
	{
		if (std::any_of(faceCoordinates.begin(), faceCoordinates.end(), [](int index) { return index >= 0; }))
		{
			hasTextureCoordinates = true;
			break;
		}
	}

	if (!hasTextureCoordinates)
		return;

	const std::filesystem::path objPath(file_name);
	for (const std::filesystem::path &libraryName : materialLibraries)
	{
		const std::filesystem::path materialPath = (objPath.parent_path() / libraryName).lexically_normal();
		std::ifstream materialFile(materialPath);
		if (!materialFile.is_open())
		{
			std::cerr << "Unable to open MTL file " << materialPath << "; mesh will remain untextured" << std::endl;
			continue;
		}

		std::string materialLine;
		std::string currentMaterialName;
		while (std::getline(materialFile, materialLine))
		{
			std::istringstream materialStream(materialLine);
			std::string record;
			if (!(materialStream >> record))
				continue;

			if (record == "newmtl")
			{
				std::getline(materialStream, currentMaterialName);
				currentMaterialName = trim(currentMaterialName);
				const std::size_t commentPosition = currentMaterialName.find(" #");
				if (commentPosition != std::string::npos)
					currentMaterialName = trim(currentMaterialName.substr(0, commentPosition));
				continue;
			}
			if (record != "map_Kd" || currentMaterialName.empty())
				continue;

			std::string textureName;
			std::getline(materialStream, textureName);
			textureName = trim(textureName);
			const std::size_t commentPosition = textureName.find(" #");
			if (commentPosition != std::string::npos)
				textureName = trim(textureName.substr(0, commentPosition));
			if (textureName.empty())
				continue;

			std::filesystem::path textureCandidate = materialPath.parent_path() / textureName;
			if (!std::filesystem::is_regular_file(textureCandidate))
			{
				std::istringstream optionStream(textureName);
				std::string option;
				while (optionStream >> option)
					textureCandidate = materialPath.parent_path() / option;
			}
			if (std::filesystem::is_regular_file(textureCandidate))
			{
				materialTexturePaths[currentMaterialName] = textureCandidate.lexically_normal().string();
				continue;
			}
			std::cerr << "Unable to open diffuse texture " << textureName << " for material "
				<< currentMaterialName << std::endl;
		}
	}

	for (const std::string &materialName : base_face_materials)
	{
		const auto texture = materialTexturePaths.find(materialName);
		if (texture != materialTexturePaths.end())
		{
			texturePath = texture->second;
			break;
		}
	}
	hasTexture = hasTextureCoordinates && !texturePath.empty();
}


std::vector<t_triangle> mesh::facesToTriangles()
{
	std::vector<t_triangle> new_triangles;
	for (std::vector<std::vector<unsigned int>>::iterator it = this->base_faces.begin(); it != this->base_faces.end(); ++it)
	{
		if (it->size() < 3)
		{
			std::cout << "Error : face with less than 3 vertexes" << std::endl;
			exit(1);
		}

		std::size_t i = 2;
		while (i < it->size())
		{
			t_triangle triangle;
			triangle.v[0] = it->at(0) - 1;
			triangle.v[1] = it->at(i - 1) - 1;
			triangle.v[2] = it->at(i) - 1;
			new_triangles.push_back(triangle);
			i++;
		}
	}
	return (new_triangles);
}

void mesh::facesToTrianglesDupVertexes()
{
	std::vector<glm::vec3> new_vertexes;
	std::vector<t_triangle> new_triangles;
	for (std::vector<std::vector<unsigned int>>::iterator it = this->base_faces.begin(); it != this->base_faces.end(); ++it)
	{
		if (it->size() < 3)
		{
			std::cout << "Error : face with less than 3 vertexes" << std::endl;
			exit(1);
		}

		std::size_t i = 2;
		while (i < it->size())
		{
			glm::vec3 vert[3];
			vert[0] = base_vertexes[it->at(0) - 1];
			vert[1] = base_vertexes[it->at(i - 1) - 1];
			vert[2] = base_vertexes[it->at(i) - 1];
			new_vertexes.push_back(vert[0]);
			new_vertexes.push_back(vert[1]);
			new_vertexes.push_back(vert[2]);
			i++;
		}
	}
	this->vertexes = new_vertexes;
	
	int i = 0;
	int max = this->vertexes.size();
	while (i < max)
	{
		t_triangle triangle;
		triangle.v[0] = i;
		triangle.v[1] = i + 1;
		triangle.v[2] = i + 2;
		new_triangles.push_back(triangle);
		i += 3;
	}
	
	this->triangles = new_triangles;
}

void mesh::addInfoToVertexesDup()
{
	std::vector<glm::vec3> new_vbo;
	glm::vec3 colors[] = {
		{1.0f, 0.0f, 0.0f}, // red
		{0.0f, 1.0f, 0.0f}, // green
		{0.0f, 0.0f, 1.0f}  // blue
	};
	glm::vec3 textureCoo[] = {
		{1.0f, 1.0f, 0.0f},
		{1.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f},
	};
	int colorIndex = 0;
	std::size_t face = 0;
	int last_face = -1;
	for (std::size_t i = 0; i < this->vertexes.size(); i++)
	{
		
		new_vbo.push_back(this->vertexes[i]);
		new_vbo.push_back(colors[colorIndex]);

		new_vbo.push_back(textureCoo[i % 2]);
		if (face < base_faces.size() && (i - last_face) == (base_faces[face].size() - 2) * 3)
		{
			// std::cout << "face : " << face << std::endl;
			// std::cout << "size face : " << base_faces[face].size() << std::endl;
			// std::cout << "i : " << i << std::endl;
			// std::cout << "last_face : " << last_face << std::endl << std::endl;
			face++;
			last_face = i;
			colorIndex = (colorIndex + 1) % 3;
		}
	}
	this->vertexes = new_vbo;
}


void mesh::EcoDupVertex()
{
	materialRanges.clear();
	if (!hasTexture)
	{
		std::vector<glm::vec3> new_vertexes;
		std::vector<t_triangle> new_triangles;
		vertByFace.clear();
		base_triangles.clear();

		for (const std::vector<unsigned int> &face : base_faces)
		{
			std::vector<t_triangle> baseFaceTriangles = createTriangleFromFace(face);
			std::vector<t_triangle> newFaceTriangles;
			for (std::size_t i = 0; i < baseFaceTriangles.size(); ++i)
			{
				newFaceTriangles.push_back(getSemioptiVerticesTriangle(
					baseFaceTriangles[i], i, &new_vertexes, &baseFaceTriangles, &newFaceTriangles));
			}

			std::vector<unsigned int> faceVertices;
			for (const t_triangle &triangle : newFaceTriangles)
			{
				new_triangles.push_back(triangle);
				for (int corner = 0; corner < 3; ++corner)
				{
					if (std::find(faceVertices.begin(), faceVertices.end(), triangle.v[corner]) == faceVertices.end())
						faceVertices.push_back(triangle.v[corner]);
				}
			}
			vertByFace.push_back(faceVertices);
			base_triangles.push_back(baseFaceTriangles);
		}

		triangles = new_triangles;
		vertexes = new_vertexes;
		textureCoordinates.clear();
		return;
	}

	std::vector<glm::vec3> new_vertexes;
	std::vector<glm::vec2> new_texture_coordinates;
	std::vector<t_triangle> new_triangles;
	vertByFace.clear();
	base_triangles.clear();

	std::vector<std::string> materialOrder;
	std::map<std::string, std::vector<std::size_t>> facesByMaterial;
	for (std::size_t faceIndex = 0; faceIndex < base_faces.size(); ++faceIndex)
	{
		const std::string materialName = faceIndex < base_face_materials.size()
			? base_face_materials[faceIndex] : "";
		if (facesByMaterial.find(materialName) == facesByMaterial.end())
			materialOrder.push_back(materialName);
		facesByMaterial[materialName].push_back(faceIndex);
	}

	for (const std::string &materialName : materialOrder)
	{
		mesh_material_range range;
		range.materialName = materialName;
		range.indexOffset = new_triangles.size() * 3;
		const auto texture = materialTexturePaths.find(materialName);
		if (texture != materialTexturePaths.end())
			range.texturePath = texture->second;

		for (std::size_t faceIndex : facesByMaterial[materialName])
		{
			const std::vector<unsigned int> &face = base_faces[faceIndex];
			const std::vector<int> &faceTextureCoordinates = base_face_texture_coordinates[faceIndex];
			if (face.size() < 3)
				continue;

			base_triangles.push_back(createTriangleFromFace(face));
			std::vector<unsigned int> faceVertices;
			for (std::size_t corner = 2; corner < face.size(); ++corner)
			{
				const std::size_t corners[] = {0, corner - 1, corner};
				t_triangle triangle;
				for (std::size_t triangleCorner = 0; triangleCorner < 3; ++triangleCorner)
				{
					const std::size_t faceCorner = corners[triangleCorner];
					triangle.v[triangleCorner] = static_cast<unsigned int>(new_vertexes.size());
					new_vertexes.push_back(base_vertexes[face[faceCorner] - 1]);
					faceVertices.push_back(triangle.v[triangleCorner]);
					const int textureIndex = faceCorner < faceTextureCoordinates.size()
						? faceTextureCoordinates[faceCorner] : -1;
					new_texture_coordinates.push_back(textureIndex >= 0
						? base_texture_coordinates[textureIndex] : glm::vec2(0.0f));
				}
				new_triangles.push_back(triangle);
			}
			vertByFace.push_back(faceVertices);
		}

		range.indexCount = new_triangles.size() * 3 - range.indexOffset;
		if (range.indexCount != 0)
			materialRanges.push_back(range);
	}

	this->triangles = new_triangles;
	this->vertexes = new_vertexes;
	this->textureCoordinates = new_texture_coordinates;
}

void mesh::buildVertexData()
{
	if (hasTexture)
	{
		texturedVertexData.clear();
		texturedVertexData.reserve(vertexes.size() * 5);
		for (std::size_t i = 0; i < vertexes.size(); ++i)
		{
			texturedVertexData.push_back(vertexes[i].x);
			texturedVertexData.push_back(vertexes[i].y);
			texturedVertexData.push_back(vertexes[i].z);
			texturedVertexData.push_back(textureCoordinates[i].x);
			texturedVertexData.push_back(textureCoordinates[i].y);
		}
		dataStruct.data = reinterpret_cast<u_char *>(texturedVertexData.data());
		dataStruct.size = texturedVertexData.size() * sizeof(float);
	}
	else
	{
		texturedVertexData.clear();
		dataStruct.data = reinterpret_cast<u_char *>(vertexes.data());
		dataStruct.size = vertexes.size() * sizeof(glm::vec3);
	}
}

t_triangle mesh::getSemioptiVerticesTriangle(t_triangle &triangle, std::size_t pos, std::vector<glm::vec3> *new_vertexes,
						std::vector<t_triangle> *base_face_triangles, std::vector<t_triangle> *new_face_triangles)
{
	t_triangle ret;
	bool found = false;
	for (int i = 0; i < 3; i++)
	{
		found = false;

		for (std::size_t y = 0; !found && y < pos && y < base_face_triangles->size(); y++)
		{
			for (int z = 0; !found && z < 3; z++)
			{
				if (base_face_triangles->at(y).v[z] == triangle.v[i])
					{
						ret.v[i] = new_face_triangles->at(y).v[z];
						found = true;
					}
			}
		}
		if (!found)
		{
			ret.v[i] = new_vertexes->size();
			new_vertexes->push_back(this->base_vertexes[triangle.v[i]]);
		}
	}
	return (ret);
}


glm::vec3 sub_vec(glm::vec3& a, glm::vec3& b);

std::vector<t_triangle> mesh::createTriangleFromFace(std::vector<unsigned int> face)
{
	std::vector<t_triangle> new_triangles;
	std::size_t i = 2;
	while (i < face.size())
	{
		t_triangle triangle;
		triangle.v[0] = face[0] - 1;
		triangle.v[1] = face[i - 1] - 1;
		triangle.v[2] = face[i] - 1;
		new_triangles.push_back(triangle);
		i++;
	}
	return (new_triangles);
}

void mesh::addInfoToVertexesEco()
{
	std::vector<glm::vec3> new_vbo;
	glm::vec3 colors[] = {
		{0.1f, 0.1f, 0.1f}, // red
		{0.2f, 0.2f, 0.2f}, // green
		{0.3f, 0.3f, 0.3f}  // blue
	};
	glm::vec3 textureCoo[] = {
		{1.0f, 1.0f, 1.0f},
		{1.0f, 0.0f, 1.0f},
		{0.0f, 0.0f, 1.0f},
		{0.0f, 1.0f, 1.0f}
	};


	int pos = 0;
	
	for (std::size_t face = 0; face < this->vertByFace.size(); face++)
	{
		for (std::size_t vertInFace = 0; vertInFace < this->vertByFace[face].size(); vertInFace++)
		{
			// std::cout << "face : " << face << " | vertInFace : " << vertInFace << std::endl;
			new_vbo.push_back(this->vertexes[this->vertByFace[face][vertInFace]]);
			//new_vbo.push_back(colors[face % 3]);
			float clor = (face % 50) * 0.01 ;
			glm::vec3 tmp = { clor, clor,clor};
			// glm::vec3 tmp = colors[face % 3];
			new_vbo.push_back(tmp);
			new_vbo.push_back(textureCoo[vertInFace % 4]);
			pos++;
		}
	}


	this->vertexes = new_vbo;
}

void mesh::min_max_bounds(glm::vec3& min_bound, glm::vec3& max_bound)
{
	for (std::vector<glm::vec3>::iterator vert = this->vertexes.begin(); vert != this->vertexes.end();)  {
		min_bound[0] = std::min(min_bound[0], (*vert)[0]);
		min_bound[1] = std::min(min_bound[1], (*vert)[1]);
		min_bound[2] = std::min(min_bound[2], (*vert)[2]);

		max_bound[0] = std::max(max_bound[0], (*vert)[0]);
		max_bound[1] = std::max(max_bound[1], (*vert)[1]);
		max_bound[2] = std::max(max_bound[2], (*vert)[2]);
		vert++;
	}
}

void	mesh::center_around_orgin()
{
	glm::vec3 min_bound = {0, 0, 0};
	glm::vec3 max_bound = {0, 0, 0};

	min_max_bounds(min_bound, max_bound);

	// std::cout << "min : " << min_bound.v[0] << " " << min_bound.v[1] << " " << min_bound.v[2] << std::endl;
	// std::cout << "max : " << max_bound.v[0] << " " << max_bound.v[1] << " " << max_bound.v[2] << std::endl;
	glm::vec3 center = {0, 0, 0};
	
	center[0] = (min_bound[0] + max_bound[0]) / 2;
	center[1] = (min_bound[1] + max_bound[1]) / 2;
	center[2] = (min_bound[2] + max_bound[2]) / 2;
	// std::cout << "center : " << center.v[0] << " " << center.v[1] << " " << center.v[2] << std::endl;

	for (std::vector<glm::vec3>::iterator vert = this->vertexes.begin(); vert != this->vertexes.end(); ++vert)  {
		(*vert)[0] -= center[0];
		(*vert)[1] -= center[1];
		(*vert)[2] -= center[2];
	}
}


mesh &mesh::operator=(const mesh &copy)
{
	this->base_vertexes = copy.base_vertexes;
	this->base_texture_coordinates = copy.base_texture_coordinates;
	this->base_faces = copy.base_faces;
	this->base_face_texture_coordinates = copy.base_face_texture_coordinates;
	this->base_face_materials = copy.base_face_materials;
	this->vertexes = copy.vertexes;
	this->textureCoordinates = copy.textureCoordinates;
	this->triangles = copy.triangles;
	this->base_triangles = copy.base_triangles;
	this->vertByFace = copy.vertByFace;
	this->shapeAssemblyData = copy.shapeAssemblyData;
	this->texturePath = copy.texturePath;
	this->materialTexturePaths = copy.materialTexturePaths;
	this->materialRanges = copy.materialRanges;
	this->hasTextureCoordinates = copy.hasTextureCoordinates;
	this->hasTexture = copy.hasTexture;
	buildVertexData();
	return (*this);
}