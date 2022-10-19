/**
	Truly Simple C OpenXR header-only library

	(based on https://github.com/cnlohr/openxr-minimal)
	(based on https://github.com/hyperlogic/openxrstub/blob/main/src/main.cpp)
	
	Portions are:
		Copyright (c) 2020 Anthony J. Thibault
	The rest is:
		Copyright (c) 2022 Charles Lohr
	
	Both under the MIT/x11 License.
	
	To use this library in a given project's C file:
	#define TSOPENXR_IMPLEMENTATION
*/
	
	

#ifndef _TSOPENXR_H
#define _TSOPENXR_H

#if (defined( WIN32 ) || defined (WINDOWS) || defined( _WIN32) ) && !defined( USE_WINDOWS )
#define USE_WINDOWS 1
#endif

#define XR_USE_GRAPHICS_API_OPENGL
#if defined(USE_WINDOWS)
#define XR_USE_PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(__ANDROID__)
#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_OPENGL_ES
#else
#define XR_USE_PLATFORM_XLIB
#endif

#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"

#ifdef XR_USE_PLATFORM_ANDROID
	#define OPENXR_SELECTED_GRAPHICS_API XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME
#else
	#define OPENXR_SELECTED_GRAPHICS_API XR_KHR_OPENGL_ENABLE_EXTENSION_NAME
#endif


typedef struct
{
	XrSwapchain handle;
	int32_t width;
	int32_t height;
} tsoSwapchainInfo;

struct tsoContext_t;

// return zero to indicate layer submissions are good.
// Nonzero will be a "no-render" for the layer.
typedef int (*tsoRenderLayerFunction_t)(struct tsoContext_t * ctx, XrTime predictedDisplayTime, XrCompositionLayerProjectionView * projectionLayerViews, int viewCountOutput );

typedef struct tsoContext_t
{
	XrExtensionProperties * tsoExtensionProps;
	int tsoNumExtensions;
	XrApiLayerProperties * tsoLayerProps;
	int tsoNumLayerProps;
	XrViewConfigurationView * tsoViewConfigs;
	int tsoNumViewConfigs;
		
	XrSpace tsoHandSpace[2];
	XrTime tsoPredictedDisplayTime;
	
	XrInstance tsoInstance;
	XrSystemId tsoSystemId;
	XrSession tsoSession;
	XrActionSet tsoActionSet;
	XrSpace tsoStageSpace;	

	// Actions
	XrPath handPath[2];

	XrPath selectPath[2];
	XrPath squeezeValuePath[2];
	XrPath squeezeClickPath[2];
	XrPath triggerValuePath[2];
	XrPath triggerClickPath[2];
	XrPath posePath[2];
	XrPath hapticPath[2];
	XrPath menuClickPath[2];

	XrAction grabAction;
	XrAction triggerAction;
	XrAction triggerActionClick;
	XrAction poseAction;
	XrAction vibrateAction;
	XrAction menuAction;
	
	// Swapchain, etc.
	
	int numSwapchainsPerFrame;
	tsoSwapchainInfo * tsoSwapchains;
	XrSwapchainImageOpenGLKHR ** tsoSwapchainImages; //[tsoNumViewConfigs][tsoSwapchainLengths[...]]
	uint32_t * tsoSwapchainLengths; //[tsoNumViewConfigs]
	
	// For debugging.
	int tsoPrintAll;
	
	int tsoSessionReady;
	XrSessionState tsoXRState;
	tsoRenderLayerFunction_t tsoRenderLayer;
	int flags;
	void * opaque;
} tsoContext;


// This will compile-in the extra debug stuff.
#ifndef TSOPENXR_ENABLE_DEBUG
#define TSOPENXR_ENABLE_DEBUG 1
#endif

#ifndef TSOPENXR_ERROR
#define TSOPENXR_ERROR printf
#endif

#ifndef TSOPENXR_INFO
#define TSOPENXR_INFO printf
#endif

// Init Flags
#define TSO_DO_DEBUG 1	// Log all
#define TSO_DOUBLEWIDE 2  // Enable double-wide frames.

// Most functions return 0 on success.
// nonzero on failure.
int tsoInitialize( tsoContext * ctx, int32_t openglMajor, int32_t openglMinor, int flags, const char * appname, void * opaque );

int tsoInitAction( tsoContext * ctx, XrActionType type, const char * action_name, const char * localized_name, XrAction * action );
int tsoDefaultCreateActions( tsoContext * ctx );

int tsoHandleLoop( tsoContext * ctx );
int tsoCreateSwapchains( tsoContext * ctx );
int tsoSyncInput( tsoContext * ctx );
int tsoRenderFrame( tsoContext * ctx );
int tsoAcquireSwapchain( tsoContext * ctx, int swapchain, uint32_t * swapchainImageIndex );
int tsoReleaseSwapchain( tsoContext * ctx, int swapchain );
int tsoDestroySwapchains( tsoContext * ctx );
int tsoTeardown( tsoContext * ctx );

// Utility functions
void tsoUtilInitPoseMat(float* result, const XrPosef * pose);
enum GraphicsAPI { GRAPHICS_VULKAN, GRAPHICS_OPENGL, GRAPHICS_OPENGL_ES, GRAPHICS_D3D };
void tsoUtilInitProjectionMat(XrCompositionLayerProjectionView * layerView, float* projMat, float * invViewMat, float * viewMat, float * modelViewProjMat,
								enum GraphicsAPI graphicsApi, 
								const float nearZ, const float farZ);
void tsoInvertOrthogonalMat(float* result, float* src);
void tsoMultiplyMat(float* result, const float* a, const float* b);


// Internal functions.
int tsoCheck( tsoContext * ctx, XrResult result, const char* str );
int tsoEnumeratetsoViewConfigs( tsoContext * ctx );
int tsoEnumerateExtensions( tsoContext * ctx );
int tsoExtensionSupported( tsoContext * ctx, const char* extensionName); // Returns 1 if supported, 0 if not.
int tsoEnumerateLayers( tsoContext * ctx );
int tsoCreateInstance( tsoContext * ctx, const char * appname );
int tsoGetSystemId( tsoContext * ctx );
int tsoCreateSession( tsoContext * ctx, uint32_t openglMajor, uint32_t openglMinor );
int tsoCreateStageSpace( tsoContext * ctx );
int tsoBeginSession( tsoContext * ctx );

#ifdef TSOPENXR_IMPLEMENTATION


#include <stdlib.h>
#include <string.h>

// For limited OpenGL Platforms, Like Windows.
#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif

#ifndef GL_SRGB8
#define GL_SRGB8 0x8C41
#endif

#ifdef MSC_VER
#define alloca _alloca
#endif

