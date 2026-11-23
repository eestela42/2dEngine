#include <classes/World/ChunkInstanciator.hpp>
#include <cstring>
#include <cmath>

ChunkInstanciator::ChunkInstanciator(u_int renderDistance,
										glm::vec3 &playerPos,					std::mutex &playerPos_mutex,
										std::deque<vao_operation> &vaoOperations, std::mutex &vaoOperations_mutex,
										bool &endThread, 						std::mutex &endThread_mutex,
										bool& casse_block,
										int &displayDistance)
	: renderDistance(renderDistance),
		realPlayerPos(playerPos), realPlayerPos_mutex(playerPos_mutex),
		vaoOperations(vaoOperations), vaoOperations_mutex(vaoOperations_mutex),
		endThread(endThread), endThread_mutex(endThread_mutex),
		casse_block(casse_block),
		displayDistance(displayDistance)
{

	/*
	Create a tab that stores the chunks
	as to compile a chunk we need to have its neighbours generated,
	the generation distance is render distance + 1
	*/
	generationDistance = renderDistance + 1;
	size_tab = generationDistance * 2 + 1;
	std::cout << "gen dist : "<< generationDistance << std::endl;
	tabChunks.resize((size_tab), std::vector<AChunk*>());

	for (u_int i = 0; i < size_tab; i++)
	{
		tabChunks[i].resize((size_tab), NULL);
	}

	std::srand(std::time(0));

	_currentSeed = 1722331298;
	_currentSeed = std::rand();
	std::cout << "seed " << _currentSeed << std::endl;

	/*
	The chunk generator uses a static list of PerlinNoise objects to generate terrain.
	This is only used in 3D for now
	*/
	ChunkGenerator::initNoise(_currentSeed);
}

ChunkInstanciator::~ChunkInstanciator()
{
	deleteAllChunks();
}

std::mutex& ChunkInstanciator::getTabChunks_mutex()
{
	return tabChunks_mutex;
}

std::vector<std::vector<AChunk*>>& ChunkInstanciator::getTabChunks()
{
	return tabChunks;
}



/*
To compile a chunk, we need to have its neighbours generated
so we check in the tab if the neighbours are generated, and the right ones
*/
bool ChunkInstanciator::compileChunksWithNeighbours(AChunk *chunk, s_neighbours neighbours)
{
	if (!chunk) //not supposed to happen curently
		return false;
	glm::ivec2 chunkPos = chunk->getPos();
	const bool replacingCompiledChunk = chunk->getIsCompiled();

	if (!neighbours.north || !neighbours.south || !neighbours.east || !neighbours.west
		|| !neighbours.north->getIsGeneratedMutex() || !neighbours.south->getIsGeneratedMutex()
		|| !neighbours.east->getIsGeneratedMutex() || !neighbours.west->getIsGeneratedMutex()
		|| neighbours.north->getPos() != chunkPos + glm::ivec2(0, 1)
		|| neighbours.south->getPos() != chunkPos + glm::ivec2(0, -1)
		|| neighbours.east->getPos() != chunkPos + glm::ivec2(1, 0)
		|| neighbours.west->getPos() != chunkPos + glm::ivec2(-1, 0))
			return false;

	chunk->pubCompile();

	info_VAO *info = new info_VAO();
	t_vbo_data vertices = chunk->pubGetPtrVertices();
	info->vertices.resize(vertices.size);

	if (!info->vertices.empty())
		std::memcpy(info->vertices.data(), vertices.data, vertices.size);

	info->indices = *chunk->pubGetPtrIndices();
	info->pos = chunk->getPos();

	vao_operation operation{vao_operation::Type::Add, info, info->pos};
	operation.replacesExisting = replacingCompiledChunk;
	vaoOperations_mutex.lock();
	if (operation.replacesExisting)
		vaoOperations.push_front(operation);
	else
		vaoOperations.push_back(operation);
	vaoOperations_mutex.unlock();

	chunk->setIsCompiled(true);
	chunk->setToUpdate(false);

	return true;

}

