#include <classes/World/AChunk.hpp>

/*
Modulo that also works for negative world coordinates.
It shifts negative values into the requested positive range before applying modulo.
*/
int mod_floor(int a, int n) {
	if (a < 0)
		a += (-a / n + 1) * n; 
    return (a % n);
}

/*
Abstract constructor for a chunk, it sets the position of the chunk in the world
*/
AChunk::AChunk(int x, int y, int z) : posX(x), posY(y), posZ(z)
{
}

/*
Disconnect this chunk from all neighbouring chunks before it is destroyed.
This prevents a neighbour from keeping a dangling pointer to this object.
*/
AChunk::~AChunk()
{
	if (neighbours.north)
		neighbours.north->setNeighbour(NEIGHB_SOUTH, NULL);
	if (neighbours.south)	
		neighbours.south->setNeighbour(NEIGHB_NORTH, NULL);
	if (neighbours.east)
		neighbours.east->setNeighbour(NEIGHB_WEST, NULL);
	if (neighbours.west)
		neighbours.west->setNeighbour(NEIGHB_EAST, NULL);
}



/*
Generate the chunk through the concrete implementation while protecting its state
and recording generation time when profiling is enabled.
*/
void AChunk::pubGenerate(u_char *data)
{
	if (PROFILER_ON)
		Profiler::StartTracking("Chunk Generate");
	mutex.lock();
	privGenerate(data);
	isGenerated = true;
	mutex.unlock();
	if (PROFILER_ON)
		Profiler::StopTracking("Chunk Generate");
}

/*
Compile the chunk mesh through the concrete implementation and mark it compiled
while holding the chunk mutex.
*/
void AChunk::pubCompile()
{
	if (PROFILER_ON)
		Profiler::StartTracking("Chunk Compile");
	mutex.lock();
	privCompile();
	isCompiled = true;
	mutex.unlock();
	if (PROFILER_ON)
		Profiler::StopTracking("Chunk Compile");
}

/*
Return a vertex description while holding the chunk mutex.
The returned pointer belongs to the concrete chunk implementation and is only
valid while the chunk remains alive.
*/
t_vbo_data AChunk::pubGetPtrVertices()
{
	mutex.lock();
	t_vbo_data ret = privGetPtrVertices();
	mutex.unlock();
	return ret;
}

/*
Return the chunk's index list while holding the chunk mutex.
The pointer lets callers transfer indices to a VAO without exposing the private implementation.
*/
std::vector<unsigned int>* AChunk::pubGetPtrIndices()
{
	mutex.lock();
	std::vector<unsigned int>* ret = privGetPtrIndices();
	mutex.unlock();
	return ret;
}

/*
Check whether a block is filled by delegating to the concrete chunk implementation.
The mutex prevents the block data from changing during the query.
*/
bool AChunk::pubIsFilled(int x, int y, int z)
{
	mutex.lock();
	bool ret = privIsFilled(x, y, z);
	mutex.unlock();
	return ret;
}

/*
Read a block type by delegating to the concrete chunk implementation.
The mutex keeps the data stable during the lookup.
*/
u_char AChunk::pubBlockType(int x, int y, int z)
{
	mutex.lock();
	u_char ret = privBlockType(x, y, z);
	mutex.unlock();
	return ret;
}

/*
Change a block if the chunk has already been generated.
Successful changes mark the chunk for recompilation by setting toUpdate.
*/
bool AChunk::pubChangeBlock(int x, int y, int z, u_char type)
{
	std::cout << "pubChangeBlock x : " << std::endl;
	mutex.lock();
	if (!isGenerated)
	{
		mutex.unlock();
		return false;
	}
	bool ret = false;
	ret = privChangeBlock(x, y, z, type);
	toUpdate = true;
	mutex.unlock();
	return ret;
}

/*Normal func*/

/*
Increase the shared-holder count while the chunk mutex is held.
The count is used by deleter implementations to delay destruction by shared users.
*/
void AChunk::borrow()
{
	mutex.lock();
	sharedHolder++;
	mutex.unlock();
}


/*
Return the chunk's integer position in the world.
*/
glm::ivec2 AChunk::getPos()
{
	return {posX, posY};
}

/*
Replace the pointer to the chunk's raw block or compiled data.
This method is intended for initialization and does not acquire the chunk mutex.
*/
void AChunk::setData(u_char *data)
{
	this->data = data;
}

/*
Replace the chunk data pointer while holding the chunk mutex.
*/
void AChunk::setDataMutex(u_char *data)
{
	mutex.lock();
	this->data = data;
	mutex.unlock();
}

/*
Return the chunk data pointer without synchronization.
*/
u_char *AChunk::getData()
{
	return data;
}

/*
Return the chunk data pointer while holding the chunk mutex.
*/
u_char *AChunk::getDataMutex()
{
	mutex.lock();
	return data;
	mutex.unlock();
}


/*
Set the generated flag while holding the chunk mutex.
*/
void AChunk::setIsGenerated(bool isGenerated)
{
	mutex.lock();
	this->isGenerated = isGenerated;
	mutex.unlock();
	
}

/*
Read the generated flag without synchronization.
*/
bool AChunk::getIsGenerated()
{
	bool ret = isGenerated;
	return ret;
}

/*
Read the generated flag while holding the chunk mutex.
*/
bool AChunk::getIsGeneratedMutex()
{
	mutex.lock();
	bool ret = isGenerated;
	mutex.unlock();
	return ret;
}

/*
Set the compiled flag while holding the chunk mutex.
*/
void AChunk::setIsCompiled(bool isCompiled)
{
	mutex.lock();
	this->isCompiled = isCompiled;
	mutex.unlock();
}

/*
Read the compiled flag while holding the chunk mutex.
*/
bool AChunk::getIsCompiled()
{
	mutex.lock();
	bool ret = isCompiled;
	mutex.unlock();
	return ret;
}

/*
Set whether this chunk needs to be recompiled after a block change.
*/
void AChunk::setToUpdate(bool toUpdate)
{
	mutex.lock();
	this->toUpdate = toUpdate;
	mutex.unlock();
}

/*
Read whether this chunk needs to be recompiled.
*/
bool AChunk::getToUpdate()
{
	mutex.lock();
	bool ret = toUpdate;
	mutex.unlock();
	return ret;
}

/*
Return a snapshot of all four neighbouring chunk pointers.
*/
s_neighbours AChunk::getNeighbours()
{
	// mutex.lock();
	s_neighbours ret =  neighbours;
	// mutex.unlock();
	return ret;
}

/*
Replace the complete set of neighbouring chunk pointers while holding the chunk mutex.
*/
void AChunk::setNeighbours(s_neighbours neighbours)
{
	mutex.lock();
	this->neighbours = neighbours;
	mutex.unlock();
}

/*
Set one neighbouring chunk by direction.
The direction constants distinguish north, south, east, and west.
*/
void AChunk::setNeighbour(int direction, AChunk *chunk)
{
	mutex.lock();
	switch (direction)
	{
	case NEIGHB_NORTH:
		neighbours.north = chunk;
		break;

	case NEIGHB_SOUTH:
		neighbours.south = chunk;
		break;

	case NEIGHB_EAST:
		neighbours.east = chunk;
		break;
	
	case NEIGHB_WEST:
		neighbours.west = chunk;
		break;
	}
	mutex.unlock();
}

