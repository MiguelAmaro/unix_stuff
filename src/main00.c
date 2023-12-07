#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#define Min(a, b) ((a)<(b)?(a):(b))
#define Max(a, b) ((a)>(b)?(a):(b))
#define WORK_AQUIRED     ((uint8_t)1)
#define WORK_NOT_AQUIRED ((uint8_t)0)

typedef struct tile tile;
struct tile
{
	int32_t Minx;
	int32_t Miny;
	int32_t Maxx;
	int32_t Maxy;
};
typedef struct work_queue work_queue;
struct work_queue
{
	tile *Tiles;
	tile *At;
	tile *OnePastLast;
	uint32_t *Image;
	pthread_t *ThreadHandles;
	uint32_t TileCount;
	uint32_t TileWidth;
	uint32_t TileHeight;
	uint32_t Width;
	uint32_t Height;
	uint32_t ThreadCount;
	uint8_t         ShouldBegin;
	pthread_cond_t  BeginSignal;
	pthread_mutex_t BeginSignalMutex;
	pthread_mutex_t WorkAtMutex;
};

uint8_t RenderTile(work_queue *Work, uint32_t ThreadId)
{
	pthread_mutex_lock(&Work->WorkAtMutex);
	tile *Tile = (Work->At<Work->OnePastLast)?Work->At++:NULL;
	pthread_mutex_unlock(&Work->WorkAtMutex);

	if(Tile == NULL) return WORK_NOT_AQUIRED;

	printf("Worker [%-2c] on tile %d of %d...\n",
		'a' + ThreadId, (uint32_t)(Tile-Work->Tiles), Work->TileCount);
	
	for(uint32_t y=Tile->Miny; y<Min(Tile->Maxy, Work->Height); y++)
	{
		for(uint32_t x=Tile->Minx; x<Min(Tile->Maxx, Work->Width); x++)
		{
			float u = (float)x/(float)Work->Width;
			float v = (float)y/(float)Work->Height;
			u -= 0.5f;
			v -= 0.5f;
			Work->Image[y*Work->Width + x] = (sqrt(u*u + v*v) > 0.25)?'a'+ThreadId:'*';
		}
	}
	
	return WORK_AQUIRED;
}
uint32_t WorkGetWorkerIndexFromThreadHandle(work_queue *Work, pthread_t ThreadId)
{
	uint32_t Result = 0xFFFFFFFF;
	for(uint32_t ThreadIdx=0; ThreadIdx<Work->ThreadCount; ThreadIdx++)
	{
		if(ThreadId == Work->ThreadHandles[ThreadIdx])
		{
			Result = ThreadIdx;
			break;
		}
	}
	return Result;
}
void *WorkProc(void *Params)
{
	work_queue *Work = (work_queue *)Params;
	pthread_t ThreadId = pthread_self();
	uint32_t  WorkerId = WorkGetWorkerIndexFromThreadHandle(Work, ThreadId);
	printf("Worker [%-2c] on standby\n", 'a' + WorkerId);

	pthread_mutex_lock(&Work->BeginSignalMutex);
	while(!Work->ShouldBegin)
	{
		pthread_cond_wait(&Work->BeginSignal, &Work->BeginSignalMutex);
	}
	pthread_mutex_unlock(&Work->BeginSignalMutex);
	while(RenderTile(Work, WorkerId));
	return NULL;
}
work_queue WorkQueueInit(uint32_t *Image, uint32_t Width, uint32_t Height, uint32_t ThreadCount)
{
	work_queue Result = {0};
	Result.TileWidth  = Max(10, Width/Max(1, ThreadCount));
	Result.TileHeight = Max(10, Width/Max(1, ThreadCount));
	uint32_t TileCountX = (Width/Result.TileWidth)   + 1;
	uint32_t TileCountY = (Height/Result.TileHeight) + 1;
	Result.TileCount    = TileCountX*TileCountY;
	Result.Tiles        = (tile *)malloc(sizeof(tile)*Result.TileCount);
	Result.At           = Result.Tiles;
	Result.OnePastLast  = Result.Tiles + Result.TileCount;
	Result.Image  = Image;
	Result.Width  = Width;
	Result.Height = Height;
	Result.ThreadHandles = (pthread_t *)malloc(sizeof(pthread_t)*ThreadCount);
	Result.ThreadCount   = ThreadCount;
	for(uint32_t TileIdx=0; TileIdx<Result.TileCount; TileIdx++)
	{
		uint32_t TilePosX = TileIdx%TileCountX;
		uint32_t TilePosY = TileIdx/TileCountX;
		Result.Tiles[TileIdx].Minx = TilePosX*Result.TileWidth;
		Result.Tiles[TileIdx].Maxx = TilePosX*Result.TileWidth + Result.TileWidth;
		Result.Tiles[TileIdx].Miny = TilePosY*Result.TileHeight;
		Result.Tiles[TileIdx].Maxy = TilePosY*Result.TileHeight + Result.TileHeight;
	}
	return Result;
}
void WorkBegin(work_queue *Work)
{
	printf("workers are allowed to work\n");
	Work->ShouldBegin = 1;
	pthread_cond_broadcast(&Work->BeginSignal);
	return;
}
void WorkSpawnWorkers(work_queue *Work)
{
	Work->ShouldBegin = 0;
	pthread_cond_init(&Work->BeginSignal, NULL);
	pthread_mutex_init(&Work->BeginSignalMutex, NULL);
	pthread_mutex_init(&Work->WorkAtMutex, NULL);

	for(int ThreadIdx=0; ThreadIdx<Work->ThreadCount; ThreadIdx++)
	{
		pthread_create(&Work->ThreadHandles[ThreadIdx], NULL, WorkProc, Work);
	}
	return;
}
void WorkWaitForWorkers(work_queue *Work)
{
	printf("waiting for workers\n");
	for(int ThreadIdx=0; ThreadIdx<Work->ThreadCount; ThreadIdx++)
	{
		pthread_join(Work->ThreadHandles[ThreadIdx], NULL);	
	}
	printf("all workers finished. continuing...\n");
	return;
}

int main(void)
{
	uint32_t  Width  = 200;
	uint32_t  Height = 200;
	uint32_t *Image = (uint32_t *)malloc(sizeof(uint32_t)*Width*Height);	

	uint32_t CoreCount = sysconf(_SC_NPROCESSORS_ONLN);
	uint32_t ThreadCount = CoreCount;

	work_queue Work = WorkQueueInit(Image, Width, Height, ThreadCount);
	WorkSpawnWorkers(&Work);
	WorkBegin(&Work);
	WorkWaitForWorkers(&Work);

	for(uint32_t y=0; y<Height; y++)
	{
		for(uint32_t x=0; x<Width; x++)
		{

			printf("%c", Image[y*Width + x]);
		}
		printf("\n");
	}
	printf("done\n");
	return 0;
}