int tsoInitialize( tsoContext * ctx, int32_t openglMajor, int32_t openglMinor, int flags, const char * appname, void * opaque )
{
	int r;
	memset( ctx, 0, sizeof( *ctx ) );
	ctx->opaque = opaque;
	ctx->flags = flags;
	ctx->tsoPrintAll = !!(flags & TSO_DO_DEBUG);

#ifdef XR_USE_PLATFORM_ANDROID
	PFN_xrInitializeLoaderKHR loaderFunc;
	XrResult result = xrGetInstanceProcAddr( XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&loaderFunc );

	if( tsoCheck( 0, result, "xrGetInstanceProcAddr(xrInitializeLoaderKHR)" ) )
	{
		return result;
	}

	XrLoaderInitInfoAndroidKHR init_data = { XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR };
	struct android_app* app = gapp;
	const struct JNIInvokeInterface ** jniiptr = app->activity->vm;
	jobject activity = app->activity->clazz;
	init_data.applicationVM = jniiptr;
	init_data.applicationContext = activity;
	loaderFunc( &init_data );
#endif

	if( ( tsoEnumerateExtensions( ctx ) ) )
	{
		return 1;
	}

	if( ! tsoExtensionSupported( ctx, OPENXR_SELECTED_GRAPHICS_API ) )
	{
		TSOPENXR_ERROR(OPENXR_SELECTED_GRAPHICS_API" not supported!\n");
		return 1;
	}

	if( ( r = tsoCreateInstance( ctx, appname ) ) ) return r;
	if( ( r = tsoEnumerateLayers( ctx ) ) ) return r;
	if( ( r = tsoGetSystemId( ctx ) ) ) return r;
	if( ( r = tsoEnumeratetsoViewConfigs( ctx ) ) ) return r;
	if( ( r = tsoCreateSession( ctx, openglMajor, openglMinor ) ) ) return r;
	if( ( r = tsoCreateStageSpace( ctx ) ) ) return r;

	return 0;
}

int tsoCheck( tsoContext * ctx, XrResult result, const char* str )
{
	XrInstance tsoInstance = ctx?ctx->tsoInstance:0;

	if( XR_SUCCEEDED( result ))
	{
		return 0;
	}

	if( tsoInstance != XR_NULL_HANDLE)
	{
		char resultString[XR_MAX_RESULT_STRING_SIZE];
		xrResultToString(tsoInstance, result, resultString);
		TSOPENXR_ERROR( "%s [%s]\n", str, resultString );
	}
	else
	{
		TSOPENXR_ERROR( "%s\n", str );
	}
	return result;
}

int tsoEnumerateExtensions( tsoContext * ctx )
{
	XrExtensionProperties ** tsoExtensionProps = &ctx->tsoExtensionProps;

	XrResult result;
	uint32_t extensionCount = 0;
	result = xrEnumerateInstanceExtensionProperties(NULL, 0, &extensionCount, NULL);

	if( tsoCheck(NULL, result, "xrEnumerateInstanceExtensionProperties failed"))
		return result;
	
	*tsoExtensionProps = realloc( *tsoExtensionProps, extensionCount * sizeof( XrExtensionProperties ) );
	for( int i = 0; i < extensionCount; i++ )
	{
		(*tsoExtensionProps)[i].type = XR_TYPE_EXTENSION_PROPERTIES;
		(*tsoExtensionProps)[i].next = NULL;
	}

	result = xrEnumerateInstanceExtensionProperties( NULL, extensionCount, &extensionCount, *tsoExtensionProps );
	if( tsoCheck( NULL, result, "xrEnumerateInstanceExtensionProperties failed" ) )
		return result;

#if TSOPENXR_ENABLE_DEBUG
	if( ctx->tsoPrintAll )
	{
		TSOPENXR_INFO("%d extensions:\n", extensionCount);
		for( uint32_t i = 0; i < extensionCount; ++i )
		{
			TSOPENXR_INFO( "	%s\n", (*tsoExtensionProps)[i].extensionName );
		}
	}
#endif

	ctx->tsoNumExtensions = extensionCount;
	return 0;
}

int tsoExtensionSupported( tsoContext * ctx, const char* extensionName)
{
	XrExtensionProperties * extensions = ctx->tsoExtensionProps;
	int extensionsCount = ctx->tsoNumExtensions;
	
	int supported = 0;
	int i;
	for( i = 0; i < extensionsCount; i++ )
	{
		if( !strcmp(extensionName, extensions[i].extensionName))
		{
			supported = 1;
		}
	}
	return supported;
}

int tsoEnumerateLayers( tsoContext * ctx )
{
	XrApiLayerProperties ** tsoLayerProps = &ctx->tsoLayerProps;

	uint32_t layerCount;
	XrResult result = xrEnumerateApiLayerProperties(0, &layerCount, NULL);
	if( tsoCheck(ctx, result, "xrEnumerateApiLayerProperties"))
	{
		return result;
	}

	if ( layerCount == 0 )
	{
		ctx->tsoNumLayerProps = 0;
		*tsoLayerProps = 0;
		return 0;
	}

	*tsoLayerProps = realloc( *tsoLayerProps, layerCount * sizeof( XrApiLayerProperties ) );
	memset( *tsoLayerProps, 0, layerCount * sizeof( XrApiLayerProperties ) );
	for ( uint32_t i = 0; i < layerCount; i++ ) {
		( *tsoLayerProps )[i].type = XR_TYPE_API_LAYER_PROPERTIES;
		( *tsoLayerProps )[i].next = NULL;
	}

	result = xrEnumerateApiLayerProperties( layerCount, &layerCount, *tsoLayerProps );
	if( tsoCheck(ctx, result, "xrEnumerateApiLayerProperties"))
	{
		return result;
	}

#if TSOPENXR_ENABLE_DEBUG
	if( ctx->tsoPrintAll )
	{
		TSOPENXR_INFO("%d layers:\n", layerCount);
		for( uint32_t i = 0; i < layerCount; i++)
		{
			TSOPENXR_INFO("	%s, %s\n", (*tsoLayerProps)[i].layerName, (*tsoLayerProps)[i].description);
		}
	}
#endif
	ctx->tsoNumLayerProps = layerCount;
	return 0;
}

int tsoCreateInstance(tsoContext * ctx, const char * appname )
{
	XrInstance * tsoInstance = &ctx->tsoInstance;

	// create openxr tsoInstance
	XrResult result;
	const char* const enabledExtensions[] = {OPENXR_SELECTED_GRAPHICS_API};
	XrInstanceCreateInfo ici = { XR_TYPE_INSTANCE_CREATE_INFO };
	ici.next = NULL;
	ici.createFlags = 0;
	ici.enabledExtensionCount = 1;
	ici.enabledExtensionNames = enabledExtensions;
	ici.enabledApiLayerCount = 0;
	ici.enabledApiLayerNames = NULL;
	strcpy(ici.applicationInfo.applicationName, appname);
	ici.applicationInfo.engineName[0] = '\0';
	ici.applicationInfo.applicationVersion = 1;
	ici.applicationInfo.engineVersion = 0;
	ici.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

	result = xrCreateInstance(&ici, tsoInstance);
	if (tsoCheck(ctx, result, "xrCreateInstance failed"))
	{
		return result;
	}

#if TSOPENXR_ENABLE_DEBUG
	if ( ctx->tsoPrintAll)
	{
		XrInstanceProperties ip = { XR_TYPE_INSTANCE_PROPERTIES };
		ip.next = NULL;

		result = xrGetInstanceProperties( ctx->tsoInstance, &ip );
		if( tsoCheck( ctx, result, "xrGetInstanceProperties failed" ) )
		{
			return result;
		}

		TSOPENXR_INFO("Runtime Name: %s\n", ip.runtimeName);
		TSOPENXR_INFO("Runtime Version: %d.%d.%d\n",
		XR_VERSION_MAJOR(ip.runtimeVersion),
		XR_VERSION_MINOR(ip.runtimeVersion),
		XR_VERSION_PATCH(ip.runtimeVersion));
	}
#endif
	return 0;
}


