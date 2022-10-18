// BARE BONES Example for using tsopenxr
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


// Force debugging off for smaller executable.
#define TSOPENXR_ENABLE_DEBUG 0

#define TSOPENXR_IMPLEMENTATION
#include "tsopenxr.h"


GLuint frameBuffer;
short windowW, windowH;

tsoContext TSO;


// In case we don't get this from OpenGL (sometimes some primitive headers don't have it)
#ifndef GL_MAJOR_VERSION
#define GL_MAJOR_VERSION   0x821B
#define GL_MINOR_VERSION   0x821C
#endif

int RenderLayer(tsoContext * ctx, XrTime predictedDisplayTime, XrCompositionLayerProjectionView * projectionLayerViews, int viewCountOutput )
{
	// Render view to the appropriate part of the swapchain image.
	for (uint32_t v = 0; v < viewCountOutput; v++)
	{
		XrCompositionLayerProjectionView * layerView = projectionLayerViews + v;
		uint32_t swapchainImageIndex;

		// Each view has a separate swapchain which is acquired, rendered to, and released.
		tsoAcquireSwapchain( ctx, v, &swapchainImageIndex );

		#ifdef XR_USE_PLATFORM_XLIB
		glXMakeCurrent( CNFGDisplay, CNFGWindow, CNFGCtx );
		#endif

		const XrSwapchainImageOpenGLKHR * swapchainImage = &ctx->tsoSwapchainImages[v][swapchainImageIndex];

		uint32_t colorTexture = swapchainImage->image;

		int width =layerView->subImage.imageRect.extent.width;
		int height = layerView->subImage.imageRect.extent.height;
		uint32_t * bufferdata = malloc( width*height*4 );

		// Show user different colors in right and left eye.
		memset( bufferdata, v*250, width*height * 4 );
		glBindTexture( GL_TEXTURE_2D, colorTexture );

		glTexSubImage2D( GL_TEXTURE_2D, 0,
			layerView->subImage.imageRect.offset.x, layerView->subImage.imageRect.offset.y,
			width, height, 
			GL_RGBA, GL_UNSIGNED_BYTE, bufferdata );
		glBindTexture( GL_TEXTURE_2D, 0 );

		free( bufferdata );

		tsoReleaseSwapchain( &TSO, v );
	}

	return 0;
}


int main()
{
	int r;

	// Tell windows we want to use VT100 terminal color codes.
	#ifdef USE_WINDOWS
	system("");
	#endif

	CNFGSetup( "TSOpenXR Example", -1, -1 );

	int32_t major = 0;
	int32_t minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	
	if( ( r = tsoInitialize( &TSO, major, minor, TSO_DO_DEBUG, "TSOpenXR Example", 0 ) ) ) return r;
	
	// Assign a layer render function.
	TSO.tsoRenderLayer = RenderLayer;
	
	if ( ( r = tsoDefaultCreateActions( &TSO ) ) ) return r;

	while ( CNFGHandleInput() )
	{
		tsoHandleLoop( &TSO );

		if (!TSO.tsoSessionReady)
		{
			OGUSleep(100000);
			continue;
		}

		if ( ( r = tsoSyncInput( &TSO ) ) )
		{
			return r;
		}

		if ( ( r = tsoRenderFrame( &TSO ) ) )
		{
			return r;
		}
	}

	return tsoTeardown( &TSO );
}

//For rawdraw (we don't use this)
void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }
