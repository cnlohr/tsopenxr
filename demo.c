// Example for using tsopenxr.
// It's like a little game.
//
// (C) 2022 Charles Lohr (under the MIT/x11 license)
//
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>

#include "os_generic.h"

#define TSOPENXR_IMPLEMENTATION
#include "tsopenxr.h"

#define CNFGOGL
#define CNFGOGL_NEED_EXTENSION
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

GLuint * colorDepthPairs; // numColorDepthPairs * 2 
int numColorDepthPairs;
GLuint debugTexture;
GLuint frameBuffer;

GLuint drawProgram;
GLuint drawProgramModelViewUniform;
GLuint textureProgram;
GLuint textureProgramModelViewUniform;
GLuint textureProgramTextureUniform;
short windowW, windowH;

XrCompositionLayerProjectionView saveLayerProjectionView;

tsoContext TSO;

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER	 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT  0x8D00
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#endif


void (*minXRglGenFramebuffers)( GLsizei n, GLuint *ids );
void (*minXRglBindFramebuffer)( GLenum target, GLuint framebuffer );
void (*minXRglFramebufferTexture2D)( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level );
void (*minXRglUniformMatrix4fv)( GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (*minXRglVertexAttribPointer)( GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void * pointer);
BOOL (*minXRwglSwapIntervalEXT)(int interval);
void (*minXRglBlitNamedFramebuffer)( GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

void EnumOpenGLExtensions()
{
	minXRglGenFramebuffers = CNFGGetProcAddress( "glGenFramebuffers" );
	minXRglBindFramebuffer = CNFGGetProcAddress( "glBindFramebuffer" );
	minXRglFramebufferTexture2D = CNFGGetProcAddress( "glFramebufferTexture2D" );
	minXRglUniformMatrix4fv = CNFGGetProcAddress( "glUniformMatrix4fv" );
	minXRglVertexAttribPointer = CNFGGetProcAddress( "glVertexAttribPointer" );	
	minXRwglSwapIntervalEXT = CNFGGetProcAddress( "wglSwapIntervalEXT" );
	minXRglBlitNamedFramebuffer = CNFGGetProcAddress( "glBlitNamedFramebuffer" );
}


int SetupRendering()
{
	minXRglGenFramebuffers(1, &frameBuffer);

	drawProgram = CNFGGLInternalLoadShader(
		"#version 100\n"
		"uniform mat4 modelViewProjMatrix;"
		"attribute vec3 vPos;"
		"attribute vec4 vColor;"
		"varying vec4 vc;"
		"void main() { gl_Position = vec4( modelViewProjMatrix * vec4( vPos.xyz, 1.0 ) ); vc = vColor; }",

		"varying vec4 vc;"
		"void main() { gl_FragColor = vec4(vc/255.0); }" 
	);
	if( drawProgram <= 0 )
	{
		TSOPENXR_ERROR( "Error: Cannot load shader\n" );
		return 0;
	}
	CNFGglUseProgram( drawProgram );
	drawProgramModelViewUniform = CNFGglGetUniformLocation ( drawProgram , "modelViewProjMatrix" );
	CNFGglBindAttribLocation( drawProgram, 0, "vPos" );
	CNFGglBindAttribLocation( drawProgram, 1, "vColor" );

	textureProgram = CNFGGLInternalLoadShader(
		"#version 100\n"
		"uniform mat4 modelViewProjMatrix;"
		"attribute vec3 vPos;"
		"attribute vec2 vUV;"
		"varying vec2 uv;"
		"void main() { gl_Position = vec4( modelViewProjMatrix * vec4( vPos.xyz, 1.0 ) ); uv = vUV; }",

		"varying vec2 uv;"
		"sampler2D texture;"
		"void main() { gl_FragColor = vec4(uv, 1.0, 1.0); }" 
	);
	if( textureProgram <= 0 )
	{
		TSOPENXR_ERROR( "Error: Cannot load shader\n" );
		return 0;
	}
	CNFGglUseProgram( textureProgram );
	textureProgramModelViewUniform = CNFGglGetUniformLocation ( textureProgram , "modelViewProjMatrix" );
	textureProgramTextureUniform = CNFGglGetUniformLocation ( textureProgram , "texture" );
	CNFGglBindAttribLocation( textureProgram, 0, "vPos" );
	CNFGglBindAttribLocation( textureProgram, 1, "vUV" );

	// Generate a texture to copy the 
	glGenTextures(1, &debugTexture);
	glBindTexture(GL_TEXTURE_2D, debugTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0 );

	return glGetError() == 0;
}

uint32_t CreateDepthTexture(uint32_t colorTexture)
{
	int32_t width, height;
	glBindTexture(GL_TEXTURE_2D, colorTexture);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

	uint32_t depthTexture;
	glGenTextures(1, &depthTexture);
	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0 );
	return depthTexture;
}

uint32_t GetDepthTextureFromColorTexture( uint32_t colorTexture )
{
	int i;
	for( i = 0; i < numColorDepthPairs; i++ )
	{
		if( colorDepthPairs[i*2] == colorTexture )
			return colorDepthPairs[i*2+1];
	}
	colorDepthPairs = realloc( colorDepthPairs, (numColorDepthPairs+1)*2*sizeof(uint32_t) );
	colorDepthPairs[numColorDepthPairs*2+0] = colorTexture;
	int ret = colorDepthPairs[numColorDepthPairs*2+1] = CreateDepthTexture( colorTexture );
	numColorDepthPairs++;
	return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int RenderLayer(tsoContext * ctx, XrTime predictedDisplayTime, XrCompositionLayerProjectionView * projectionLayerViews, int viewCountOutput )
{
	XrResult  result;

	// Render view to the appropriate part of the swapchain image.
	for (uint32_t i = 0; i < viewCountOutput; i++)
	{
		XrCompositionLayerProjectionView * layerView = projectionLayerViews + i;
		uint32_t swapchainImageIndex;

		// Each view has a separate swapchain which is acquired, rendered to, and released.
		tsoAcquireSwapchain( ctx, i, &swapchainImageIndex );		
		const XrSwapchainImageOpenGLKHR * swapchainImage = &ctx->tsoSwapchainImages[i][swapchainImageIndex];
		
		uint32_t colorTexture = swapchainImage->image;
		uint32_t depthTexture = GetDepthTextureFromColorTexture( colorTexture );

		minXRglBindFramebuffer( GL_FRAMEBUFFER, frameBuffer );

		glViewport(layerView->subImage.imageRect.offset.x,
				   layerView->subImage.imageRect.offset.y,
				   layerView->subImage.imageRect.extent.width,
				   layerView->subImage.imageRect.extent.height);

		minXRglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
		minXRglFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

		glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
		glClearDepth(1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		// Render Pipeline copied from https://github.com/hyperlogic/openxrstub/blob/main/src/main.cpp

		// convert XrFovf into an OpenGL projection matrix.
		const float tanLeft = tan(layerView->fov.angleLeft);
		const float tanRight = tan(layerView->fov.angleRight);
		const float tanDown = tan(layerView->fov.angleDown);
		const float tanUp = tan(layerView->fov.angleUp);
		const float nearZ = 0.05f;
		const float farZ = 100.0f;
		float projMat[16];
		float invViewMat[16];
		float viewMat[16];
		float modelViewProjMat[16];
		tsoUtilInitProjectionMat(&layerView->pose, projMat, invViewMat, viewMat, modelViewProjMat, GRAPHICS_OPENGL, tanLeft, tanRight, tanUp, tanDown, nearZ, farZ);

		// Actually start rendering
		
		CNFGglUseProgram( drawProgram );
		minXRglUniformMatrix4fv( drawProgramModelViewUniform, 1, GL_FALSE, modelViewProjMat );

		static const float Vertices[] = { 
			0, 0, 0, 1, 0, 0, 
			0, 0, 0, 0, 1, 0, 
			0, 0, 0, 0, 0, 1 };
		static const uint32_t Colors[] = {
			0xff0000ff, 0xff0000ff,
			0xff00ff00, 0xff00ff00,
			0xffff0000, 0xffff0000, 
			};
			
		static const int GeometryIndices[] = {
			0, 1, 2, 3, 4, 5,
		};

		CNFGglEnableVertexAttribArray(0);
		CNFGglEnableVertexAttribArray(1);
		minXRglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, Vertices);
		minXRglVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_FALSE, 0, Colors);

		glEnable( GL_BLEND );
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
		glLineWidth( 2.0 );

		glDrawElements( GL_LINES, 6, GL_UNSIGNED_INT, GeometryIndices );
		
		
		
		
		CNFGglUseProgram( textureProgram );
		minXRglUniformMatrix4fv( textureProgramModelViewUniform, 1, GL_FALSE, modelViewProjMat );
		
		
		
		minXRglBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Show debug view.
		if( i == 0 )
		{
			int targh = layerView->subImage.imageRect.extent.width*windowH/windowW;
			int thofs = (layerView->subImage.imageRect.extent.height-targh)/2;
			minXRglBlitNamedFramebuffer( frameBuffer, 0, 0, thofs, layerView->subImage.imageRect.extent.width, targh+thofs, 0, 0, windowW, windowH, GL_COLOR_BUFFER_BIT, GL_NEAREST );
		}

		tsoReleaseSwapchain( &TSO, i );
	}

	memcpy( &saveLayerProjectionView, projectionLayerViews, sizeof( saveLayerProjectionView ) );

	return 0;
}


int main()
{
	int r;

	// Tell windows we want to use VT100 terminal color codes.
	#ifdef USE_WINDOWS
	system("");
	#endif

	CNFGSetup( "TSOpenXR Example", 640, 360 );

	int32_t major = 0;
	int32_t minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	
	if( ( r = tsoInitialize( &TSO, major, minor, TSO_DO_DEBUG, "TSOpenXR Example", 0 ) ) ) return r;
	
	// Assign a layer render function.
	TSO.tsoRenderLayer = RenderLayer;

	EnumOpenGLExtensions();

	if( SetupRendering() == 0 ) return -1;
	
	if ( ( r = tsoDefaultCreateActions( &TSO ) ) ) return r;
	
	if ( ( r = tsoCreateSwapchains( &TSO ) ) ) return r;
	
	// Disable vsync, on appropriate system.
	#ifdef USE_WINDOWS
	if( minXRwglSwapIntervalEXT )
	{
		minXRwglSwapIntervalEXT( 0 );
	}
	#else
	CNFGSetVSync( 0 );
	#endif

	while ( CNFGHandleInput() )
	{
		tsoHandleLoop( &TSO );

		if (!TSO.tsoSessionReady)
		{
			short w, h;
			CNFGClearFrame();
			CNFGGetDimensions( &w, &h );
			glViewport( 0, 0, w, h );
			CNFGPenX = 1;
			CNFGPenY = 1;
			CNFGColor( 0xffffffff );
			CNFGDrawText( "Session Not Ready.", 2 );
			CNFGSwapBuffers();
			OGUSleep(100000);
			continue;
		}

		if ( ( r = tsoSyncInput( &TSO ) ) )
		{
			return r;
		}

		CNFGClearFrame();
		CNFGGetDimensions( &windowW, &windowH );

		if ( ( r = tsoRenderFrame( &TSO ) ) )
		{
			return r;
		}

		glViewport( 0, 0, windowW, windowH );

		char debugBuffer[16384];
		char * dbg = debugBuffer;
		dbg += snprintf( dbg, sizeof(debugBuffer)-1-(dbg-debugBuffer), 
			"Rect: %d,%d [%d,%d]\n"
			"FoV:  %6.3f%6.3f%6.3f%6.3f\n"
			"Pose: %6.3f%6.3f%6.3f\n\n"
			,
			saveLayerProjectionView.subImage.imageRect.offset.x,
			saveLayerProjectionView.subImage.imageRect.offset.y,
			saveLayerProjectionView.subImage.imageRect.extent.width,
			saveLayerProjectionView.subImage.imageRect.extent.height,
			saveLayerProjectionView.fov.angleLeft,
			saveLayerProjectionView.fov.angleRight,
			saveLayerProjectionView.fov.angleUp,
			saveLayerProjectionView.fov.angleDown,
			saveLayerProjectionView.pose.position.x,
			saveLayerProjectionView.pose.position.y,
			saveLayerProjectionView.pose.position.z
		);
		
		XrSpaceLocation handLocations[2] = { 0 };
		handLocations[0].type = XR_TYPE_SPACE_LOCATION;
		handLocations[1].type = XR_TYPE_SPACE_LOCATION;
		XrResult handResults[2];
		handResults[0] = xrLocateSpace( TSO.tsoHandSpace[0], TSO.tsoStageSpace, TSO.tsoPredictedDisplayTime, &handLocations[0] );
		handResults[1] = xrLocateSpace( TSO.tsoHandSpace[1], TSO.tsoStageSpace, TSO.tsoPredictedDisplayTime, &handLocations[1] );

		dbg += snprintf( dbg, sizeof(debugBuffer)-1-(dbg-debugBuffer), 
			"Hand1 %6.3f%6.3f%6.3f %d %d\n"
			"Hand2 %6.3f%6.3f%6.3f %d %d\n",
			handLocations[0].pose.position.x,
			handLocations[0].pose.position.y,
			handLocations[0].pose.position.z,
			handResults[0],
			(int)handLocations[0].locationFlags,
			handLocations[1].pose.position.x,
			handLocations[1].pose.position.y,
			handLocations[1].pose.position.z,
			handResults[1],
			(int)handLocations[1].locationFlags
		);


		XrActionStateFloat floatState[2];
		XrActionStateGetInfo actionGetInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
		actionGetInfo.action = TSO.triggerAction;
		actionGetInfo.subactionPath = TSO.handPath[0];
		handResults[0] = tsoCheck( &TSO, xrGetActionStateFloat( TSO.tsoSession, &actionGetInfo, floatState + 0 ), "xrGetActionStateFloat1" );
		actionGetInfo.subactionPath = TSO.handPath[1];
		handResults[1] = tsoCheck( &TSO, xrGetActionStateFloat( TSO.tsoSession, &actionGetInfo, floatState + 1 ), "xrGetActionStateFloat2" );
		dbg += snprintf( dbg, sizeof(debugBuffer)-1-(dbg-debugBuffer), 
			"Input1 %d %d %f\n"
			"Input2 %d %d %f\n",
			handResults[0], floatState[0].isActive, floatState[0].currentState,
			handResults[1], floatState[1].isActive, floatState[1].currentState );
			
		CNFGPenX = 1;
		CNFGPenY = 1;
		CNFGColor( 0xffffffff );
		CNFGDrawText( debugBuffer, 2 );

		glReadBuffer(GL_FRONT);
		glCopyTexImage2D( debugTexture, 0, GL_RGBA, 0, 0, windowW, windowH, 0 );

		CNFGSwapBuffers();
	}


	return tsoTeardown( &TSO );
}

//For rawdraw (we don't use this)
void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }
