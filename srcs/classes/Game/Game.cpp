#include <classes/Game/Game.hpp>
#include <classes/Game/mesh.hpp>
#include <algorithm>
#include "../../../imgui/imgui.h"
#include "../../../imgui/backends/imgui_impl_glfw.h"
#include "../../../imgui/backends/imgui_impl_opengl3.h"

unsigned int depthMapFBO;
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

unsigned int depthMap;


Game::Game()
{
	window = new Window("Minecraft", DrawMode::FILL);
	inputHandler = new InputHandler(window->GetWindow());
	inputHandler->AddCallback((I_Input*)this); // c'est quoi c'truc
	inputHandler->AddCallback((I_Input*)window);
	shaderHandler = new ShaderHandler("shaders");
	

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

	skyBox = new SkyBox(shaderHandler->GetShader("cubemap"));

	chunkInstanciator = new ChunkInstanciator(renderDistance, cameraPosition, playerPos_mutex,
													vaoOperations, vaoOperations_mutex,
													playerHasMoved, playerHasMoved_mutex,
													endThreads, endThreads_mutex,
													casse_block,
													displayDistance);

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

	tmpSetupComputeShader();

	
	model_mesh = new mesh("object3d/42.obj");
	model_VAO = new VertexArrayObject(
		new VertexBufferObject(model_mesh->dataStruct),
		new ElementBufferObject(model_mesh->shapeAssemblyData),
		shaderHandler->GetShader("model"));

}