s_neighbours ChunkInstanciator::getNeighbours(glm::ivec2 tabPos, glm::ivec2 chunkPos)
{
	s_neighbours neighbours;
	neighbours.east = tabChunks[(tabPos.x + 1) % size_tab][tabPos.y];
	if (neighbours.east && neighbours.east->getPos().x != chunkPos.x + 1)
		neighbours.east = NULL;
		
	neighbours.west = tabChunks[(tabPos.x - 1 + size_tab) % size_tab][tabPos.y];
	if (neighbours.west && neighbours.west->getPos().x != chunkPos.x - 1)
		neighbours.west = NULL;

	neighbours.north = tabChunks[tabPos.x][(tabPos.y + 1) % size_tab];
	if (neighbours.north && neighbours.north->getPos().y != chunkPos.y + 1)
		neighbours.north = NULL;

	neighbours.south = tabChunks[tabPos.x][(tabPos.y - 1 + size_tab) % size_tab];
	if (neighbours.south && neighbours.south->getPos().y != chunkPos.y - 1)
		neighbours.south = NULL;

	return neighbours;
}

void ChunkInstanciator::setChunkNeighbours(AChunk *chunk, s_neighbours neighbours)
{
	chunk->setNeighbour(NEIGHB_NORTH, neighbours.north);
	if (neighbours.north)
		neighbours.north->setNeighbour(NEIGHB_SOUTH, chunk);

	chunk->setNeighbour(NEIGHB_SOUTH, neighbours.south);
	if (neighbours.south)
		neighbours.south->setNeighbour(NEIGHB_NORTH, chunk);

	chunk->setNeighbour(NEIGHB_EAST, neighbours.east);
	if (neighbours.east)
		neighbours.east->setNeighbour(NEIGHB_WEST, chunk);
		
	chunk->setNeighbour(NEIGHB_WEST, neighbours.west);
	if (neighbours.west)
		neighbours.west->setNeighbour(NEIGHB_EAST, chunk);
}


/*** Get Next Position ***/
/*
The chunks are stored in a tab,
the player position is the "center of the world"
but as the player moves it cannot be the center of the tab

The getNextPos function is used to get the next chunk to manage (generate, compile)
The chunks are managed in a spiral pattern around the player position
*/


void ChunkInstanciator::resetGetNextPos()
{
	size_direction = 0;
	size_pos = 0;
	incr_pos = 0;
}

int ChunkInstanciator::getNextPos(glm::ivec2 &pos)
{
	if (size_direction == 0)
	{
		size_direction++;
		return size_direction;
	}

	pos += incr[incr_pos];
	size_pos++;
	if (size_pos == size_direction)
	{
		size_pos = 0;
		size_direction += incr_pos % 2;
		incr_pos = (incr_pos + 1) % 4;
	}
	return size_direction;
	
}

void ChunkInstanciator::deleteBadChunk(glm::ivec2 chunkTabPos)
{
	/*
	If a chunk is displayed, then we need to remove it from the GPU
	*/
	if (tabChunks[chunkTabPos.x][chunkTabPos.y]->getIsCompiled())
	{
		vaoOperations_mutex.lock();
		vaoOperations.push_back({vao_operation::Type::Remove, NULL,
								tabChunks[chunkTabPos.x][chunkTabPos.y]->getPos()});
		vaoOperations_mutex.unlock();

		tabChunks[chunkTabPos.x][chunkTabPos.y]->setIsCompiled(false);
	}

	if (tabChunks[chunkTabPos.x][chunkTabPos.y]->getIsGeneratedMutex())
	{
		tabChunks_mutex.lock();
		tabChunks[chunkTabPos.x][chunkTabPos.y]->deleter();	
		tabChunks[chunkTabPos.x][chunkTabPos.y] = NULL;
		tabChunks_mutex.unlock();

	}
}