int tsoGetSystemId( tsoContext * ctx )
{
	XrInstance tsoInstance = ctx->tsoInstance;
	
	XrResult result;
	XrSystemGetInfo sgi = { XR_TYPE_SYSTEM_GET_INFO };
	sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	sgi.next = NULL;
	result = xrGetSystem(tsoInstance, &sgi, &ctx->tsoSystemId);
	if (tsoCheck(ctx, result, "xrGetSystemFailed"))
	{
		return result;
	}
#if TSOPENXR_ENABLE_DEBUG
	if ( ctx->tsoPrintAll)
	{
		XrSystemProperties sp = { XR_TYPE_SYSTEM_PROPERTIES };

		result = xrGetSystemProperties(tsoInstance, ctx->tsoSystemId, &sp);
		if (tsoCheck(ctx, result, "xrGetSystemProperties failed"))
		{
			return result;
		}

		TSOPENXR_INFO("System properties for system \"%s\":\n", sp.systemName);
		TSOPENXR_INFO("	maxLayerCount: %d\n", sp.graphicsProperties.maxLayerCount);
		TSOPENXR_INFO("	maxSwapChainImageHeight: %d\n", sp.graphicsProperties.maxSwapchainImageHeight);
		TSOPENXR_INFO("	maxSwapChainImageWidth: %d\n", sp.graphicsProperties.maxSwapchainImageWidth);
		TSOPENXR_INFO("	Orientation Tracking: %s\n", sp.trackingProperties.orientationTracking ? "true" : "false");
		TSOPENXR_INFO("	Position Tracking: %s\n", sp.trackingProperties.positionTracking ? "true" : "false");
	}
#endif

	return 0;
}

int tsoEnumeratetsoViewConfigs( tsoContext * ctx )
{
	
	XrInstance tsoInstance = ctx->tsoInstance;
	XrSystemId tsoSystemId = ctx->tsoSystemId;
	
	XrResult result;
	uint32_t viewCount = ctx->tsoNumViewConfigs;
	XrViewConfigurationType stereoViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	result = xrEnumerateViewConfigurationViews(tsoInstance, tsoSystemId, stereoViewConfigType, viewCount, &viewCount, ctx->tsoViewConfigs);
	if (tsoCheck(ctx, result, "xrEnumerateViewConfigurationViews"))
	{
		return result;
	}

	XrViewConfigurationView * tsoViewConfigs = ctx->tsoViewConfigs;

	if( ctx->tsoNumViewConfigs != viewCount )
	{
		tsoViewConfigs = ctx->tsoViewConfigs = realloc( ctx->tsoViewConfigs, viewCount * sizeof(XrViewConfigurationView) );
		for (uint32_t i = 0; i < viewCount; i++)
		{
			tsoViewConfigs[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
			tsoViewConfigs[i].next = NULL;
		}

		result = xrEnumerateViewConfigurationViews(tsoInstance, tsoSystemId, stereoViewConfigType, viewCount, &viewCount, tsoViewConfigs);
		if (tsoCheck(ctx, result, "xrEnumerateViewConfigurationViews"))
		{
			return result;
		}
	}

#if TSOPENXR_ENABLE_DEBUG
	if (ctx->tsoPrintAll && ctx->tsoNumViewConfigs != viewCount )
	{
		TSOPENXR_INFO("%d tsoViewConfigs:\n", viewCount);
		for (uint32_t i = 0; i < viewCount; i++)
		{
			TSOPENXR_INFO("	tsoViewConfigs[%d]:\n", i);
			TSOPENXR_INFO("		recommendedImageRectWidth: %d\n", tsoViewConfigs[i].recommendedImageRectWidth);
			TSOPENXR_INFO("		maxImageRectWidth: %d\n", tsoViewConfigs[i].maxImageRectWidth);
			TSOPENXR_INFO("		recommendedImageRectHeight: %d\n", tsoViewConfigs[i].recommendedImageRectHeight);
			TSOPENXR_INFO("		maxImageRectHeight: %d\n", tsoViewConfigs[i].maxImageRectHeight);
			TSOPENXR_INFO("		recommendedSwapchainSampleCount: %d\n", tsoViewConfigs[i].recommendedSwapchainSampleCount);
			TSOPENXR_INFO("		maxSwapchainSampleCount: %d\n", tsoViewConfigs[i].maxSwapchainSampleCount);
		}
	}
#endif

	ctx->tsoNumViewConfigs = viewCount;
	return 0;
}

int tsoCreateSession( tsoContext * ctx, uint32_t openglMajor, uint32_t openglMinor )
{
	XrInstance tsoInstance = ctx->tsoInstance;
	XrSystemId tsoSystemId = ctx->tsoSystemId;
	
	XrResult result;

	// check if opengl version is sufficient.
	{
#ifndef ANDROID
		XrGraphicsRequirementsOpenGLKHR reqs;
		reqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
		reqs.next = NULL;
		PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = NULL;
		result = xrGetInstanceProcAddr(tsoInstance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction *)&pfnGetOpenGLGraphicsRequirementsKHR);
		if (tsoCheck(ctx, result, "xrGetInstanceProcAddr"))
		{
			return result;
		}

		result = pfnGetOpenGLGraphicsRequirementsKHR(tsoInstance, tsoSystemId, &reqs);
		if (tsoCheck(ctx, result, "GetOpenGLGraphicsRequirementsPKR"))
		{
			return result;
		}

		const XrVersion desiredApiVersion = XR_MAKE_VERSION(openglMajor, openglMinor, 0);
#if TSOPENXR_ENABLE_DEBUG
		if (ctx->tsoPrintAll)
		{
			TSOPENXR_INFO("current OpenGL version: %d.%d.%d\n", XR_VERSION_MAJOR(desiredApiVersion),
				XR_VERSION_MINOR(desiredApiVersion), XR_VERSION_PATCH(desiredApiVersion));
			TSOPENXR_INFO("minimum OpenGL version: %d.%d.%d\n", XR_VERSION_MAJOR(reqs.minApiVersionSupported),
				XR_VERSION_MINOR(reqs.minApiVersionSupported), XR_VERSION_PATCH(reqs.minApiVersionSupported));
		}
#endif
		if (reqs.minApiVersionSupported > desiredApiVersion)
		{
			TSOPENXR_INFO("Runtime does not support desired Graphics API and/or version\n");
			return result;
		}
#endif
	}


#ifdef ANDROID

	// Get the graphics requirements.
	PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
	xrGetInstanceProcAddr(
		ctx->tsoInstance,
		"xrGetOpenGLESGraphicsRequirementsKHR",
		(PFN_xrVoidFunction*)(&pfnGetOpenGLESGraphicsRequirementsKHR));

	XrGraphicsRequirementsOpenGLESKHR graphicsRequirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR };
	result = pfnGetOpenGLESGraphicsRequirementsKHR(ctx->tsoInstance, ctx->tsoSystemId, &graphicsRequirements);
	if (tsoCheck(ctx, result, "pfnGetOpenGLESGraphicsRequirementsKHR"))
	{
		return result;
	}

