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

// numColorDepthPairs * 2 
GLuint * colorDepthPairs;
int numColorDepthPairs;
GLuint frameBuffer;

GLuint drawProgram;
GLuint drawProgramModelViewUniform;

XrCompositionLayerProjectionView saveLayerProjectionView;



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

void EnumOpenGLExtensions()
{
	minXRglGenFramebuffers = CNFGGetProcAddress( "glGenFramebuffers" );
	minXRglBindFramebuffer = CNFGGetProcAddress( "glBindFramebuffer" );
	minXRglFramebufferTexture2D = CNFGGetProcAddress( "glFramebufferTexture2D" );
	minXRglUniformMatrix4fv = CNFGGetProcAddress( "glUniformMatrix4fv" );
	minXRglVertexAttribPointer = CNFGGetProcAddress( "glVertexAttribPointer" );	
	minXRwglSwapIntervalEXT = CNFGGetProcAddress( "wglSwapIntervalEXT" );	
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

uint32_t GetDepthTextureFromColorTexture( uint32_t tex )
{
	int i;
	for( i = 0; i < numColorDepthPairs; i++ )
	{
		if( colorDepthPairs[i*2] == tex )
			return colorDepthPairs[i*2+1];
	}
	colorDepthPairs = realloc( colorDepthPairs, (numColorDepthPairs+1)*2*sizeof(uint32_t) );
	colorDepthPairs[numColorDepthPairs*2+0] = tex;
	int ret = colorDepthPairs[numColorDepthPairs*2+1] = CreateDepthTexture( tex );
	numColorDepthPairs++;
	return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static void MultiplyMat(float* result, const float* a, const float* b)
{
    result[0] = a[0] * b[0] + a[4] * b[1] + a[8] * b[2] + a[12] * b[3];
    result[1] = a[1] * b[0] + a[5] * b[1] + a[9] * b[2] + a[13] * b[3];
    result[2] = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3];
    result[3] = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3];

    result[4] = a[0] * b[4] + a[4] * b[5] + a[8] * b[6] + a[12] * b[7];
    result[5] = a[1] * b[4] + a[5] * b[5] + a[9] * b[6] + a[13] * b[7];
    result[6] = a[2] * b[4] + a[6] * b[5] + a[10] * b[6] + a[14] * b[7];
    result[7] = a[3] * b[4] + a[7] * b[5] + a[11] * b[6] + a[15] * b[7];

    result[8] = a[0] * b[8] + a[4] * b[9] + a[8] * b[10] + a[12] * b[11];
    result[9] = a[1] * b[8] + a[5] * b[9] + a[9] * b[10] + a[13] * b[11];
    result[10] = a[2] * b[8] + a[6] * b[9] + a[10] * b[10] + a[14] * b[11];
    result[11] = a[3] * b[8] + a[7] * b[9] + a[11] * b[10] + a[15] * b[11];

    result[12] = a[0] * b[12] + a[4] * b[13] + a[8] * b[14] + a[12] * b[15];
    result[13] = a[1] * b[12] + a[5] * b[13] + a[9] * b[14] + a[13] * b[15];
    result[14] = a[2] * b[12] + a[6] * b[13] + a[10] * b[14] + a[14] * b[15];
    result[15] = a[3] * b[12] + a[7] * b[13] + a[11] * b[14] + a[15] * b[15];
}

static void InvertOrthogonalMat(float* result, float* src)
{
    result[0] = src[0];
    result[1] = src[4];
    result[2] = src[8];
    result[3] = 0.0f;
    result[4] = src[1];
    result[5] = src[5];
    result[6] = src[9];
    result[7] = 0.0f;
    result[8] = src[2];
    result[9] = src[6];
    result[10] = src[10];
    result[11] = 0.0f;
    result[12] = -(src[0] * src[12] + src[1] * src[13] + src[2] * src[14]);
    result[13] = -(src[4] * src[12] + src[5] * src[13] + src[6] * src[14]);
    result[14] = -(src[8] * src[12] + src[9] * src[13] + src[10] * src[14]);
    result[15] = 1.0f;
}