/*
Creates a chunk if it does not exist,
generates it if it is not generated,
and compiles it if it is not compiled
*/
void ChunkInstanciator::createGoodChunk(glm::ivec2 chunkPos, glm::ivec2 chunkTabPos, glm::ivec2 playerChunkPos)
{
	
	if (!tabChunks[chunkTabPos.x][chunkTabPos.y] || tabChunks[chunkTabPos.x][chunkTabPos.y]->getIsGeneratedMutex() == false)
	{
		tabChunks[chunkTabPos.x][chunkTabPos.y] = new Chunk2D(chunkPos.x, chunkPos.y, 0);
		// tabChunks[chunkTabPos.x][chunkTabPos.y] = new Chunk2D(chunkPos.x, chunkPos.y, 0);
		setChunkNeighbours(tabChunks[chunkTabPos.x][chunkTabPos.y], getNeighbours(chunkTabPos, chunkPos));
		tabChunks[chunkTabPos.x][chunkTabPos.y]->pubGenerate(generator.generator(chunkPos));

		s_neighbours neighbours = getNeighbours(chunkTabPos, chunkPos);
		AChunk** neighb = (AChunk**)(&neighbours);
		for (int i = 0; i < 4; i++)
		{
			if (!neighb[i])
				continue ;
			
			glm::ivec2 neighbPos = neighb[i]->getPos();
			glm::ivec2 neighbTabPos;
			neighbTabPos.x = mod_floor(neighbPos.x, size_tab);
			neighbTabPos.y = mod_floor(neighbPos.y, size_tab);

			updateChunk(neighbPos, neighbTabPos, playerChunkPos);
		}
	}

	if (tabChunks[chunkTabPos.x][chunkTabPos.y]->getIsCompiled() == false)
		compileChunksWithNeighbours(tabChunks[chunkTabPos.x][chunkTabPos.y], getNeighbours(chunkTabPos, chunkPos));
	
}


/*
Check if something is wrong with the chunk,
delete it and create a new one
or try to compile it
or update it if needed
*/
void ChunkInstanciator::updateChunk(glm::ivec2 chunkWorldPos, glm::ivec2 chunkTabPos, glm::ivec2 playerChunkPos)
{
	AChunk *chunk = tabChunks[chunkTabPos.x][chunkTabPos.y];
	if (!chunk) //npt supposed to happen curently
		return ;

	bool to_delete = (chunkWorldPos.x - playerChunkPos.x > renderDistance || chunkWorldPos.x - playerChunkPos.x < -renderDistance
				|| chunkWorldPos.y - playerChunkPos.y > renderDistance || chunkWorldPos.y - playerChunkPos.y < -renderDistance);

	if (to_delete && chunk->getIsCompiled())
	{
		vaoOperations_mutex.lock();
		vaoOperations.push_back({vao_operation::Type::Remove, NULL, chunk->getPos()});
		vaoOperations_mutex.unlock();

		chunk->setIsCompiled(false);
	}
	else if (!to_delete && chunk->getIsCompiled() == false)
	{
		compileChunksWithNeighbours(chunk, getNeighbours(chunkTabPos, chunkWorldPos));
	}
	else if (!to_delete && chunk->getToUpdate())
	{
		compileChunksWithNeighbours(chunk, getNeighbours(chunkTabPos, chunkWorldPos));

	}
}

void ChunkInstanciator::checkChunkPosTab(glm::ivec2 chunkWorldPos, glm::ivec2 chunkTabPos, glm::ivec2 playerChunkPos, int x, int y)
{
	if (chunkWorldPos.x != playerChunkPos.x + x || chunkWorldPos.y != playerChunkPos.y + y)
	{	//Updatechunk fait deja le boulot nn ?
		deleteBadChunk(chunkTabPos);
		chunkWorldPos = glm::ivec2(playerChunkPos.x + x, playerChunkPos.y + y);
		createGoodChunk(chunkWorldPos, chunkTabPos, playerChunkPos);
	}
}

glm::ivec2 ChunkInstanciator::getPlayerChunkPos()
{
	std::lock_guard<std::mutex> lock(realPlayerPos_mutex);
	return glm::ivec2(
		static_cast<int>(std::floor(realPlayerPos.x / AChunk::sizeX)),
		static_cast<int>(std::floor(realPlayerPos.z / AChunk::sizeY)));
}