#if TSOPENXR_ENABLE_DEBUG
	if (ctx->tsoPrintAll)
	{
		const XrVersion desiredApiVersion = XR_MAKE_VERSION(openglMajor, openglMinor, 0);
		TSOPENXR_INFO("current OpenGL version: %d.%d.%d\n", XR_VERSION_MAJOR(desiredApiVersion),
			XR_VERSION_MINOR(desiredApiVersion), XR_VERSION_PATCH(desiredApiVersion));
		TSOPENXR_INFO("minimum OpenGL version: %d.%d.%d\n", XR_VERSION_MAJOR(graphicsRequirements.minApiVersionSupported),
			XR_VERSION_MINOR(graphicsRequirements.minApiVersionSupported), XR_VERSION_PATCH(graphicsRequirements.minApiVersionSupported));
	}
#endif

	// Check the graphics requirements.
	const XrVersion eglVersion = XR_MAKE_VERSION(openglMajor, openglMinor, 0);
	if (eglVersion < graphicsRequirements.minApiVersionSupported ||
		eglVersion > graphicsRequirements.maxApiVersionSupported) {
		TSOPENXR_ERROR("GLES version %d.%d not supported", openglMajor, openglMinor);
		return -1;
	}
#endif


	// Create the OpenXR Session.

#ifdef XR_USE_PLATFORM_WIN32
	XrGraphicsBindingOpenGLWin32KHR glBinding = { XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR  };
	glBinding.hDC = wglGetCurrentDC();
	glBinding.hGLRC = wglGetCurrentContext();
#elif defined( XR_USE_PLATFORM_ANDROID )
	XrGraphicsBindingOpenGLESAndroidKHR glBinding = { XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR };
	glBinding.display = egl_display;
	glBinding.config = egl_config;
	glBinding.context = egl_context;
#elif defined( XR_USE_PLATFORM_XLIB )
	XrGraphicsBindingOpenGLXlibKHR glBinding = { XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR };
	glBinding.xDisplay = CNFGDisplay;
	glBinding.visualid = CNFGVisualID;
	glBinding.glxFBConfig = CNFGGLXFBConfig;
	glBinding.glxDrawable = CNFGWindow;
	glBinding.glxContext = CNFGCtx;
#endif

	XrSessionCreateInfo sci = { XR_TYPE_SESSION_CREATE_INFO };
	sci.next = &glBinding;
	sci.systemId = tsoSystemId;

	result = xrCreateSession(ctx->tsoInstance, &sci, &ctx->tsoSession);
	if (tsoCheck(ctx, result, "xrCreateSession"))
	{
		return result;
	}
	
	return 0;
}

int tsoInitAction( tsoContext * ctx, XrActionType type, const char * action_name, const char * localized_name, XrAction * action )
{
	XrResult result;
	XrActionCreateInfo aci;
	aci.type = XR_TYPE_ACTION_CREATE_INFO;
	aci.next = NULL;
	aci.actionType = type;
	strcpy(aci.actionName, action_name );
	strcpy(aci.localizedActionName, localized_name );
	aci.countSubactionPaths = 2;
	aci.subactionPaths = ctx->handPath;
	result = xrCreateAction(ctx->tsoActionSet, &aci, action );
	if( result )
	{
		char actionError[1024];
		snprintf( actionError, sizeof(actionError)-1, "xrCreateAction %s", action_name );
		tsoCheck(ctx, result, actionError);
	}
	return result;
}