Game::~Game() {
	std::cout << "start game destructor" << std::endl;
	delete model_VAO;
	delete model_mesh;
	delete window;
	delete inputHandler;
	delete shaderHandler;
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
	const glm::vec3 direction = glm::normalize(cameraDirection);
	const float zoomDistance = static_cast<float>(inputHandler->ConsumeScrollOffset()) * 2.0f;
	if (zoomDistance != 0.0f)
	{
		float adjustedZoomDistance = zoomDistance;
		if (zoomDistance > 0.0f && direction.y < -0.001f)
		{
			const float distanceToGround = -cameraPosition.y / direction.y;
			if (distanceToGround >= 0.0f)
				adjustedZoomDistance = std::min(zoomDistance, std::max(0.0f, distanceToGround - 0.1f));
		}
		cameraPosition += direction * adjustedZoomDistance;
	}

	if (direction.y < -0.001f)
	{
		const float distance = -cameraPosition.y / direction.y;
		if (distance > 0.0f)
		{
			modelPosition = cameraPosition + direction * distance;
			modelPosition.y = 0.0f;
		}
	}

	glm::mat4 view = glm::lookAt(cameraPosition, cameraPosition + cameraDirection, cameraUp);
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)DEFAULT_WINDOW_WIDTH/(float)DEFAULT_WINDOW_HEIGHT, 0.1f, 16000.0f);
	glm::mat4 matrix = glm::mat4(1.0f);
	matrix = proj * view;
	
	glBindFramebuffer(GL_FRAMEBUFFER, frontGroundFBO);
    glViewport(0, 0, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDepthMask(GL_TRUE);
	glClearDepth(1.0);
	window->Clear();

	glDepthFunc(GL_LEQUAL);
	skyBox->drawSkybox(view, proj, cameraPosition);
	glDepthFunc(GL_LESS);

	GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL error: " << err << std::endl;
    }

	// shaderHandler->Use("RLE-Regular");
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
		glm::mat4 meshTransform(1.0f);
		meshTransform = glm::rotate(meshTransform, glm::radians(modelRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		meshTransform = glm::rotate(meshTransform, glm::radians(modelRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		meshTransform = glm::rotate(meshTransform, glm::radians(modelRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		meshTransform = glm::scale(meshTransform, glm::vec3(modelScale));

		float lowestY = 0.0f;
		if (!model_mesh->vertexes.empty())
		{
			lowestY = (meshTransform * glm::vec4(model_mesh->vertexes.front(), 1.0f)).y;
			for (const glm::vec3& vertex : model_mesh->vertexes)
				lowestY = std::min(lowestY, (meshTransform * glm::vec4(vertex, 1.0f)).y);
		}

		glm::mat4 model = glm::translate(glm::mat4(1.0f),
			glm::vec3(modelPosition.x, modelPosition.y - lowestY, modelPosition.z)) * meshTransform;
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
	std::deque<vao_operation> pendingOperations;
	vaoOperations_mutex.lock();
	pendingOperations.swap(vaoOperations);
	vaoOperations_mutex.unlock();

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

		if (existing != pos_to_vao.end())
		{
			vertexArrayObjectHandler->RemoveVAO(existing->second.VAO);
			pos_to_vao.erase(existing);
			vao_counter--;
		}

		t_vertexData dataStruct = {operation.info->vertices.data(), operation.info->vertices.size()};
		VertexArrayObject *VAO = new VertexArrayObject(new VertexBufferObject(dataStruct),
														new ElementBufferObject(operation.info->indices),
														shaderHandler->GetShader("default_texture"));
		u_int VAO_id = vertexArrayObjectHandler->AddVAO(VAO);
		pos_to_vao.insert({pos, {VAO_id}});
		delete operation.info;
		vao_counter++;
	}
	vaoOperations_mutex.unlock();
}

//for the stream, auto-move
bool Gogogo = false;

void Game::SendKeys(u_char *keyState, double mouseMoveX, double mouseMoveY) {
	float speedMultiplier = (keyState[KEY_SPRINT] & KEY_HOLD) ? 20 : 1;
	glm::vec3 oldCamPos = cameraPosition;

	playerPos_mutex.lock();
	glm::vec3 horizontalForward(cameraDirection.x, 0.0f, cameraDirection.z);
	if (glm::length(horizontalForward) > 0.0001f)
		horizontalForward = glm::normalize(horizontalForward);
	else
		horizontalForward = glm::normalize(glm::vec3(
			cos(glm::radians(yaw)), 0.0f, sin(glm::radians(yaw))));
	glm::vec3 horizontalRight = glm::normalize(glm::cross(horizontalForward, cameraUp));
	glm::vec3 horizontalMovement(0.0f);

	if(keyState[KEY_MOVE_FORWARD] & KEY_HOLD)
		horizontalMovement += horizontalForward;
	if(keyState[KEY_MOVE_BACKWARD] & KEY_HOLD)
		horizontalMovement -= horizontalForward;
	if(keyState[KEY_MOVE_RIGHTWARD] & KEY_HOLD)
		horizontalMovement += horizontalRight;
	if(keyState[KEY_MOVE_LEFTWARD] & KEY_HOLD)
		horizontalMovement -= horizontalRight;

	if (glm::length(horizontalMovement) > 1.0f)
		horizontalMovement = glm::normalize(horizontalMovement);
	cameraPosition += speed * speedMultiplier * horizontalMovement;

	if(keyState[KEY_SPACE] & KEY_HOLD)
		cameraPosition += cameraUp * speed * speedMultiplier;
	if(keyState[KEY_MOVE_DOWNWARD] & KEY_HOLD)
		cameraPosition -= cameraUp * speed * speedMultiplier;

	if (keyState[KEY_DELETE_ONE_BLOCK] & KEY_PRESS)
		Gogogo = !Gogogo;
	if (Gogogo)
	{
		cameraPosition += 2 * speed * speedMultiplier * horizontalForward;
		mouseMoveX += 0.5;
	}
	if (keyState[KEY_SWITCH_CURSOR_MODE] & KEY_PRESS)
	{
		window->switchCursorMode();
	}

	playerPos_mutex.unlock();
	if (oldCamPos.x / AChunk::sizeX != cameraPosition.x / AChunk::sizeX || oldCamPos.z / AChunk::sizeY != cameraPosition.z / AChunk::sizeY)
	{
		playerHasMoved_mutex.lock();
		playerHasMoved = true;
		playerHasMoved_mutex.unlock();
	}


	// if (window->getCursorMode() == GLFW_CURSOR_DISABLED)
	// {

	// 	yaw += mouseMoveX * sensitivity;
	// 	pitch -= mouseMoveY * sensitivity;
	// 	if(pitch > 89.0f)
	// 		pitch =  89.0f;
	// 	if(pitch < -89.0f)
	// 		pitch = -89.0f;
	// }

	double xpos, ypos;
	glfwGetCursorPos(window->GetWindow(), &xpos, &ypos);


	if (window->getCursorMode() == GLFW_CURSOR_DISABLED)
	{
		if (firstMouse)
		{
			lastX = xpos;
			lastY = ypos;
			firstMouse = false;
		}

		mouseMoveX = xpos - lastX;
		mouseMoveY = ypos - lastY;

		lastX = xpos;
		lastY = ypos;

		yaw += mouseMoveX * sensitivity;
		pitch -= mouseMoveY * sensitivity;
		pitch = glm::clamp(pitch, -89.0f, 89.0f);
	}
	else
	{
		firstMouse = true;  // Reset when leaving FPS mode
	}
	cameraDirection.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraDirection.y = sin(glm::radians(pitch));
	cameraDirection.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

	view = glm::lookAt(	cameraPosition, 
						cameraPosition + glm::normalize(cameraDirection),
						cameraUp);
						


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
		ImGui::DragFloat("Scale", &modelScale, 0.05f, 0.1f, 20.0f);
		ImGui::DragFloat3("Rotation (degrees)", glm::value_ptr(modelRotation), 1.0f, -360.0f, 360.0f);
	}

	
	ImGui::Text("Toogle free mouse: Press left.ALT");
	if (ImGui::CollapsingHeader("Inputs"))
	{
		ImGui::Text("WASD: Move parallel to ground");
		ImGui::Text("Mouse wheel: Zoom along camera axis");
		ImGui::Text("Space: Go up");
		ImGui::Text("Left Shift: Go down");
		ImGui::Text("Left Control: Sprint");
		ImGui::Text("Left ALT: Toggle free mouse");
	}


	ImGui::End();
}


void Game::tmpSetupComputeShader()
{
	// configure compute shader

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

	glBindImageTexture(0, outTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glDispatchCompute((DEFAULT_WINDOW_WIDTH + 15) / 16, (DEFAULT_WINDOW_HEIGHT + 15) / 16, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, outFBO);
	// glBindFramebuffer(GL_READ_FRAMEBUFFER, frontGroundFBO);
}