int RenderLayer(XrInstance tsoInstance, XrSession tsoSession, XrViewConfigurationView * tsoViewConfigs, int tsoViewConfigsCount,
		 XrSpace tsoStageSpace,  XrTime predictedDisplayTime, XrCompositionLayerProjection * layer, XrCompositionLayerProjectionView * projectionLayerViews,
		 XrView * views, int viewCountOutput, void * opaque )
{
	XrResult  result;

	memset( projectionLayerViews, 0, sizeof( XrCompositionLayerProjectionView ) * viewCountOutput );


	// Render view to the appropriate part of the swapchain image.
	for (uint32_t i = 0; i < viewCountOutput; i++)
	{
		// Each view has a separate swapchain which is acquired, rendered to, and released.
		const tsoSwapchainInfo * viewSwapchain = tsoSwapchains + i;

		XrSwapchainImageAcquireInfo ai = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
		uint32_t swapchainImageIndex;
		result = xrAcquireSwapchainImage(viewSwapchain->handle, &ai, &swapchainImageIndex);
		if (!tsoCheck(tsoInstance, result, "xrAquireSwapchainImage"))
		{
			return 0;
		}

		XrSwapchainImageWaitInfo wi = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
		wi.timeout = XR_INFINITE_DURATION;
		result = xrWaitSwapchainImage(viewSwapchain->handle, &wi);
		if (!tsoCheck(tsoInstance, result, "xrWaitSwapchainImage"))
		{
			return 0;
		}
		
		XrCompositionLayerProjectionView * layerView = projectionLayerViews + i;

		layerView->type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		layerView->pose = views[i].pose;
		layerView->fov = views[i].fov;
		layerView->subImage.swapchain = viewSwapchain->handle;
		layerView->subImage.imageRect.offset.x = 0;
		layerView->subImage.imageRect.offset.y = 0;
		layerView->subImage.imageRect.extent.width = viewSwapchain->width;
		layerView->subImage.imageRect.extent.height = viewSwapchain->height;
			
		const XrSwapchainImageOpenGLKHR * swapchainImage = &tsoSwapchainImages[i][swapchainImageIndex];

		uint32_t colorTexture = swapchainImage->image;
		uint32_t depthTexture = GetDepthTextureFromColorTexture( colorTexture );

		minXRglBindFramebuffer( GL_FRAMEBUFFER, frameBuffer );

		glViewport(layerView->subImage.imageRect.offset.x,
				   layerView->subImage.imageRect.offset.y,
				   layerView->subImage.imageRect.extent.width,
				   layerView->subImage.imageRect.extent.height);

		minXRglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
		minXRglFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

		glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
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
		tsoUtilInitProjectionMat(projMat, GRAPHICS_OPENGL, tanLeft, tanRight, tanUp, tanDown, nearZ, farZ);

		// compute view matrix by inverting the pose
		float invViewMat[16];
		tsoUtilInitPoseMat(invViewMat, &layerView->pose);

		float viewMat[16];
		InvertOrthogonalMat(viewMat, invViewMat);

		float modelViewProjMat[16];
		MultiplyMat(modelViewProjMat, projMat, viewMat);

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

		minXRglBindFramebuffer(GL_FRAMEBUFFER, 0);

		XrSwapchainImageReleaseInfo ri = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
		result = xrReleaseSwapchainImage( viewSwapchain->handle, &ri );
		if (!tsoCheck(tsoInstance, result, "xrReleaseSwapchainImage"))
		{
			return 0;
		}
	}

	memcpy( &saveLayerProjectionView, projectionLayerViews, sizeof( saveLayerProjectionView ) );

	layer->viewCount = viewCountOutput;
	layer->views = projectionLayerViews;

	return 1;
}


int main()
{
	// Tell windows we want to use VT100 terminal color codes.
	#ifdef USE_WINDOWS
	system("");
	#endif

	if( ( tsoNumExtensions = EnumerateExtensions( &tsoExtensionProps ) ) == 0 ) return -1;

	if( ! tsoExtensionSupported( tsoExtensionProps, tsoNumExtensions, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME ) )
	{
		TSOPENXR_ERROR("XR_KHR_opengl_enable not supported!\n");
		return 1;
	}

	if ( !tsoCreateInstance( &tsoInstance, "TSOpenXR Example" ) ) return -1;
	if ( !tsoGetSystemId( tsoInstance, &tsoSystemId ) ) return -1;
	if ( ( tsoNumViewConfigs = tsoEnumeratetsoViewConfigs(tsoInstance, tsoSystemId, &tsoViewConfigs ) ) == 0 ) return -1;

	CNFGSetup( "TSOpenXR Example", 640, 360 );
	EnumOpenGLExtensions();

	int32_t major = 0;
	int32_t minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	
	if ( !tsoCreateSession(tsoInstance, tsoSystemId, &tsoSession, major, minor ) ) return -1;
	if ( !tsoDefaultCreateActions(tsoInstance, tsoSystemId, tsoSession, &tsoActionSet ) ) return -1;
	if ( !tsoCreateStageSpace(tsoInstance, tsoSystemId, tsoSession, &tsoStageSpace ) ) return -1;

	if( SetupRendering() == 0 ) return -1;
	
	if (!tsoCreateSwapchains(tsoInstance, tsoSession, tsoViewConfigs, tsoNumViewConfigs,
						  &tsoSwapchains, &tsoSwapchainImages, &tsoSwapchainLengths )) return -1;
	
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
		tsoHandleLoop( tsoInstance );

		if (tsoSessionReady)
		{
			if (!tsoSyncInput(tsoInstance, tsoSession, tsoActionSet))
			{
				return -1;
			}

			if (!tsoRenderFrame(tsoInstance, tsoSession, tsoViewConfigs, tsoNumViewConfigs,
							 tsoStageSpace, RenderLayer, 0 ) )
			{
				return -1;
			}
		}
		else
		{
			OGUSleep(100000);
		}

		short w, h;
		CNFGClearFrame();
		CNFGGetDimensions( &w, &h );
		glViewport( 0, 0, w, h );

		char debugBuffer[8192];
		snprintf( debugBuffer, sizeof(debugBuffer)-1, 
			"Rect: %d,%d [%d,%d]\n"
			"FoV:  %6.3f%6.3f%6.3f%6.3f\n"
			"Pose: %6.3f%6.3f%6.3f\n",
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
		
		CNFGPenX = 1;
		CNFGPenY = 1;
		CNFGColor( 0xffffffff );
		CNFGDrawText( debugBuffer, 2 );
		CNFGSwapBuffers();
	}

	XrResult result;
	int i;
	for( i = 0; i < tsoNumViewConfigs; i++ )
	{
		result = xrDestroySwapchain(tsoSwapchains[i].handle);
		tsoCheck(tsoInstance, result, "xrDestroySwapchain");
	}

	result = xrDestroySpace(tsoStageSpace);
	tsoCheck(tsoInstance, result, "xrDestroySpace");

	result = xrEndSession(tsoSession);
	tsoCheck(tsoInstance, result, "xrEndSession");

	result = xrDestroySession(tsoSession);
	tsoCheck(tsoInstance, result, "xrDestroySession");

	result = xrDestroyInstance(tsoInstance);
	tsoCheck(XR_NULL_HANDLE, result, "xrDestroyInstance");

	return 0;
}

//For rawdraw (we don't use this)
void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }
