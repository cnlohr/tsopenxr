#ifndef _TESTGAME_H
#define _TESTGAME_H

#include <stdint.h>


extern double frameTime;
extern float * GeometryWorldSpaceLocations;
extern uint32_t * GeometryColors;
extern int * GeometryIndices;
extern int numLines;
extern int resLines;

extern uint32_t frameBuffer;

int SetupRendering( void );
void AppendLines( const float * Locations, const uint32_t * Colors, int addLines );
void AppendDebugBufferLine( const char * s, uint32_t color );
void GameLogError(const char * fmt, ...);
void GameLogInfo(const char * fmt, ...);

void RenderTerminal( void );

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER	 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT  0x8D00
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#endif

#endif
