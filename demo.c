// Example for using tsopenxr.
// Shows a bunch of debug info.
//
// (C) 2022 Charles Lohr (under the MIT/x11 license)
//
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>

#include "os_generic.h"

#define CNFGOGL
#define CNFGOGL_NEED_EXTENSION
#define CNFG_IMPLEMENTATION
#include "rawdraw_sf.h"

#define TSOPENXR_IMPLEMENTATION
#include "tsopenxr.h"

#ifdef ANDROID
	#define GLES_VER_TARG "100"
	#define TARGET_OFFSCREEN
#elif defined(USE_WINDOWS)
	#define GLES_VER_TARG "150"
#else
	// OpenGL 4.3, desktop.
	#define GLES_VER_TARG "150"
#endif


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



//
// We may be operating on a really primitive OpenGL version.
// No need to include GLEW or anything like that!
//

#ifndef GL_FRAMEBUFFER
#define GL_TEXTURE0 0x84C0
#define GL_FRAMEBUFFER	 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT  0x8D00
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#endif

#ifndef GL_MAJOR_VERSION
#define GL_MAJOR_VERSION   0x821B
#endif

#ifndef GL_MINOR_VERSION
#define GL_MINOR_VERSION   0x821C
#endif

void (*minXRglGenFramebuffers)( GLsizei n, GLuint *ids );
void (*minXRglBindFramebuffer)( GLenum target, GLuint framebuffer );
void (*minXRglFramebufferTexture2D)( GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level );
void (*minXRglUniformMatrix4fv)( GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (*minXRglVertexAttribPointer)( GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void * pointer);
void (*minXRwglSwapIntervalEXT)(int interval);
void (*minXRglBlitNamedFramebuffer)( GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
void (*minXRglActiveTexture)( GLenum texture );
void (*minXRglUniform1i)( GLint location, GLint v0 );

void EnumOpenGLExtensions()
{
	minXRglGenFramebuffers = CNFGGetProcAddress( "glGenFramebuffers" );
	minXRglBindFramebuffer = CNFGGetProcAddress( "glBindFramebuffer" );
	minXRglFramebufferTexture2D = CNFGGetProcAddress( "glFramebufferTexture2D" );
	minXRglUniformMatrix4fv = CNFGGetProcAddress( "glUniformMatrix4fv" );
	minXRglVertexAttribPointer = CNFGGetProcAddress( "glVertexAttribPointer" );
	minXRwglSwapIntervalEXT = CNFGGetProcAddress( "wglSwapIntervalEXT" );
	minXRglBlitNamedFramebuffer = CNFGGetProcAddress( "glBlitNamedFramebuffer" );
	minXRglActiveTexture = CNFGGetProcAddress( "glActiveTexture" );
	minXRglUniform1i = CNFGGetProcAddress( "glUniform1i" );
}

// Setup rendering, loading shaders.
// We do use the convenience functions from
// rawdraw for shader loading admittedly.
int SetupRendering()
{
	minXRglGenFramebuffers(1, &frameBuffer);

	drawProgram = CNFGGLInternalLoadShader(
		"#version " GLES_VER_TARG "\n"
		"uniform mat4 modelViewProjMatrix;"
		"attribute vec3 vPos;"
		"attribute vec4 vColor;"
		"varying vec4 vc;"
		"void main() { gl_Position = vec4( modelViewProjMatrix * vec4( vPos.xyz, 1.0 ) ); vc = vColor; }",

		"#version " GLES_VER_TARG "\n"
		"varying vec4 vc;"
		"void main() { gl_FragColor = vec4(vc/255.0); }" 
	);
	if( drawProgram <= 0 )
	{
		TSOPENXR_ERROR( "Error: Cannot load shader\n" );
		return -1;
	}
	CNFGglUseProgram( drawProgram );
	drawProgramModelViewUniform = CNFGglGetUniformLocation ( drawProgram , "modelViewProjMatrix" );
	CNFGglBindAttribLocation( drawProgram, 0, "vPos" );
	CNFGglBindAttribLocation( drawProgram, 1, "vColor" );

	textureProgram = CNFGGLInternalLoadShader(
		"#version " GLES_VER_TARG "\n"
		"uniform mat4 modelViewProjMatrix;"
		"attribute vec3 vPos;"
		"attribute vec2 vUV;"
		"varying vec2 uv;"
		"void main() { gl_Position = vec4( modelViewProjMatrix * vec4( vPos.xyz, 1.0 ) ); uv = vUV; }",

		"#version " GLES_VER_TARG "\n"
		"varying vec2 uv;"
		"uniform sampler2D texture;"
		"void main() { gl_FragColor = vec4(texture2D(texture, uv).rgb, 1.0); }" 
	);
	if( textureProgram <= 0 )
	{
		TSOPENXR_ERROR( "Error: Cannot load shader\n" );
		return -1;
	}
	CNFGglUseProgram( textureProgram );
	textureProgramModelViewUniform = CNFGglGetUniformLocation ( textureProgram , "modelViewProjMatrix" );
	textureProgramTextureUniform = CNFGglGetUniformLocation ( textureProgram , "texture" );
	CNFGglBindAttribLocation( textureProgram, 0, "vPos" );
	CNFGglBindAttribLocation( textureProgram, 1, "vUV" );

	glGenTextures(1, &debugTexture);
	glBindTexture(GL_TEXTURE_2D, debugTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef TARGET_OFFSCREEN
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
#else
	uint32_t colordata[] = { 0xff000000, 0xff0000ff, 0xff00ff00, 0xffff0000 };
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, colordata );
#endif
	glBindTexture(GL_TEXTURE_2D, 0);

	int err = glGetError();
	if( err )
	{
		TSOPENXR_ERROR( "OpenGL Error On Init %d\n", err );
		return err;
	}
	return 0;
}

uint32_t CreateDepthTexture(uint32_t colorTexture, int width, int height )
{
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

uint32_t GetDepthTextureFromColorTexture( uint32_t colorTexture, int width, int height )
{
	int i;
	for( i = 0; i < numColorDepthPairs; i++ )
	{
		if( colorDepthPairs[i*2] == colorTexture )
			return colorDepthPairs[i*2+1];
	}
	colorDepthPairs = realloc( colorDepthPairs, (numColorDepthPairs+1)*2*sizeof(uint32_t) );
	colorDepthPairs[numColorDepthPairs*2+0] = colorTexture;
	int ret = colorDepthPairs[numColorDepthPairs*2+1] = CreateDepthTexture( colorTexture, width, height );
	numColorDepthPairs++;
	return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int RenderLayer(tsoContext * ctx, XrTime predictedDisplayTime, XrCompositionLayerProjectionView * projectionLayerViews, int viewCountOutput )
{
	
	// If each view has a separate swapchain which is acquired, rendered to, and released.
	// then it would need to be inside the for loop.  But we don't do that here, because
	// we are double-wide;
	uint32_t swapchainImageIndex;
	tsoAcquireSwapchain( ctx, 0, &swapchainImageIndex );
	const XrSwapchainImageOpenGLKHR * swapchainImage = &ctx->tsoSwapchainImages[0][swapchainImageIndex];

	uint32_t colorTexture = swapchainImage->image;
	uint32_t depthTexture = GetDepthTextureFromColorTexture( colorTexture, ctx->tsoSwapchains[0].width, ctx->tsoSwapchains[0].height );
	minXRglBindFramebuffer( GL_FRAMEBUFFER, frameBuffer );
	minXRglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
	minXRglFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

	glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
#ifndef ANDROID
	glClearDepth(1.0f);
#endif
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	// Render view to the appropriate part of the swapchain image.
	for (uint32_t i = 0; i < viewCountOutput; i++)
	{
		XrCompositionLayerProjectionView * layerView = projectionLayerViews + i;
		glViewport(layerView->subImage.imageRect.offset.x,
				   layerView->subImage.imageRect.offset.y,
				   layerView->subImage.imageRect.extent.width,
				   layerView->subImage.imageRect.extent.height);

		const float nearZ = 0.05f;
		const float farZ = 100.0f;
		float projMat[16];
		float invViewMat[16];
		float viewMat[16];
		float modelViewProjMat[16];
		tsoUtilInitProjectionMat( layerView, projMat, invViewMat, viewMat, modelViewProjMat, GRAPHICS_OPENGL, nearZ, farZ);

		// Actually start rendering
		glEnable( GL_BLEND );
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
		
		CNFGglUseProgram( drawProgram );
		minXRglUniformMatrix4fv( drawProgramModelViewUniform, 1, GL_FALSE, modelViewProjMat );

		{
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

			glLineWidth( 2.0 );

			glDrawElements( GL_LINES, 6, GL_UNSIGNED_INT, GeometryIndices );
		}
		
		
		CNFGglUseProgram( textureProgram );
		minXRglUniformMatrix4fv( textureProgramModelViewUniform, 1, GL_FALSE, modelViewProjMat );
		{
			minXRglUniform1i( textureProgramTextureUniform, 0 );
			glBindTexture(GL_TEXTURE_2D, debugTexture);
			static const float Vertices[] = { 
				0, 0, 0,  0, 0, 1,  1, 0, 0,  1, 0, 1 };
			static const float UVs[] = {
				1, 0,  1, 1,  0, 0,  0, 1 };
			static const int GeometryIndices[] = {
				0, 1, 2, 2, 1, 3,
			};

			CNFGglEnableVertexAttribArray(0);
			CNFGglEnableVertexAttribArray(1);
			minXRglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, Vertices);
			minXRglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, UVs);

			glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, GeometryIndices );
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		// Show debug view.
		if( i == 0 && minXRglBlitNamedFramebuffer )
		{
			int targh = layerView->subImage.imageRect.extent.width*windowH/windowW;
			int thofs = (layerView->subImage.imageRect.extent.height-targh)/2;
			minXRglBlitNamedFramebuffer( frameBuffer, 0, 0, thofs, layerView->subImage.imageRect.extent.width, targh+thofs, 0, 0, windowW, windowH, GL_COLOR_BUFFER_BIT, GL_LINEAR );
		}

	}
	minXRglBindFramebuffer(GL_FRAMEBUFFER, 0);
	tsoReleaseSwapchain( &TSO, 0 );

	memcpy( &saveLayerProjectionView, projectionLayerViews, sizeof( saveLayerProjectionView ) );

	return 0;
}


int main()
{
	int r;

	// Tell windows we want to use VT100 terminal color codes.
	#if defined( USE_WINDOWS ) && !defined( _MSC_VER )
	system("");
	#endif

	CNFGSetup( "TSOpenXR Example", 256, 256 );

	int32_t major = 0;
	int32_t minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	
	if( ( r = tsoInitialize( &TSO, major, minor, TSO_DO_DEBUG | TSO_DOUBLEWIDE, "TSOpenXR Example", 0 ) ) ) return r;
	
	// Assign a layer render function.
	TSO.tsoRenderLayer = RenderLayer;

	EnumOpenGLExtensions();

	if( SetupRendering() ) return -1;
	
	if ( ( r = tsoDefaultCreateActions( &TSO ) ) ) return r;
	
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

		#ifdef TARGET_OFFSCREEN
		minXRglBindFramebuffer( GL_FRAMEBUFFER, frameBuffer );
		minXRglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, debugTexture, 0);
		minXRglFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, GetDepthTextureFromColorTexture( debugTexture, windowW, windowH ), 0);

		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		#endif

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
		
		XrSpaceLocation handLocations[2] = { { XR_TYPE_SPACE_LOCATION }, { XR_TYPE_SPACE_LOCATION } };
		XrResult handResults[4];
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


		XrActionStateFloat floatState[2] = { { XR_TYPE_ACTION_STATE_FLOAT }, { XR_TYPE_ACTION_STATE_FLOAT } };
		XrActionStateBoolean boolState[2] = { { XR_TYPE_ACTION_STATE_BOOLEAN }, { XR_TYPE_ACTION_STATE_BOOLEAN } };
		XrActionStateGetInfo actionGetInfo = { XR_TYPE_ACTION_STATE_GET_INFO };

		actionGetInfo.action = TSO.triggerAction;
		actionGetInfo.subactionPath = TSO.handPath[0];
		handResults[0] = tsoCheck( &TSO, xrGetActionStateFloat( TSO.tsoSession, &actionGetInfo, floatState + 0 ), "xrGetActionStateFloat1" );
		actionGetInfo.subactionPath = TSO.handPath[1];
		handResults[1] = tsoCheck( &TSO, xrGetActionStateFloat( TSO.tsoSession, &actionGetInfo, floatState + 1 ), "xrGetActionStateFloat2" );

		actionGetInfo.action = TSO.triggerActionClick;
		actionGetInfo.subactionPath = TSO.handPath[0];
		handResults[2] = tsoCheck( &TSO, xrGetActionStateBoolean( TSO.tsoSession, &actionGetInfo, boolState + 0 ), "XrActionStateBoolean1" );
		actionGetInfo.subactionPath = TSO.handPath[1];
		handResults[3] = tsoCheck( &TSO, xrGetActionStateBoolean( TSO.tsoSession, &actionGetInfo, boolState + 1 ), "XrActionStateBoolean 2" );

		dbg += snprintf( dbg, sizeof(debugBuffer)-1-(dbg-debugBuffer), 
			"Input1 Analog %d %d %f\n"
			"Input2 Analog %d %d %f\n"
			"Input1 Digital %d %d %d\n"
			"Input2 Digital %d %d %d\n",
			handResults[0], floatState[0].isActive, floatState[0].currentState,
			handResults[1], floatState[1].isActive, floatState[1].currentState,
			handResults[2], boolState[0].isActive, boolState[0].currentState,
			handResults[3], boolState[1].isActive, boolState[1].currentState );
			
		CNFGPenX = 1;
		CNFGPenY = 1;
		CNFGColor( 0xffffffff );
		CNFGDrawText( debugBuffer, 2 );

		CNFGSwapBuffers();

		// Tricky: If we are doing off-screen rendering, we have to copy-pasta our debug window into a texture.
#ifndef TARGET_OFFSCREEN
		glReadBuffer(GL_FRONT);
		glBindTexture(GL_TEXTURE_2D, debugTexture);
		glCopyTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, windowW, windowH, 0 );
		glBindTexture(GL_TEXTURE_2D, 0);
#else
		minXRglBindFramebuffer( GL_FRAMEBUFFER, 0 );
#endif

	}

	return tsoTeardown( &TSO );
}

//For rawdraw (we don't use this)
void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

// On Android.
void HandleResume() { }
void HandleSuspend() { }
