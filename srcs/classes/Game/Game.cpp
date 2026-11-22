#include <classes/Game/Game.hpp>
#include <classes/Game/mesh.hpp>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include "../../../imgui/imgui.h"
#include "../../../imgui/backends/imgui_impl_glfw.h"
#include "../../../imgui/backends/imgui_impl_opengl3.h"

// namespace
// {
// 	class SlowBlockTimer
// 	{
// 	public:
// 		explicit SlowBlockTimer(const char *name, std::size_t payloadBytes = 0)
// 			: name(name), payloadBytes(payloadBytes), start(std::chrono::steady_clock::now())
// 		{
// 		}

// 		~SlowBlockTimer()
// 		{
// 			const auto elapsed = std::chrono::steady_clock::now() - start;
// 			if (elapsed >= std::chrono::milliseconds(50))
// 			{
// 				const double milliseconds =
// 					std::chrono::duration<double, std::milli>(elapsed).count();
// 				std::cerr << "[slow] " << name << ": " << milliseconds << " ms";
// 				if (payloadBytes != 0)
// 					std::cerr << " (" << payloadBytes << " bytes)";
// 				std::cerr << '\n';
// 			}
// 		}

// 	private:
// 		const char *name;
// 		std::size_t payloadBytes;
// 		std::chrono::steady_clock::time_point start;
// 	};
// }

unsigned int depthMapFBO;
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

unsigned int depthMap;