/*
Main update function for the chunk instanciator
*/
void ChunkInstanciator::update()
{
	endThread_mutex.lock();
	while(endThread == false)
	{
		endThread_mutex.unlock();

		/*
		Get the player position and convert it to chunk coordinates
		*/
		glm::ivec2 playerChunkPos = getPlayerChunkPos();
		
		/*
		Convert the player chunk position to tab coordinates
		this will be the "center of the world" for the tab of chunks
		*/
		glm::ivec2 playerTabPos;
		playerTabPos.x = mod_floor(playerChunkPos.x, size_tab);
		playerTabPos.y = mod_floor(playerChunkPos.y, size_tab);
		

		// zop is the position in the tab of chunks, it will be used to iterate over the tab
		glm::ivec2 zop = {0, 0};
		resetGetNextPos();
		_updateMutex.lock();

		//this is the number of slots in the tab of chunks
		const int slotsPerSweep = size_tab * size_tab;

		for (int x1 = 0; getKeepUpdating() && x1 < slotsPerSweep; x1++) {
			endThread_mutex.lock();

			/*
			Check the stop conditions
			*/
			const bool shouldStop = endThread;
			endThread_mutex.unlock();
			if (shouldStop)
				break;
				
			if (getPlayerChunkPos() != playerChunkPos)
				break ;

			/*
			we are in the loop, let's start working
			*/
			getNextPos(zop);
			int x = zop.x;
			int y = zop.y;

			/*
			modulo operation to get the chunk position in the tab,
			this makes the player center of the tab, even if it's not at the center of the tab
			*/
			glm::ivec2 chunkTabPos(mod_floor(playerTabPos.x + x, size_tab),
									mod_floor(playerTabPos.y + y, size_tab));

			
			glm::ivec2 chunkWorldPos;
			AChunk *chunk = tabChunks[chunkTabPos.x][chunkTabPos.y];

			/*
			if no chunk create one, of course
			*/
			if (chunk == NULL)
			{
				chunkWorldPos = glm::ivec2(playerChunkPos.x + x, playerChunkPos.y + y);
				createGoodChunk(chunkWorldPos, chunkTabPos, playerChunkPos);

				continue ;
			}

			/*
			here the chunk is already created
			*/
			chunkWorldPos = chunk->getPos();
			
			/*
			check if the chunk is at the right position in the tab, if not delete it and create a new one
			*/
			checkChunkPosTab(chunkWorldPos, chunkTabPos, playerChunkPos, x, y);

			/*
			we check that everything is fine with it
			*/
			updateChunk(chunkWorldPos, chunkTabPos, playerChunkPos);
		}
		_updateMutex.unlock();
		endThread_mutex.lock();
	}
	endThread_mutex.unlock();
}

void ChunkInstanciator::deleteAllChunks()
{
	displayDistance = 0;
	vaoOperations_mutex.lock();
	std::deque<vao_operation> pendingRemovals;
	for (const vao_operation &operation : vaoOperations)
	{
		if (operation.type == vao_operation::Type::Add)
			delete operation.info;
		else
			pendingRemovals.push_back(operation);
	}
	vaoOperations.swap(pendingRemovals);
	vaoOperations_mutex.unlock();

	for (int i = 0; i < tabChunks.size(); i++)
	{
		for (int j = 0; j < tabChunks[i].size(); j++)
		{
			if (tabChunks[i][j])
			{
				deleteBadChunk(glm::ivec2(i, j));
			}
		}
	}
	tabChunks.clear();
	tabChunks.resize((size_tab), std::vector<AChunk*>());

	for (u_int i = 0; i < size_tab; i++)
	{
		tabChunks[i].resize((size_tab), NULL);
	}
}

long int ChunkInstanciator::getCurrentSeed() const
{
	return _currentSeed;
}

void ChunkInstanciator::changeSeed(long int seed)
{
	if (seed == _currentSeed)
		return ;
	std::cout << "Changing seed to " << seed << std::endl;
	setKeepUpdating(false); 
	
	_currentSeed = seed;
	ChunkGenerator::initNoise(_currentSeed);
	_updateMutex.lock();
	deleteAllChunks();
	_updateMutex.unlock();
	setKeepUpdating(true);

}

void ChunkInstanciator::changeRenderDistance(int newRenderDistance)
{
	if (newRenderDistance == renderDistance)
		return ;
		
	setKeepUpdating(false);
	_updateMutex.lock();
	renderDistance = newRenderDistance;
	generationDistance = renderDistance + 1;
	size_tab = generationDistance * 2 + 1;

	deleteAllChunks();
	
	_updateMutex.unlock();
	setKeepUpdating(true);

}

void ChunkInstanciator::unloadAllChunks()
{
	setKeepUpdating(false);
	_updateMutex.lock();

	deleteAllChunks();
	
	_updateMutex.unlock();
	setKeepUpdating(true);
}


void ChunkInstanciator::setKeepUpdating(bool status)
{
	_keepUpdating_mutex.lock();
	_keepupdating = status;
	_keepUpdating_mutex.unlock();
}

bool ChunkInstanciator::getKeepUpdating()
{
	bool ret;
	_keepUpdating_mutex.lock();
	ret = _keepupdating;
	_keepUpdating_mutex.unlock();
	return ret;
}