int tsoDefaultCreateActions( tsoContext * ctx )
{
	XrInstance tsoInstance = ctx->tsoInstance;
	XrSession tsoSession = ctx->tsoSession;
	XrResult result;

	// create action set
	XrActionSetCreateInfo asci;
	asci.type = XR_TYPE_ACTION_SET_CREATE_INFO;
	asci.next = NULL;
	strcpy(asci.actionSetName, "gameplay");
	strcpy(asci.localizedActionSetName, "Gameplay");
	asci.priority = 0;
	result = xrCreateActionSet(tsoInstance, &asci, &ctx->tsoActionSet);
	if (tsoCheck(ctx, result, "xrCreateActionSet XR_TYPE_ACTION_SET_CREATE_INFO"))
	{
		return result;
	}

	xrStringToPath(tsoInstance, "/user/hand/left", &ctx->handPath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right", &ctx->handPath[1]);
	if (tsoCheck(ctx, result, "xrStringToPath"))
	{
		return result;
	}

	tsoInitAction( ctx, XR_ACTION_TYPE_FLOAT_INPUT, "grab_object", "Grab Object", &ctx->grabAction );
	tsoInitAction( ctx, XR_ACTION_TYPE_FLOAT_INPUT, "trigger_action", "Trigger Action", &ctx->triggerAction );
	tsoInitAction( ctx, XR_ACTION_TYPE_BOOLEAN_INPUT, "trigger_action_click", "Trigger Action Click", &ctx->triggerActionClick );
	tsoInitAction( ctx, XR_ACTION_TYPE_POSE_INPUT, "hand_pose", "Hand Pose", &ctx->poseAction );
	tsoInitAction( ctx, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_hand", "Vibrate Hand", &ctx->vibrateAction );
	tsoInitAction( ctx, XR_ACTION_TYPE_BOOLEAN_INPUT, "quit_session", "Menu Button", &ctx->menuAction );

	xrStringToPath(tsoInstance, "/user/hand/left/input/select/click", &ctx->selectPath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/input/select/click", &ctx->selectPath[1]);
	xrStringToPath(tsoInstance, "/user/hand/left/input/squeeze/value",  &ctx->squeezeValuePath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/input/squeeze/value", &ctx->squeezeValuePath[1]);
	xrStringToPath(tsoInstance, "/user/hand/left/input/squeeze/click",  &ctx->squeezeClickPath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/input/squeeze/click", &ctx->squeezeClickPath[1]);
	xrStringToPath(tsoInstance, "/user/hand/left/input/trigger/value",  &ctx->triggerValuePath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/input/trigger/value", &ctx->triggerValuePath[1]);
	xrStringToPath(tsoInstance, "/user/hand/left/input/trigger/click",  &ctx->triggerClickPath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/input/trigger/click", &ctx->triggerClickPath[1]);
	xrStringToPath(tsoInstance, "/user/hand/left/input/grip/pose", &ctx->posePath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/input/grip/pose", &ctx->posePath[1]);
	xrStringToPath(tsoInstance, "/user/hand/left/output/haptic", &ctx->hapticPath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/output/haptic", &ctx->hapticPath[1]);
	xrStringToPath(tsoInstance, "/user/hand/left/input/menu/click", &ctx->menuClickPath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/input/menu/click", &ctx->menuClickPath[1]);
	if (tsoCheck(ctx, result, "xrStringToPath"))
	{
		return result;
	}

	// KHR Simple (no need on most runtimes; it's rough to map)
	if( 0 ){
		XrPath interactionProfilePath = XR_NULL_PATH;
		xrStringToPath(tsoInstance, "/interaction_profiles/khr/simple_controller", &interactionProfilePath);
		XrActionSuggestedBinding bindings[] = {
			{ctx->grabAction, ctx->selectPath[0]},
			{ctx->grabAction, ctx->selectPath[1]},
			{ctx->poseAction, ctx->posePath[0]},
			{ctx->poseAction, ctx->posePath[1]},
			{ctx->menuAction, ctx->menuClickPath[0]},
			{ctx->menuAction, ctx->menuClickPath[1]},
			{ctx->vibrateAction, ctx->hapticPath[0]},
			{ctx->vibrateAction, ctx->hapticPath[1]}
		};

		XrInteractionProfileSuggestedBinding suggestedBindings;
		suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		suggestedBindings.next = NULL;
		suggestedBindings.interactionProfile = interactionProfilePath;
		suggestedBindings.suggestedBindings = bindings;
		suggestedBindings.countSuggestedBindings = sizeof( bindings ) / sizeof( bindings[0] );
		result = xrSuggestInteractionProfileBindings(tsoInstance, &suggestedBindings);
		if (tsoCheck(ctx, result, "xrSuggestInteractionProfileBindings"))
		{
			return result;
		}
	}

	// Oculus touch (Auto-remaps well to Index)
	{
		XrPath interactionProfilePath = XR_NULL_PATH;
		xrStringToPath(tsoInstance, "/interaction_profiles/oculus/touch_controller", &interactionProfilePath);
		XrActionSuggestedBinding bindings[] = {
			{ctx->grabAction, ctx->squeezeValuePath[0]},
			{ctx->grabAction, ctx->squeezeValuePath[1]},
			{ctx->triggerAction, ctx->triggerValuePath[0]},
			{ctx->triggerAction, ctx->triggerValuePath[1]},
			{ctx->triggerActionClick, ctx->triggerValuePath[0]},
			{ctx->triggerActionClick, ctx->triggerValuePath[1]},
			{ctx->poseAction, ctx->posePath[0]},
			{ctx->poseAction, ctx->posePath[1]},
			{ctx->menuAction, ctx->menuClickPath[0]},
			//{menuAction, menuClickPath[1]},  // no menu button on right controller?
			{ctx->vibrateAction, ctx->hapticPath[0]},
			{ctx->vibrateAction, ctx->hapticPath[1]}
		};

		XrInteractionProfileSuggestedBinding suggestedBindings;
		suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		suggestedBindings.next = NULL;
		suggestedBindings.interactionProfile = interactionProfilePath;
		suggestedBindings.suggestedBindings = bindings;
		suggestedBindings.countSuggestedBindings = sizeof( bindings ) / sizeof( bindings[0] );
		result = xrSuggestInteractionProfileBindings(tsoInstance, &suggestedBindings);
		if (tsoCheck(ctx, result, "xrSuggestInteractionProfileBindings (oculus)"))
		{
			return result;
		}
	}
	
	XrActionSpaceCreateInfo aspci = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
	aspci.action = ctx->poseAction;
	XrPosef identity = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f} };
	aspci.poseInActionSpace = identity;

	aspci.subactionPath = ctx->handPath[0];
	result = xrCreateActionSpace(ctx->tsoSession, &aspci, &ctx->tsoHandSpace[0]);
	if (tsoCheck(ctx, result, "xrCreateActionSpace"))
	{
		return result;
	}

	aspci.subactionPath = ctx->handPath[1];
	result = xrCreateActionSpace(ctx->tsoSession, &aspci, &ctx->tsoHandSpace[1]);
	if (tsoCheck(ctx, result, "xrCreateActionSpace"))
	{
		return result;
	}

	XrSessionActionSetsAttachInfo sasai;
	sasai.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
	sasai.next = NULL;
	sasai.countActionSets = 1;
	sasai.actionSets = &ctx->tsoActionSet;
	result = xrAttachSessionActionSets(tsoSession, &sasai);
	if (tsoCheck(ctx, result, "xrSessionActionSetsAttachInfo"))
	{
		return result;
	}

	return 0;
}


int tsoCreateStageSpace( tsoContext * ctx )
{
	XrSession tsoSession = ctx->tsoSession;
	XrResult result;
	
#if TSOPENXR_ENABLE_DEBUG
	if (ctx->tsoPrintAll)
	{
		uint32_t referenceSpacesCount;
		result = xrEnumerateReferenceSpaces(tsoSession, 0, &referenceSpacesCount, NULL);
		if (tsoCheck(ctx, result, "xrEnumerateReferenceSpaces"))
		{
			return result;
		}

		XrReferenceSpaceType * referenceSpaces = malloc( referenceSpacesCount * sizeof(XrReferenceSpaceType) );
		int i;
		for( i = 0; i < referenceSpacesCount; i++ )
			referenceSpaces[i] = XR_REFERENCE_SPACE_TYPE_VIEW;
		result = xrEnumerateReferenceSpaces(tsoSession, referenceSpacesCount, &referenceSpacesCount, referenceSpaces );
		if (tsoCheck(ctx, result, "xrEnumerateReferenceSpaces"))
		{
			return result;
		}

		TSOPENXR_INFO("referenceSpaces:\n");
		for (uint32_t i = 0; i < referenceSpacesCount; i++)
		{
			switch (referenceSpaces[i])
			{
			case XR_REFERENCE_SPACE_TYPE_VIEW:
				TSOPENXR_INFO("	XR_REFERENCE_SPACE_TYPE_VIEW\n");
				break;
			case XR_REFERENCE_SPACE_TYPE_LOCAL:
				TSOPENXR_INFO("	XR_REFERENCE_SPACE_TYPE_LOCAL\n");
				break;
			case XR_REFERENCE_SPACE_TYPE_STAGE:
				TSOPENXR_INFO("	XR_REFERENCE_SPACE_TYPE_STAGE\n");
				break;
			default:
				TSOPENXR_INFO("	XR_REFERENCE_SPACE_TYPE_%d\n", referenceSpaces[i]);
				break;
			}
		}
	}
#endif
	XrPosef identityPose = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f} };

	XrReferenceSpaceCreateInfo rsci;
	rsci.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	rsci.next = NULL;
	rsci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
	rsci.poseInReferenceSpace = identityPose;
	result = xrCreateReferenceSpace(tsoSession, &rsci, &ctx->tsoStageSpace);
	if (tsoCheck(ctx, result, "xrCreateReferenceSpace"))
	{
		return result;
	}

	return 0;
}



int tsoCreateSwapchains( tsoContext * ctx )
{
	XrSession tsoSession = ctx->tsoSession;
	XrViewConfigurationView * tsoViewConfigs = ctx->tsoViewConfigs;
	int tsoNumViewConfigs = ctx->tsoNumViewConfigs;
	tsoSwapchainInfo ** tsoSwapchains = &ctx->tsoSwapchains; // Will allocate to tsoNumViewConfigs
	XrSwapchainImageOpenGLKHR *** tsoSwapchainImages = &ctx->tsoSwapchainImages;
	uint32_t ** tsoSwapchainLengths = &ctx->tsoSwapchainLengths; // Actually array of pointers to pointers
					  
	int i;
	XrResult result;
	uint32_t swapchainFormatCount;
	result = xrEnumerateSwapchainFormats(tsoSession, 0, &swapchainFormatCount, NULL);
	if (tsoCheck(ctx, result, "xrEnumerateSwapchainFormats"))
	{
		return result;
	}

	int64_t * swapchainFormats = alloca(swapchainFormatCount * sizeof( int64_t ));
	result = xrEnumerateSwapchainFormats(tsoSession, swapchainFormatCount, &swapchainFormatCount, swapchainFormats);
	if (tsoCheck(ctx, result, "xrEnumerateSwapchainFormats"))
	{
		return result;
	}
	
	int selfmt = 0;
	int isdefault = 1;

	for( i = 0; i < swapchainFormatCount; i++ )
	{
		// Prefer SRGB8_ALPHA8
		if( swapchainFormats[i] == GL_SRGB8_ALPHA8 )
		{
			isdefault = 0;
			selfmt = i;
		}
		if( swapchainFormats[i] == GL_SRGB8 && isdefault )
		{
			isdefault = 0;
			selfmt = i;
		}
	}
	
#if TSOPENXR_ENABLE_DEBUG
	if( ctx->tsoPrintAll )
	{
		TSOPENXR_INFO( "Formats:" );
		for( i = 0; i < swapchainFormatCount; i++ )
		{
			TSOPENXR_INFO( "%c%04x%c", ( i == selfmt )?'*':' ', (int)swapchainFormats[i], (i == swapchainFormatCount-1)?'\n':',' );
		} 
	}
#endif

	if( *tsoSwapchains )
	{
		tsoDestroySwapchains( ctx );
	}

	// For now we just pick the default one.
	int64_t swapchainFormatToUse = swapchainFormats[selfmt];

	int numSwapchainsPerFrame = ctx->numSwapchainsPerFrame = (ctx->flags & TSO_DOUBLEWIDE)?1:tsoNumViewConfigs;

	*tsoSwapchains = realloc( *tsoSwapchains, numSwapchainsPerFrame * sizeof( tsoSwapchainInfo ) );
	*tsoSwapchainLengths = realloc( *tsoSwapchainLengths, numSwapchainsPerFrame * sizeof( uint32_t ) );
	for (uint32_t i = 0; i < numSwapchainsPerFrame; i++)
	{
		XrSwapchainCreateInfo sci = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
		sci.createFlags = 0;
		sci.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		sci.format = swapchainFormatToUse;
		sci.sampleCount = 1;
		sci.width = tsoViewConfigs[i].recommendedImageRectWidth * ((ctx->flags & TSO_DOUBLEWIDE)?tsoNumViewConfigs:1);
		sci.height = tsoViewConfigs[i].recommendedImageRectHeight;
		sci.faceCount = 1;
		sci.arraySize = 1;
		sci.mipCount = 1;

		XrSwapchain swapchainHandle;
		result = xrCreateSwapchain(tsoSession, &sci, &swapchainHandle);
		if (tsoCheck(ctx, result, "xrCreateSwapchain"))
		{
			return result;
		}

		(*tsoSwapchains)[i].handle = swapchainHandle;
		(*tsoSwapchains)[i].width = sci.width;
		(*tsoSwapchains)[i].height = sci.height;

		result = xrEnumerateSwapchainImages((*tsoSwapchains)[i].handle, 0, &(*tsoSwapchainLengths)[i], NULL);
		if (tsoCheck(ctx, result, "xrEnumerateSwapchainImages [view]"))
		{
			return result;
		}
	}

	*tsoSwapchainImages = realloc( *tsoSwapchainImages, numSwapchainsPerFrame * sizeof( XrSwapchainImageOpenGLKHR * ) ); 
	for (uint32_t i = 0; i < numSwapchainsPerFrame; i++)
	{
		(*tsoSwapchainImages)[i] = malloc( (*tsoSwapchainLengths)[i] * sizeof(XrSwapchainImageOpenGLKHR) );
		for (uint32_t j = 0; j < (*tsoSwapchainLengths)[i]; j++)
		{
#ifdef ANDROID
			(*tsoSwapchainImages)[i][j].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
#else
			(*tsoSwapchainImages)[i][j].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
#endif
			(*tsoSwapchainImages)[i][j].next = NULL;
		}

		result = xrEnumerateSwapchainImages((*tsoSwapchains)[i].handle, (*tsoSwapchainLengths)[i], &(*tsoSwapchainLengths)[i],
											(XrSwapchainImageBaseHeader*)((*tsoSwapchainImages)[i]));

		if (tsoCheck(ctx, result, "xrEnumerateSwapchainImages [final]"))
		{
			return result;
		}
	}
	
	return 0;
}


int tsoBeginSession( tsoContext * ctx )
{
	XrResult result;
	XrViewConfigurationType stereoViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	XrSessionBeginInfo sbi;
	sbi.type = XR_TYPE_SESSION_BEGIN_INFO;
	sbi.next = NULL;
	sbi.primaryViewConfigurationType = stereoViewConfigType;

	result = xrBeginSession(ctx->tsoSession, &sbi);
	if (tsoCheck(ctx, result, "xrBeginSession"))
	{
		return result;
	}

	return 0;
}


int tsoSyncInput( tsoContext * ctx )
{
	XrSession tsoSession = ctx->tsoSession;
	XrActionSet tsoActionSet = ctx->tsoActionSet;
	XrResult result;

	// syncInput
	XrActiveActionSet aas;
	aas.actionSet = tsoActionSet;
	aas.subactionPath = XR_NULL_PATH;
	XrActionsSyncInfo asi;
	asi.type = XR_TYPE_ACTIONS_SYNC_INFO;
	asi.next = NULL;
	asi.countActiveActionSets = 1;
	asi.activeActionSets = &aas;
	result = xrSyncActions(tsoSession, &asi);
	if (tsoCheck(ctx, result, "xrSyncActions"))
	{
		return result;
	}

	// Todo's from original author.
	// AJT: TODO: xrGetActionStateFloat()
	// AJT: TODO: xrGetActionStatePose()

	return 0;
}

int tsoRenderFrame( tsoContext * ctx )
{
	XrSession tsoSession = ctx->tsoSession;
	int tsoNumViewConfigs = ctx->tsoNumViewConfigs;
	XrSpace tsoStageSpace = ctx->tsoStageSpace;

	XrFrameState fs;
	fs.type = XR_TYPE_FRAME_STATE;
	fs.next = NULL;

	XrFrameWaitInfo fwi;
	fwi.type = XR_TYPE_FRAME_WAIT_INFO;
	fwi.next = NULL;

	XrResult result = xrWaitFrame(tsoSession, &fwi, &fs);
	if (tsoCheck(ctx, result, "xrWaitFrame"))
	{
		return result;
	}

	XrFrameBeginInfo fbi;
	fbi.type = XR_TYPE_FRAME_BEGIN_INFO;
	fbi.next = NULL;
	result = xrBeginFrame(tsoSession, &fbi);
	if (tsoCheck(ctx, result, "xrBeginFrame"))
	{
		return result;
	}

	// Potentially resize.
	tsoEnumeratetsoViewConfigs( ctx );
	
	// Originally written this way to allow for  || ctx->tsoViewConfigs[0].recommendedImageRectWidth != ctx->tsoSwapchains[0].width  ... But this doesn't work in any current runtimes.
	if( !ctx->tsoNumViewConfigs || !ctx->tsoSwapchains )
	{
		if ( ( result = tsoCreateSwapchains( ctx ) ) ) return result;
	}		

	int layerCount = 0;
	XrCompositionLayerProjection layer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
	layer.layerFlags = 0; //XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
	layer.next = NULL;
	layer.space = tsoStageSpace;

	const XrCompositionLayerBaseHeader * layers[1] = { (XrCompositionLayerBaseHeader *)&layer };

	XrView * views = alloca( sizeof( XrView) * tsoNumViewConfigs );
	for (size_t i = 0; i < tsoNumViewConfigs; i++)
	{
		views[i].type = XR_TYPE_VIEW;
		views[i].next = NULL;
	}
	
	uint32_t viewCountOutput;
	XrViewState viewState = { XR_TYPE_VIEW_STATE };

	XrViewLocateInfo vli;
	vli.type = XR_TYPE_VIEW_LOCATE_INFO;
	vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	ctx->tsoPredictedDisplayTime = vli.displayTime = fs.predictedDisplayTime;
	vli.space = tsoStageSpace;
	result = xrLocateViews( tsoSession, &vli, &viewState, tsoNumViewConfigs, &viewCountOutput, views );
	if (tsoCheck(ctx, result, "xrLocateViews"))
	{
		return result;
	}

	XrCompositionLayerProjectionView *projectionLayerViews = alloca( viewCountOutput * sizeof( XrCompositionLayerProjectionView ) );
	memset( projectionLayerViews, 0, sizeof( XrCompositionLayerProjectionView ) * viewCountOutput );

	int i;
	int viewCounts = (ctx->flags & TSO_DOUBLEWIDE)?1:viewCountOutput;
	for( i = 0; i < viewCountOutput; i++ )
	{
		// Each view has a separate swapchain which is acquired, rendered to, and released.
		XrCompositionLayerProjectionView * layerView = projectionLayerViews + i;
		layerView->type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		layerView->pose = views[i].pose;
		layerView->fov = views[i].fov;
		
		if( ctx->flags & TSO_DOUBLEWIDE )
		{
			const tsoSwapchainInfo * viewSwapchain = ctx->tsoSwapchains;
			int individualWidth = viewSwapchain->width / viewCountOutput;
			layerView->subImage.swapchain = viewSwapchain->handle;
			layerView->subImage.imageRect.offset.x = i*individualWidth;
			layerView->subImage.imageRect.offset.y = 0;
			layerView->subImage.imageRect.extent.width = individualWidth;
			layerView->subImage.imageRect.extent.height = viewSwapchain->height;
			layerView->subImage.imageArrayIndex = 0;
		}
		else
		{
			const tsoSwapchainInfo * viewSwapchain = ctx->tsoSwapchains + i;
			layerView->subImage.swapchain = viewSwapchain->handle;
			layerView->subImage.imageRect.offset.x = 0;
			layerView->subImage.imageRect.offset.y = 0;
			layerView->subImage.imageRect.extent.width = viewSwapchain->width;
			layerView->subImage.imageRect.extent.height = viewSwapchain->height;
			layerView->subImage.imageArrayIndex = 0;
		}
	}
		
	// We only support up to 1 layer.
	if (fs.shouldRender == XR_TRUE && XR_UNQUALIFIED_SUCCESS(result))
	{
		if (ctx->tsoRenderLayer(ctx, fs.predictedDisplayTime, projectionLayerViews, viewCountOutput) == 0)
		{
			layer.viewCount = viewCountOutput;
			layer.views = projectionLayerViews;
			layerCount = 1;
		}
	}

	XrFrameEndInfo fei = { XR_TYPE_FRAME_END_INFO };
	fei.displayTime = fs.predictedDisplayTime;
	fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	fei.layerCount = layerCount;
	fei.layers = layers;

	result = xrEndFrame(tsoSession, &fei);
	if (tsoCheck(ctx, result, "xrEndFrame"))
	{
		return result;
	}

	return 0;
}



int tsoHandleLoop( tsoContext * ctx )
{
	XrEventDataBuffer xrEvent = { XR_TYPE_EVENT_DATA_BUFFER };
	XrResult result = xrPollEvent(ctx->tsoInstance, &xrEvent);

	if (result == XR_SUCCESS)
	{
		switch (xrEvent.type)
		{
		case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
			// Receiving the XrEventDataInstanceLossPending event structure indicates that the application is about to lose the indicated XrInstance at the indicated lossTime in the future.
			// The application should call xrDestroyInstance and relinquish any tsoInstance-specific resources.
			// This typically occurs to make way for a replacement of the underlying runtime, such as via a software update.
			TSOPENXR_INFO("xrEvent: XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING\n");
			break;
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
		{
			// Receiving the XrEventDataSessionStateChanged event structure indicates that the application has changed lifecycle stat.e
			TSOPENXR_INFO("xrEvent: XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED -> ");
			XrEventDataSessionStateChanged* ssc = (XrEventDataSessionStateChanged*)&xrEvent;
			ctx->tsoXRState = ssc->state;
			switch (ctx->tsoXRState)
			{
			case XR_SESSION_STATE_IDLE:
				// The initial state after calling xrCreateSession or returned to after calling xrEndSession.
				TSOPENXR_INFO("XR_SESSION_STATE_IDLE\n");
				break;
			case XR_SESSION_STATE_READY:
				// The application is ready to call xrBeginSession and sync its frame loop with the runtime.
				TSOPENXR_INFO("XR_SESSION_STATE_READY\n");
				if( ( result = tsoBeginSession( ctx ) ) )
				{
					return result;
				}
				ctx->tsoSessionReady = 1;
				break;
			case XR_SESSION_STATE_SYNCHRONIZED:
				// The application has synced its frame loop with the runtime but is not visible to the user.
				TSOPENXR_INFO("XR_SESSION_STATE_SYNCHRONIZED\n");
				break;
			case XR_SESSION_STATE_VISIBLE:
				// The application has synced its frame loop with the runtime and is visible to the user but cannot receive XR input.
				TSOPENXR_INFO("XR_SESSION_STATE_VISIBLE\n");
				break;
			case XR_SESSION_STATE_FOCUSED:
				// The application has synced its frame loop with the runtime, is visible to the user and can receive XR input.
				TSOPENXR_INFO("XR_SESSION_STATE_FOCUSED\n");
				break;
			case XR_SESSION_STATE_STOPPING:
				// The application should exit its frame loop and call xrEndSession.
				TSOPENXR_INFO("XR_SESSION_STATE_STOPPING\n");
				break;
			case XR_SESSION_STATE_LOSS_PENDING:
				TSOPENXR_INFO("XR_SESSION_STATE_LOSS_PENDING\n");
				// The tsoSession is in the process of being lost. The application should destroy the current tsoSession and can optionally recreate it.
				break;
			case XR_SESSION_STATE_EXITING:
				TSOPENXR_INFO("XR_SESSION_STATE_EXITING\n");
				// The application should end its XR experience and not automatically restart it.
				break;
			default:
				TSOPENXR_INFO("XR_SESSION_STATE_??? %d\n", (int)ctx->tsoXRState);
				break;
			}
			break;
		}
		case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
			// The XrEventDataReferenceSpaceChangePending event is sent to the application to notify it that the origin (and perhaps the bounds) of a reference space is changing.
			TSOPENXR_INFO("XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING\n");
			break;
		case XR_TYPE_EVENT_DATA_EVENTS_LOST:
			// Receiving the XrEventDataEventsLost event structure indicates that the event queue overflowed and some events were removed at the position within the queue at which this event was found.
			TSOPENXR_INFO("xrEvent: XR_TYPE_EVENT_DATA_EVENTS_LOST\n");
			break;
		case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
			// The XrEventDataInteractionProfileChanged event is sent to the application to notify it that the active input form factor for one or more top level user paths has changed.:
			TSOPENXR_INFO("XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED\n");
			break;
		default:
			TSOPENXR_INFO("Unhandled event type %d\n", xrEvent.type);
			break;
		}
	}
	return 0;
}

int tsoAcquireSwapchain( tsoContext * ctx, int swapchainNumber, uint32_t * swapchainImageIndex )
{
	const tsoSwapchainInfo * viewSwapchain = ctx->tsoSwapchains + swapchainNumber;

	XrSwapchainImageAcquireInfo ai = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
	XrResult result = xrAcquireSwapchainImage(viewSwapchain->handle, &ai, swapchainImageIndex);
	if (tsoCheck(ctx, result, "xrAquireSwapchainImage"))
	{
		return result;
	}

	XrSwapchainImageWaitInfo wi = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
	wi.timeout = XR_INFINITE_DURATION;
	result = xrWaitSwapchainImage(viewSwapchain->handle, &wi);
	if (tsoCheck(ctx, result, "xrWaitSwapchainImage"))
	{
		return result;
	}
	
	return 0;
}

int tsoReleaseSwapchain( tsoContext * ctx, int swapchainNumber )
{
	const tsoSwapchainInfo * viewSwapchain = ctx->tsoSwapchains + swapchainNumber;

	XrResult result;
	XrSwapchainImageReleaseInfo ri = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
	result = xrReleaseSwapchainImage( viewSwapchain->handle, &ri );
	if (tsoCheck(ctx, result, "xrReleaseSwapchainImage"))
	{
		return result;
	}
	return 0;
}

int tsoDestroySwapchains( tsoContext * ctx )
{
	int i;
	XrResult result;
	for( i = 0; i < ctx->tsoNumViewConfigs; i++ )
	{
		result = xrDestroySwapchain( ctx->tsoSwapchains[i].handle);
		if( tsoCheck(ctx, result, "xrDestroySwapchain") ) return result;
	}
	free( ctx->tsoSwapchains );
	ctx->tsoSwapchains = 0;
	return 0;
}

int tsoTeardown( tsoContext * ctx )
{
	XrResult result;
	int ret = 0;
	tsoDestroySwapchains( ctx );

	result = xrDestroySpace(ctx->tsoStageSpace);
	tsoCheck(ctx, result, "xrDestroySpace");
	ret |= result;

	result = xrEndSession(ctx->tsoSession);
	tsoCheck(ctx, result, "xrEndSession");
	ret |= result;

	result = xrDestroySession(ctx->tsoSession);
	tsoCheck(ctx, result, "xrDestroySession");
	ret |= result;

	result = xrDestroyInstance(ctx->tsoInstance);
	ctx->tsoInstance = 0;
	tsoCheck(ctx, result, "xrDestroyInstance");
	ret |= result;
	
	memset( ctx, 0, sizeof( *ctx ) );
	return ret;
}


// Utility functions from copied from https://github.com/hyperlogic/openxrstub/blob/main/src/main.cpp

void tsoUtilInitPoseMat(float* result, const XrPosef * pose)
{
	const float x2  = pose->orientation.x + pose->orientation.x;
	const float y2  = pose->orientation.y + pose->orientation.y;
	const float z2  = pose->orientation.z + pose->orientation.z;

	const float xx2 = pose->orientation.x * x2;
	const float yy2 = pose->orientation.y * y2;
	const float zz2 = pose->orientation.z * z2;

	const float yz2 = pose->orientation.y * z2;
	const float wx2 = pose->orientation.w * x2;
	const float xy2 = pose->orientation.x * y2;
	const float wz2 = pose->orientation.w * z2;
	const float xz2 = pose->orientation.x * z2;
	const float wy2 = pose->orientation.w * y2;

	result[0] = 1.0f - yy2 - zz2;
	result[1] = xy2 + wz2;
	result[2] = xz2 - wy2;
	result[3] = 0.0f;

	result[4] = xy2 - wz2;
	result[5] = 1.0f - xx2 - zz2;
	result[6] = yz2 + wx2;
	result[7] = 0.0f;

	result[8] = xz2 + wy2;
	result[9] = yz2 - wx2;
	result[10] = 1.0f - xx2 - yy2;
	result[11] = 0.0f;

	result[12] = pose->position.x;
	result[13] = pose->position.y;
	result[14] = pose->position.z;
	result[15] = 1.0;
}

void tsoMultiplyMat(float* result, const float* a, const float* b)
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

void tsoInvertOrthogonalMat(float* result, float* src)
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

void tsoUtilInitProjectionMat(XrCompositionLayerProjectionView * layerView, float* projMat, float * invViewMat, float * viewMat, float * modelViewProjMat,
								enum GraphicsAPI graphicsApi, 
								const float nearZ, const float farZ)
{
	// convert XrFovf into an OpenGL projection matrix.
	const float tanAngleLeft = tan(layerView->fov.angleLeft);
	const float tanAngleRight = tan(layerView->fov.angleRight);
	const float tanAngleDown = tan(layerView->fov.angleDown);
	const float tanAngleUp = tan(layerView->fov.angleUp);
	
	const float tanAngleWidth = tanAngleRight - tanAngleLeft;

	// Set to tanAngleDown - tanAngleUp for a clip space with positive Y down (Vulkan).
	// Set to tanAngleUp - tanAngleDown for a clip space with positive Y up (OpenGL / D3D / Metal).
	const float tanAngleHeight = graphicsApi == GRAPHICS_VULKAN ? (tanAngleDown - tanAngleUp) : (tanAngleUp - tanAngleDown);

	// Set to nearZ for a [-1,1] Z clip space (OpenGL / OpenGL ES).
	// Set to zero for a [0,1] Z clip space (Vulkan / D3D / Metal).
	const float offsetZ = (graphicsApi == GRAPHICS_OPENGL || graphicsApi == GRAPHICS_OPENGL_ES) ? nearZ : 0;

	if (farZ <= nearZ)
	{
		// place the far plane at infinity
		projMat[0] = 2 / tanAngleWidth;
		projMat[4] = 0;
		projMat[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
		projMat[12] = 0;

		projMat[1] = 0;
		projMat[5] = 2 / tanAngleHeight;
		projMat[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
		projMat[13] = 0;

		projMat[2] = 0;
		projMat[6] = 0;
		projMat[10] = -1;
		projMat[14] = -(nearZ + offsetZ);

		projMat[3] = 0;
		projMat[7] = 0;
		projMat[11] = -1;
		projMat[15] = 0;
	}
	else
	{
		// normal projection
		projMat[0] = 2 / tanAngleWidth;
		projMat[4] = 0;
		projMat[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
		projMat[12] = 0;

		projMat[1] = 0;
		projMat[5] = 2 / tanAngleHeight;
		projMat[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
		projMat[13] = 0;

		projMat[2] = 0;
		projMat[6] = 0;
		projMat[10] = -(farZ + offsetZ) / (farZ - nearZ);
		projMat[14] = -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);

		projMat[3] = 0;
		projMat[7] = 0;
		projMat[11] = -1;
		projMat[15] = 0;
	}
	
	
	// compute view matrix by inverting the pose
	tsoUtilInitPoseMat(invViewMat, &layerView->pose);
	tsoInvertOrthogonalMat(viewMat, invViewMat);
	tsoMultiplyMat(modelViewProjMat, projMat, viewMat);
}

#endif // TSOPENXR_IMPLEMENTATION

#endif // _TSOPENXR_H