Game::Game()
{
	window = new Window("Minecraft", DrawMode::FILL);
	inputHandler = new InputHandler(window->GetWindow());
	camera = new Camera(window);
	player = new Player(window, *camera);

	/*
	Register the player and camera as input receivers, keeping their input
	handling separate from the game loop and rendering responsibilities.
	*/
	inputHandler->AddCallback(player);
	inputHandler->AddCallback(camera);
	inputHandler->AddCallback((I_Input*)window);
	shaderHandler = new ShaderHandler("shaders");
	

	/*
	A VAO stores the informations of triangles to be drawn
	(vertices (= points), indices to use per triangle, textures positions, etc).
	in a VAO there is a Vertex Buffer Object (VBO) that stores the vertices and an Element Buffer Object (EBO) that stores the indices.
	*/
	vertexArrayObjectHandler = new VertexArrayObjectHandler();

	blockTextureArray = TextureLoader::LoadTextureArray(
	{
		std::filesystem::path("textures/texturePack/dirt.jpg"),
		std::filesystem::path("textures/texturePack/dirt.jpg"),
		std::filesystem::path("textures/texturePack/grass.jpg"),
		std::filesystem::path("textures/texturePack/stone.jpg"),
		std::filesystem::path("textures/texturePack/sand.jpg"),
		std::filesystem::path("textures/texturePack/oak_wood_side.jpg"),
		std::filesystem::path("textures/texturePack/leaves_2.jpg"),
		std::filesystem::path("textures/texturePack/bedrock.jpg"),
		std::filesystem::path("textures/texturePack/water.jpg"),
		std::filesystem::path("textures/texturePack/snow.jpg"),
		std::filesystem::path("textures/texturePack/iron_ore.jpg"),
		std::filesystem::path("textures/texturePack/gold_ore.jpg"),
		std::filesystem::path("textures/texturePack/diamond_ore.jpg"),
		std::filesystem::path("textures/texturePack/UNKNOWN.jpg"),
	});

	/*
	The skybox is a cube that surrounds the entire scene and is used to create the illusion of a distant environment.
	It is rendered first, before any other objects in the scene,
	and is typically drawn with a special shader that uses a cubemap texture to create the illusion of a distant environment.
	*/
	skyBox = new SkyBox(shaderHandler->GetShader("cubemap"));

	/*
	The chunk instanciator runs on a separate thread.
	It receives shared state and synchronization objects so it can update chunks
	without directly modifying OpenGL objects from the worker thread.
	*/
	chunkInstanciator = new ChunkInstanciator(renderDistance, player->GetPosition(), player->GetPositionMutex(),
													vaoOperations, vaoOperations_mutex,
													endThreads, endThreads_mutex,
													casse_block,
													displayDistance);


	player->SetupChunks(chunkInstanciator->getTabChunks(), chunkInstanciator->getTabChunks_mutex());

	/*
	ImGui needs both its GLFW and OpenGL backends:
	GLFW handles window input and events, while the OpenGL backend renders the interface.
	*/
	IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark(); // Optional

	const GLubyte* version = glGetString(GL_SHADING_LANGUAGE_VERSION);
    if (version) {
        std::cout << "GLSL version: " << version << std::endl;
    } else {
        std::cout << "Failed to get GLSL version (no GL context?)" << std::endl;
    }

    ImGui_ImplGlfw_InitForOpenGL(window->GetWindow(), true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

	seedUI = chunkInstanciator->getCurrentSeed();

	/*
	This setup creates the framebuffer targets and OpenGL state used by the
	compute-shader post-processing pass.
	*/
	tmpSetupComputeShader();

	/*
	The OBJ model uses a dedicated VAO and shader.
	It is separate from chunk VAOs because it uses one model transform rather than chunk data.
	*/
	model_mesh = new mesh("object3d/sheep.obj");
	model_VAO = new VertexArrayObject(
		new VertexBufferObject(model_mesh->dataStruct),
		new ElementBufferObject(model_mesh->shapeAssemblyData),
		shaderHandler->GetShader("model"));

}

Game::~Game() {
	std::cout << "start game destructor" << std::endl;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	if (blockTextureArray.id)
		glDeleteTextures(1, &blockTextureArray.id);
	if (frontGroundFBO)
		glDeleteFramebuffers(1, &frontGroundFBO);
	if (backgroundFBO)
		glDeleteFramebuffers(1, &backgroundFBO);
	if (outFBO)
		glDeleteFramebuffers(1, &outFBO);
	glDeleteTextures(1, &frontColorTexture);
	glDeleteTextures(1, &frontDepthTexture);
	glDeleteTextures(1, &backColorTexture);
	glDeleteTextures(1, &backDepthTexture);
	glDeleteTextures(1, &outTexture);

	delete model_VAO;
	delete model_mesh;
	delete skyBox;
	delete vertexArrayObjectHandler;
	delete chunkInstanciator;
	delete player;
	delete camera;
	delete inputHandler;
	delete shaderHandler;
	delete window;
	std::cout << "end game destructor" << std::endl;
}

void Game::StartLoop() {
	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	u_int fps = 0;

	std::thread chunkThread(&ChunkInstanciator::update, chunkInstanciator);
	// std::thread ecsThread(&ECS::update, ecs);
	
	static auto lastTime = std::chrono::steady_clock::now();

	while(window->ShouldContinue())
	{
		fps++;
		Loop();

		//fps
		auto now = std::chrono::steady_clock::now();
		float deltaSeconds = std::chrono::duration<float>(now - lastTime).count();
		lastTime = now;

		fpsCounter = 1.0f / deltaSeconds;
	}

	/*
	Signal the worker before joining so it has a chance to finish safely,
	instead of destroying shared state while update() is still running.
	*/
	endThreads_mutex.lock();
	endThreads = true;
	endThreads_mutex.unlock();
	
	std::cout << "Joining threads..." << std::endl;
	chunkThread.join();
}



void Game::drawUI()
{
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Game::Loop() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Your UI
	manageUI();

	// Render
	ImGui::Render();

	
	inputHandler->HandleInput();
	const glm::vec3 playerPosition = player->GetPosition();
	

	const glm::vec3 cameraPosition = camera->GetPosition(playerPosition);
	const glm::mat4 view = camera->GetViewMatrix(playerPosition);
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)DEFAULT_WINDOW_WIDTH/(float)DEFAULT_WINDOW_HEIGHT, 0.1f, 16000.0f);
	glm::mat4 matrix = glm::mat4(1.0f);
	matrix = proj * view;
	
	
	glBindFramebuffer(GL_FRAMEBUFFER, frontGroundFBO);
    glViewport(0, 0, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDepthMask(GL_TRUE);
	glClearDepth(1.0);
	window->Clear();

	/*
	Render the scene into the front framebuffer first.
	The skybox has no depth buffer,
	meaning that if we compute the depth buffer later, it will be ignored = drawn over.
	*/
	skyBox->drawSkybox(view, proj, cameraPosition);
	glDepthFunc(GL_LESS);

	GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL error: " << err << std::endl;
    }

	// shaderHandler->Use("RLE-Geometry");
	shaderHandler->Use("default_texture");
	if (Shader::GetActiveShader()) {
		Shader::GetActiveShader()->SetFloat4("cameraPos", cameraPosition.x, cameraPosition.y, cameraPosition.z, 0);
		Shader::GetActiveShader()->Setmat4("matrix", matrix);
		Shader::GetActiveShader()->SetInt("chunk_size_x", AChunk::sizeX);
		Shader::GetActiveShader()->SetInt("chunk_size_y", AChunk::sizeY);
		Shader::GetActiveShader()->SetInt("TextureArraySize", 6);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, blockTextureArray.id);
		Shader::GetActiveShader()->SetInt("ourTextureArray", 0);
	}

	manageVAO();
	draw();

	shaderHandler->Use("model");
	if (Shader::GetActiveShader()) {
		// Map the model's local X-up axis to the world's Y-up axis.
		glm::mat4 meshTransform = glm::rotate(
			glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		meshTransform = glm::rotate(meshTransform, glm::radians(modelRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		meshTransform = glm::rotate(meshTransform, glm::radians(modelRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		meshTransform = glm::rotate(meshTransform, glm::radians(modelRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		meshTransform = glm::scale(meshTransform, glm::vec3(modelScale));


		/*
		Compute the model's ground offset so it rests on the ground plane.
		THIS IS TEMPORARY
		*/
		if (modelGroundOffsetDirty)
		{
			modelGroundOffset = 0.0f;
			if (!model_mesh->vertexes.empty())
			{
				modelGroundOffset = (meshTransform * glm::vec4(model_mesh->vertexes.front(), 1.0f)).y;
				for (const glm::vec3 &vertex : model_mesh->vertexes)
					modelGroundOffset = std::min(
						modelGroundOffset, (meshTransform * glm::vec4(vertex, 1.0f)).y);
			}
			modelGroundOffsetDirty = false;
		}

		glm::vec3 modelPosition = playerPosition;
		modelPosition.y -= modelGroundOffset;
		glm::mat4 model = glm::translate(glm::mat4(1.0f), modelPosition) * meshTransform;
		Shader::GetActiveShader()->Setmat4("model", model);
		Shader::GetActiveShader()->Setmat4("view", view);
		Shader::GetActiveShader()->Setmat4("proj", proj);
	}
	model_VAO->Bind();
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(model_VAO->GetIndicesSize()), GL_UNSIGNED_INT, nullptr);
	model_VAO->Unbind();


	shaderHandler->Use("entity");
	if (Shader::GetActiveShader()) {
		Shader::GetActiveShader()->Setmat4("view", view);
		Shader::GetActiveShader()->Setmat4("proj", proj);
		Shader::GetActiveShader()->Setmat4("model", matrix);
		Shader::GetActiveShader()->SetInt("TextureArraySize", 6);
	}

	// manageVaoEntity();
	// drawEntity();

	//tmpuseComputeShader();

	/*
	Copy the completed front framebuffer into the default framebuffer for display.

	Maybe drawing directly without copying would be more efficient,
	but this is a simple way to implement post-processing effects.
	*/
	glBindFramebuffer(GL_READ_FRAMEBUFFER, frontGroundFBO);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glDrawBuffer(GL_BACK);
	glBlitFramebuffer(0, 0, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 0, 0, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	drawUI();
	window->SwapBuffersAndPollEvents();
}

void Game::draw()
{
	vertexArrayObjectHandler->DrawAll(0);

}

void Game::drawEntity()
{
	glBindVertexArray(model_VAO->GetVAO());
	
	glDrawElementsInstanced(GL_TRIANGLES, static_cast<unsigned int>(model_VAO->GetEBO()->GetSize()), GL_UNSIGNED_INT, 0, amount);
	glDisableVertexAttribArray(1);
	glBindVertexArray(0);
}

//CLANKER CODE
void interpolatePositions(const glm::vec3* oldPos, 
	const glm::vec3* newPos, 
	std::vector<glm::vec3>& interpolatedPos, 
	float alpha,
	int amount) {
	// Ensure vectors have the same size
	// if (!oldPos || !newPos || oldPos->size() != newPos->size()) return;

	// Resize output vector
	interpolatedPos.resize(amount);

	// Perform interpolation
	for (size_t i = 0; i < amount; i++) {
		interpolatedPos[i] = glm::mix((oldPos)[i], (newPos)[i], alpha);
}
}

void Game::manageVaoEntity()
{
	std::vector<glm::mat4> usedModelMatrices(amount);
	entityPos_mutex.lock();
	glm::vec3 *newPos = (glm::vec3*)entityPos->data();

	// float alpha = (currentTime - lastTickTime) / tickInterval;

	std::vector<glm::vec3> interpolatedPos;
	interpolatePositions(oldPos, newPos, interpolatedPos, 0.5f, amount);
	memcpy(oldPos, newPos, amount * sizeof(glm::vec3));

	for (unsigned int i = 0; i < amount; i++) {
		glm::mat4 model = glm::mat4(1.0f);
		glm::vec3 pos = interpolatedPos[i];

		model = glm::translate(model, glm::vec3(pos.x, pos.z, pos.y));
		modelMatrices[i] = model;
	}
	entityPos_mutex.unlock();

	unsigned int VAO = model_VAO->GetVAO();
	
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, amount * sizeof(glm::mat4), &modelMatrices[0], GL_STATIC_DRAW);
	
	glBindVertexArray(VAO);
	// set attribute pointers for matrix (4 times vec4)
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));

	glVertexAttribDivisor(1, 1);
	glVertexAttribDivisor(2, 1);
	glVertexAttribDivisor(3, 1);
	glVertexAttribDivisor(4, 1);

}

void Game::manageVAO()
{
	/*
	VAO operations are queued by the chunk worker because OpenGL objects must be
	created and destroyed on the thread that owns the OpenGL context.
	*/
	const int maxVaoAdditionsPerFrame = 2;
	std::deque<vao_operation> pendingOperations;
	{
		std::lock_guard<std::mutex> lock(vaoOperations_mutex);
		int additionsThisFrame = 0;
		while (!vaoOperations.empty())
		{
			if (vaoOperations.front().type == vao_operation::Type::Add
				&& additionsThisFrame >= maxVaoAdditionsPerFrame)
				break;

			if (vaoOperations.front().type == vao_operation::Type::Add)
				++additionsThisFrame;
			pendingOperations.push_back(vaoOperations.front());
			vaoOperations.pop_front();
		}
	}

	while (!pendingOperations.empty())
	{
		vao_operation operation = pendingOperations.front();
		pendingOperations.pop_front();
		std::pair<int, int> pos = std::make_pair(operation.pos.x, operation.pos.y);
		auto existing = pos_to_vao.find(pos);

		if (operation.type == vao_operation::Type::Remove)
		{
			if (existing != pos_to_vao.end())
			{
				vertexArrayObjectHandler->RemoveVAO(existing->second.VAO);
				pos_to_vao.erase(existing);
				vao_counter--;
			}
			continue;
		}

		/*
		A chunk update replaces the existing mesh for that position rather than
		adding a duplicate VAO, so remove the old GPU object before installing the new one.
		*/
		if (existing != pos_to_vao.end())
		{
			vertexArrayObjectHandler->RemoveVAO(existing->second.VAO);
			pos_to_vao.erase(existing);
			vao_counter--;
		}

		t_vertexData dataStruct = {operation.info->vertices.data(), operation.info->vertices.size()};
		const std::size_t indexDataSize =
			operation.info->indices.size() * sizeof(unsigned int);

		VertexBufferObject *vertexBuffer;
		vertexBuffer = new VertexBufferObject(dataStruct);

		ElementBufferObject *elementBuffer;
		elementBuffer = new ElementBufferObject(operation.info->indices);

		VertexArrayObject *VAO;
		VAO = new VertexArrayObject(
			vertexBuffer,
			elementBuffer,
			shaderHandler->GetShader("default_texture"));

		u_int VAO_id = vertexArrayObjectHandler->AddVAO(VAO);
		pos_to_vao.insert({pos, {VAO_id}});

		delete operation.info;
		vao_counter++;
	}
}

void Game::manageUI()
{
	/*
		Render distance
		Seed
		Speed
		Number of Entities
		Chunk Type
		
	
	
	*/
	ImGui::Begin("This is nice UI", nullptr, ImGuiWindowFlags_AlwaysAutoResize);;

	ImGui::Text("FPS: %d", fpsCounter);
	const glm::vec3 cameraPosition = camera->GetPosition(player->GetPosition());
	ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", cameraPosition.x, cameraPosition.y, cameraPosition.z);
	ImGui::Text("Seed: %ld", chunkInstanciator->getCurrentSeed());
	ImGui::Text("Entities: %d", amount);

	if (ImGui::CollapsingHeader("Actions"))
	{
		if (ImGui::Button("Unload all chunks"))
			chunkInstanciator->unloadAllChunks();
		
		
		
		ImGui::DragScalar("Seed", ImGuiDataType_S64, &seedUI, 0.1f);
		ImGui::Checkbox("Instant update seed", &instantChange);
		if (instantChange || ImGui::Button("Change seed"))
		{
			chunkInstanciator->changeSeed(seedUI);
		}

		ImGui::DragInt("Render distance", &renderDistance, 0.1f, 1, 100);
		if (ImGui::Button("Change render distance"))
		{
			// currentDisplayDistance = 0.f;
			chunkInstanciator->changeRenderDistance(renderDistance);
		}
		
		
		if (ImGui::Button("Switch cursors mode"))
		window->switchCursorMode();
	}

	if (ImGui::CollapsingHeader("Mesh"))
	{
		if (ImGui::DragFloat("Scale", &modelScale, 0.05f, 0.1f, 20.0f))
			modelGroundOffsetDirty = true;
		if (ImGui::DragFloat3("Rotation (degrees)", glm::value_ptr(modelRotation), 1.0f, -360.0f, 360.0f))
			modelGroundOffsetDirty = true;
	}

	player->DrawUI();
	camera->DrawUI();

	
	ImGui::Text("Toogle free mouse: Press left.ALT");
	if (ImGui::CollapsingHeader("Inputs"))
	{
		ImGui::Text("WASD: Move player (normal cursor) / camera (disabled cursor)");
		ImGui::Text("Mouse wheel: Zoom along camera axis");
		ImGui::Text("Space / Left Control: Zoom in / out (normal cursor); move up / down (free camera)");
		ImGui::Text("Left Shift: Sprint");
		ImGui::Text("Left ALT: Toggle free mouse");
	}


	ImGui::End();
}


void Game::tmpSetupComputeShader()
{
	// configure compute shader

	/*
	The front target stores the scene color and depth before post-processing.
	THIS IS NOT USED ANYMORE
	*/
    glGenFramebuffers(1, &frontGroundFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, frontGroundFBO);

    glGenTextures(1, &frontColorTexture);
    glBindTexture(GL_TEXTURE_2D, frontColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frontColorTexture, 0);

    glGenTextures(1, &frontDepthTexture);
    glBindTexture(GL_TEXTURE_2D, frontDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,  DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, frontDepthTexture, 0);

    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Setup backgroundFBO (backColorTexture, backDepthTexture)
	/*
	The background target provides the second depth/color reference used by
	the compute pass when deciding how to blend the frame.
	*/
    glGenFramebuffers(1, &backgroundFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, backgroundFBO);

    glGenTextures(1, &backColorTexture);
    glBindTexture(GL_TEXTURE_2D, backColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,  DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, backColorTexture, 0);

    glGenTextures(1, &backDepthTexture);
    glBindTexture(GL_TEXTURE_2D, backDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,  DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, backDepthTexture, 0);

    glDrawBuffers(1, drawBuffers);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "backgroundFBO incomplete: " << status << std::endl;
    } else {
        std::cout << "backgroundFBO complete!" << std::endl;
    }

	glGenFramebuffers(1, &outFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, outFBO);

	/*
	The output texture is an image bound to the compute shader, so it can be
	written as a grid of pixels independently of the regular raster pipeline.
	*/
    glGenTextures(1, &outTexture);
    glBindTexture(GL_TEXTURE_2D, outTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,  DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTexture, 0);


    glDrawBuffers(1, drawBuffers);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "outFBO incomplete: " << status << std::endl;
    } else {
        std::cout << "outFBO complete!" << std::endl;
    }


    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


	/*
	Query the GPU limits before using the compute shader.
	The arrays and values are kept here as runtime information for the temporary setup.
	*/
	shaderHandler->Use("compute");
	int work_grp_cnt[3];
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_grp_cnt[0]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_grp_cnt[1]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_grp_cnt[2]);


	int work_grp_size[3];
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_grp_size[0]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_grp_size[1]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_grp_size[2]);

	int work_grp_inv;
	glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &work_grp_inv);
}


