#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "testgame.h"

float * GeometryWorldSpaceLocations;
uint32_t * GeometryColors;
int * GeometryIndices;
int numLines;
int resLines;
double frameTime;

#define DEBUG_BUFFER_W 60
#define DEBUG_BUFFER_H 120

struct DebugBufferLine
{
	char debugBuffer[DEBUG_BUFFER_W];
	double time;
	uint32_t color;
};

struct DebugBufferLine debugBuffer[DEBUG_BUFFER_H];

void RenderTerminal()
{
	int y, x;
	for( y = 0; y < DEBUG_BUFFER_W; y++ )
	{
		struct DebugBufferLine * dbl = debugBuffer + y;
		for( x = 0; x < DEBUG_BUFFER_H; x++ )
		{
			float offset[3] = { ( x - DEBUG_BUFFER_W ) / (float)DEBUG_BUFFER_W, ( y - DEBUG_BUFFER_H ) / (float)DEBUG_BUFFER_H, -1 };

			

			char c = dbl->debugBuffer[x];

			AppendLines( (const float[]){ 0, 0, 0, 1, 1, 1, },
				(const uint32_t[]) { 0x000000ff, 0xffffffff, }, 1 );
		}
	}

}

void AppendDebugBufferLine( const char * s, uint32_t color )
{
	int i;
	for( i = 0; i < DEBUG_BUFFER_H-1; i++ )
	{
		debugBuffer[i] = debugBuffer[i+1];
	}
	int len = strlen( s );
	if( len > DEBUG_BUFFER_W ) len = DEBUG_BUFFER_W;
	memcpy( debugBuffer[i].debugBuffer, s, len );
	memset( debugBuffer[i].debugBuffer + len, 0, DEBUG_BUFFER_W - len );
	debugBuffer[i].time = frameTime;
	debugBuffer[i].color = color;
}

void GameLogError(const char * fmt, ...)
{
	char buff[8192];
	va_list args;
	va_start (args, fmt);
	vsnprintf (buff, sizeof(buff)-1, fmt, args);
	va_end (args);
	printf( "\033[31m%s", buff );
	AppendDebugBufferLine( buff, 0xff0000ff );
}

void GameLogInfo(const char * fmt, ...)
{
	char buff[8192];
	va_list args;
	va_start (args, fmt);
	vsnprintf (buff, sizeof(buff)-1, fmt, args);
	va_end (args);
	printf( "\033[39m%s", buff );
	AppendDebugBufferLine( buff, 0xaaaaaaff );
}



void AppendLines( const float * Locations, const uint32_t * Colors, int addLines )
{
	int newLines = numLines + addLines;
	if( newLines > resLines )
	{
		GeometryWorldSpaceLocations = realloc( GeometryWorldSpaceLocations, sizeof(float)*6*newLines );
		GeometryColors = realloc( GeometryColors, sizeof(uint32_t)*2*newLines );
		GeometryIndices = realloc( GeometryIndices, sizeof(uint32_t)*2*newLines );
		int i;
		for( i = 0; i < addLines*2; i++ )
		{
			GeometryIndices[numLines*2+i] = numLines*2+i;
		}
		resLines = newLines;
	}
	memcpy( GeometryWorldSpaceLocations + numLines*6, Locations, addLines * sizeof(float)*6 );
	memcpy( GeometryColors + numLines*2, Colors, addLines * sizeof(uint32_t)*2 );
	numLines = newLines;
}