void Game::tmpuseComputeShader()
{
	
	glBindFramebuffer(GL_FRAMEBUFFER, outFBO);
    glViewport(0, 0, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	
    /*
    The fog distance is scaled for the current compute-shader coordinate system.
    Multiplying by 1000 here is used because THIS IS SHIT
    */
	currentDisplayDistance += (float)renderDistance/360.f * 16;
	
	// int startFog = currentDisplayDistance;
	int startFog = currentDisplayDistance * 1000; // this is because the compute shader is SHIT
	sizeFog = renderDistance * AChunk::sizeX; //this is bad
	
	

	
	shaderHandler->Use("compute");
	if (Shader::GetActiveShader())
	{
		Shader::GetActiveShader()->SetTexture("frontTex", frontColorTexture, GL_TEXTURE0);
		Shader::GetActiveShader()->SetTexture("depthTex", frontDepthTexture, GL_TEXTURE1);
		Shader::GetActiveShader()->SetTexture("backTex", backColorTexture, GL_TEXTURE2);
		Shader::GetActiveShader()->SetInt("startFog", startFog);
		Shader::GetActiveShader()->SetInt("endFog", startFog + sizeFog);
	}

	/*
	Bind the post-process result as an image and dispatch one 16x16 work group
	for each region needed to cover the full window.
	*/
	glBindImageTexture(0, outTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glDispatchCompute((DEFAULT_WINDOW_WIDTH + 15) / 16, (DEFAULT_WINDOW_HEIGHT + 15) / 16, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, outFBO);
	// glBindFramebuffer(GL_READ_FRAMEBUFFER, frontGroundFBO);
}