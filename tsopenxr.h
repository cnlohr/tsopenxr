
// Very basic C OpenXR header-only library
// (based on https://github.com/cnlohr/openxr-minimal)
// (based on https://github.com/hyperlogic/openxrstub/blob/main/src/main.cpp)
// 
// Portions are: 
//	Copyright (c) 2020 Anthony J. Thibault
// The rest is:
//	Copyright (c) 2022 Charles Lohr
//
// Both under the MIT/x11 License.
//
// To use this library in a given project's C file:
//  #define TSOPENXR_IMPLEMENTATION

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
#else
#define XR_USE_PLATFORM_XLIB
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

extern XrExtensionProperties * tsoExtensionProps;
extern int tsoNumExtensions;
extern XrApiLayerProperties * tsoLayerProps;
extern int tsoNumLayerProps;
extern XrViewConfigurationView * tsoViewConfigs;
extern int tsoNumViewConfigs;
extern XrSpace tsoHandSpace[2];
extern XrTime tsoPredictedDisplayTime;

extern XrInstance tsoInstance;
extern XrSystemId tsoSystemId;
extern XrSession tsoSession;
extern XrActionSet tsoActionSet;
extern XrSpace tsoStageSpace;

typedef struct
{
	XrSwapchain handle;
	int32_t width;
	int32_t height;
} tsoSwapchainInfo;

extern tsoSwapchainInfo * tsoSwapchains;
extern XrSwapchainImageOpenGLKHR ** tsoSwapchainImages; //[tsoViewConfigsCount][tsoSwapchainLengths[...]]
extern uint32_t * tsoSwapchainLengths; //[tsoViewConfigsCount]

typedef int (*tsoRenderLayer_t)(XrInstance tsoInstance, XrSession tsoSession, XrViewConfigurationView * tsoViewConfigs, int tsoViewConfigsCount,
		 XrSpace tsoStageSpace,  XrTime predictedDisplayTime, XrCompositionLayerProjection * layer, XrCompositionLayerProjectionView * projectionLayerViews,
		 XrView * views, int viewCountOutput, void * opaque );


// For debugging.
extern int tsoPrintAll;

extern int tsoSessionReady;
extern XrSessionState tsoXRState;

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

int tsoCheck( XrInstance tsoInstance, XrResult result, const char* str );
int tsoExtensionSupported( XrExtensionProperties * extensions, int extensionsCount, const char* extensionName );
int tsoEnumerateLayers( XrApiLayerProperties ** tsoLayerProps );
int tsoCreateInstance( XrInstance * tsoInstance, const char * appname );
int tsoGetSystemId( XrInstance tsoInstance, XrSystemId * tsoSystemId );
int tsoEnumeratetsoViewConfigs( XrInstance tsoInstance, XrSystemId tsoSystemId, XrViewConfigurationView ** tsoViewConfigs );
int tsoCreateSession(XrInstance tsoInstance, XrSystemId tsoSystemId, XrSession * tsoSession, uint32_t openglMajor, uint32_t openglMinor );

// You almost certainly will want to override this!
int tsoDefaultCreateActions(XrInstance tsoInstance, XrSystemId tsoSystemId, XrSession tsoSession, XrActionSet * tsoActionSet);

int tsoCreateStageSpace(XrInstance tsoInstance, XrSystemId tsoSystemId, XrSession tsoSession, XrSpace * tsoStageSpace);
int tsoBeginSession(XrInstance tsoInstance, XrSystemId tsoSystemId, XrSession tsoSession);
int tsoCreateSwapchains(XrInstance tsoInstance, XrSession tsoSession,
					  XrViewConfigurationView * tsoViewConfigs, int tsoViewConfigsCount,
					  tsoSwapchainInfo ** tsoSwapchains, // Will allocate to tsoViewConfigsCount
					  XrSwapchainImageOpenGLKHR *** tsoSwapchainImages,
					  uint32_t ** tsoSwapchainLengths); // Actually array of pointers to pointers
int tsoSyncInput( XrInstance tsoInstance, XrSession tsoSession, XrActionSet tsoActionSet );

int tsoRenderFrame(XrInstance tsoInstance, XrSession tsoSession, XrViewConfigurationView * tsoViewConfigs, int tsoViewConfigsCount,
				 XrSpace tsoStageSpace, tsoRenderLayer_t RenderLayer, void * opaque );

// Utility functions

void tsoUtilInitPoseMat(float* result, const XrPosef * pose);
enum GraphicsAPI { GRAPHICS_VULKAN, GRAPHICS_OPENGL, GRAPHICS_OPENGL_ES, GRAPHICS_D3D };
void tsoUtilInitProjectionMat(float* result, enum GraphicsAPI graphicsApi, const float tanAngleLeft,
							  const float tanAngleRight, const float tanAngleUp, float const tanAngleDown,
							  const float nearZ, const float farZ);



#ifdef TSOPENXR_IMPLEMENTATION


#include <stdlib.h>
#include <string.h>

// For limited OpenGL Platforms, Like Windows.
#ifndef GL_MAJOR_VERSION
#define GL_MAJOR_VERSION   0x821B
#endif

#ifndef GL_MINOR_VERSION
#define GL_MINOR_VERSION   0x821C
#endif

#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif

#ifndef GL_SRGB8
#define GL_SRGB8 0x8C41
#endif


XrExtensionProperties * tsoExtensionProps;
int tsoNumExtensions;
XrApiLayerProperties * tsoLayerProps;
int tsoNumLayerProps;
XrViewConfigurationView * tsoViewConfigs;
int tsoNumViewConfigs;

XrInstance tsoInstance = XR_NULL_HANDLE;
XrSystemId tsoSystemId = XR_NULL_HANDLE;
XrSession tsoSession = XR_NULL_HANDLE;
XrActionSet tsoActionSet = XR_NULL_HANDLE;
XrSpace tsoStageSpace = XR_NULL_HANDLE;
XrSpace tsoHandSpace[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
XrTime tsoPredictedDisplayTime;

tsoSwapchainInfo * tsoSwapchains;
XrSwapchainImageOpenGLKHR ** tsoSwapchainImages;
uint32_t * tsoSwapchainLengths;

// For debugging.
int tsoPrintAll = 1;

int tsoSessionReady = 0;
XrSessionState tsoXRState = XR_SESSION_STATE_UNKNOWN;

int tsoCheck( XrInstance tsoInstance, XrResult result, const char* str )
{
	if( XR_SUCCEEDED( result ))
	{
		return 1;
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
	return 0;
}

int EnumerateExtensions( XrExtensionProperties ** tsoExtensionProps )
{
	XrResult result;
	uint32_t extensionCount = 0;
	result = xrEnumerateInstanceExtensionProperties(NULL, 0, &extensionCount, NULL);
	if( !tsoCheck(NULL, result, "xrEnumerateInstanceExtensionProperties failed"))
		return 0;
	
	*tsoExtensionProps = realloc( *tsoExtensionProps, extensionCount * sizeof( XrExtensionProperties ) );
	for( int i = 0; i < extensionCount; i++ )
	{
		(*tsoExtensionProps)[i].type = XR_TYPE_EXTENSION_PROPERTIES;
		(*tsoExtensionProps)[i].next = NULL;
	}

	result = xrEnumerateInstanceExtensionProperties( NULL, extensionCount, &extensionCount, *tsoExtensionProps );
	if( !tsoCheck( NULL, result, "xrEnumerateInstanceExtensionProperties failed" ) )
		return 0;

#if TSOPENXR_ENABLE_DEBUG
	if( tsoPrintAll )
	{
		TSOPENXR_INFO("%d extensions:\n", extensionCount);
		for( uint32_t i = 0; i < extensionCount; ++i )
		{
			TSOPENXR_INFO( "	%s\n", (*tsoExtensionProps)[i].extensionName );
		}
	}
#endif
	return extensionCount;
}

int tsoExtensionSupported( XrExtensionProperties * extensions, int extensionsCount, const char* extensionName)
{
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

int tsoEnumerateLayers( XrApiLayerProperties ** tsoLayerProps )
{
	uint32_t layerCount;
	XrResult result = xrEnumerateApiLayerProperties(0, &layerCount, NULL);
	if( !tsoCheck(NULL, result, "xrEnumerateApiLayerProperties"))
	{
		return 0;
	}

	*tsoLayerProps = realloc( *tsoLayerProps, layerCount * sizeof(XrApiLayerProperties) );
	for( uint32_t i = 0; i < layerCount; i++) {
		(*tsoLayerProps)[i].type = XR_TYPE_API_LAYER_PROPERTIES;
		(*tsoLayerProps)[i].next = NULL;
	}
	result = xrEnumerateApiLayerProperties( layerCount, &layerCount, *tsoLayerProps );
	if( !tsoCheck(NULL, result, "xrEnumerateApiLayerProperties"))
	{
		return 0;
	}

	if( tsoPrintAll )
	{
		TSOPENXR_INFO("%d layers:\n", layerCount);
		for( uint32_t i = 0; i < layerCount; i++)
		{
			TSOPENXR_INFO("	%s, %s\n", (*tsoLayerProps)[i].layerName, (*tsoLayerProps)[i].description);
		}
	}

	return layerCount;
}

int tsoCreateInstance(XrInstance * tsoInstance, const char * appname )
{
	// create openxr tsoInstance
	XrResult result;
	const char* const enabledExtensions[] = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
	XrInstanceCreateInfo ici;
	ici.type = XR_TYPE_INSTANCE_CREATE_INFO;
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
	if (!tsoCheck(NULL, result, "xrCreateInstance failed"))
	{
		return 0;
	}

#if TSOPENXR_ENABLE_DEBUG
	if ( tsoPrintAll)
	{
		XrInstanceProperties ip;
		ip.type = XR_TYPE_INSTANCE_PROPERTIES;
		ip.next = NULL;

		result = xrGetInstanceProperties( *tsoInstance, &ip );
		if( !tsoCheck( *tsoInstance, result, "xrGetInstanceProperties failed" ) )
		{
			return 0;
		}

		TSOPENXR_INFO("Runtime Name: %s\n", ip.runtimeName);
		TSOPENXR_INFO("Runtime Version: %d.%d.%d\n",
			   XR_VERSION_MAJOR(ip.runtimeVersion),
			   XR_VERSION_MINOR(ip.runtimeVersion),
			   XR_VERSION_PATCH(ip.runtimeVersion));
	}
#endif
	return 1;
}


int tsoGetSystemId(XrInstance tsoInstance, XrSystemId * tsoSystemId)
{
	XrResult result;
	XrSystemGetInfo sgi;
	sgi.type = XR_TYPE_SYSTEM_GET_INFO;
	sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	sgi.next = NULL;

	result = xrGetSystem(tsoInstance, &sgi, tsoSystemId);
	if (!tsoCheck(tsoInstance, result, "xrGetSystemFailed"))
	{
		return 0;
	}

#if TSOPENXR_ENABLE_DEBUG
	if ( tsoPrintAll)
	{
		XrSystemProperties sp = { 0 };
		sp.type = XR_TYPE_SYSTEM_PROPERTIES;

		result = xrGetSystemProperties(tsoInstance, *tsoSystemId, &sp);
		if (!tsoCheck(tsoInstance, result, "xrGetSystemProperties failed"))
		{
			return 0;
		}

		TSOPENXR_INFO("System properties for system \"%s\":\n", sp.systemName);
		TSOPENXR_INFO("	maxLayerCount: %d\n", sp.graphicsProperties.maxLayerCount);
		TSOPENXR_INFO("	maxSwapChainImageHeight: %d\n", sp.graphicsProperties.maxSwapchainImageHeight);
		TSOPENXR_INFO("	maxSwapChainImageWidth: %d\n", sp.graphicsProperties.maxSwapchainImageWidth);
		TSOPENXR_INFO("	Orientation Tracking: %s\n", sp.trackingProperties.orientationTracking ? "true" : "false");
		TSOPENXR_INFO("	Position Tracking: %s\n", sp.trackingProperties.positionTracking ? "true" : "false");
	}
#endif

	return 1;
}

int tsoEnumeratetsoViewConfigs(XrInstance tsoInstance, XrSystemId tsoSystemId, XrViewConfigurationView ** tsoViewConfigs)
{
	XrResult result;
	uint32_t viewCount;
	XrViewConfigurationType stereoViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	result = xrEnumerateViewConfigurationViews(tsoInstance, tsoSystemId, stereoViewConfigType, 0, &viewCount, NULL);
	if (!tsoCheck(tsoInstance, result, "xrEnumerateViewConfigurationViews"))
	{
		return 0;
	}

	*tsoViewConfigs = realloc( *tsoViewConfigs, viewCount * sizeof(XrViewConfigurationView) );
	for (uint32_t i = 0; i < viewCount; i++)
	{
		(*tsoViewConfigs)[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
		(*tsoViewConfigs)[i].next = NULL;
	}

	result = xrEnumerateViewConfigurationViews(tsoInstance, tsoSystemId, stereoViewConfigType, viewCount, &viewCount, *tsoViewConfigs);
	if (!tsoCheck(tsoInstance, result, "xrEnumerateViewConfigurationViews"))
	{
		return 0;
	}

#if TSOPENXR_ENABLE_DEBUG
	if (tsoPrintAll)
	{
		TSOPENXR_INFO("%d tsoViewConfigs:\n", viewCount);
		for (uint32_t i = 0; i < viewCount; i++)
		{
			TSOPENXR_INFO("	tsoViewConfigs[%d]:\n", i);
			TSOPENXR_INFO("		recommendedImageRectWidth: %d\n", (*tsoViewConfigs)[i].recommendedImageRectWidth);
			TSOPENXR_INFO("		maxImageRectWidth: %d\n", (*tsoViewConfigs)[i].maxImageRectWidth);
			TSOPENXR_INFO("		recommendedImageRectHeight: %d\n", (*tsoViewConfigs)[i].recommendedImageRectHeight);
			TSOPENXR_INFO("		maxImageRectHeight: %d\n", (*tsoViewConfigs)[i].maxImageRectHeight);
			TSOPENXR_INFO("		recommendedSwapchainSampleCount: %d\n", (*tsoViewConfigs)[i].recommendedSwapchainSampleCount);
			TSOPENXR_INFO("		maxSwapchainSampleCount: %d\n", (*tsoViewConfigs)[i].maxSwapchainSampleCount);
		}
	}
#endif
	return viewCount;
}

int tsoCreateSession(XrInstance tsoInstance, XrSystemId tsoSystemId, XrSession * tsoSession, uint32_t openglMajor, uint32_t openglMinor )
{
	XrResult result;

	// check if opengl version is sufficient.
	{
		XrGraphicsRequirementsOpenGLKHR reqs;
		reqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
		reqs.next = NULL;
		PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = NULL;
		result = xrGetInstanceProcAddr(tsoInstance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction *)&pfnGetOpenGLGraphicsRequirementsKHR);
		if (!tsoCheck(tsoInstance, result, "xrGetInstanceProcAddr"))
		{
			return 0;
		}

		result = pfnGetOpenGLGraphicsRequirementsKHR(tsoInstance, tsoSystemId, &reqs);
		if (!tsoCheck(tsoInstance, result, "GetOpenGLGraphicsRequirementsPKR"))
		{
			return 0;
		}

		const XrVersion desiredApiVersion = XR_MAKE_VERSION(openglMajor, openglMinor, 0);

#if TSOPENXR_ENABLE_DEBUG
		if (tsoPrintAll)
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
			return 0;
		}
	}

#ifdef XR_USE_PLATFORM_WIN32
	XrGraphicsBindingOpenGLWin32KHR glBinding;
	glBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
	glBinding.next = NULL;
	glBinding.hDC = wglGetCurrentDC();
	glBinding.hGLRC = wglGetCurrentContext();
#else
	// XrGraphicsBindingOpenGLXlibKHR
	// XrGraphicsBindingOpenGLESAndroidKHR
#endif

	XrSessionCreateInfo sci;
	sci.type = XR_TYPE_SESSION_CREATE_INFO;
	sci.next = &glBinding;
	sci.systemId = tsoSystemId;

	result = xrCreateSession(tsoInstance, &sci, tsoSession);
	if (!tsoCheck(tsoInstance, result, "xrCreateSession"))
	{
		return 0;
	}
	
	return 1;
}


int tsoDefaultCreateActions(XrInstance tsoInstance, XrSystemId tsoSystemId, XrSession tsoSession, XrActionSet * tsoActionSet)
{
	XrResult result;

	// create action set
	XrActionSetCreateInfo asci;
	asci.type = XR_TYPE_ACTION_SET_CREATE_INFO;
	asci.next = NULL;
	strcpy(asci.actionSetName, "gameplay");
	strcpy(asci.localizedActionSetName, "Gameplay");
	asci.priority = 0;
	result = xrCreateActionSet(tsoInstance, &asci, tsoActionSet);
	if (!tsoCheck(tsoInstance, result, "xrCreateActionSet XR_TYPE_ACTION_SET_CREATE_INFO"))
	{
		return 0;
	}

	XrPath handPath[2] = { XR_NULL_PATH, XR_NULL_PATH };
	xrStringToPath(tsoInstance, "/user/hand/left", &handPath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right", &handPath[1]);
	if (!tsoCheck(tsoInstance, result, "xrStringToPath"))
	{
		return 0;
	}

	XrAction grabAction = XR_NULL_HANDLE;
	XrActionCreateInfo aci;
	aci.type = XR_TYPE_ACTION_CREATE_INFO;
	aci.next = NULL;
	aci.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
	strcpy(aci.actionName, "grab_object");
	strcpy(aci.localizedActionName, "Grab Object");
	aci.countSubactionPaths = 2;
	aci.subactionPaths = handPath;
	result = xrCreateAction(*tsoActionSet, &aci, &grabAction);
	if (!tsoCheck(tsoInstance, result, "xrCreateAction XR_TYPE_ACTION_CREATE_INFO 1"))
	{
		return 0;
	}

	XrAction poseAction = XR_NULL_HANDLE;
	aci.type = XR_TYPE_ACTION_CREATE_INFO;
	aci.next = NULL;
	aci.actionType = XR_ACTION_TYPE_POSE_INPUT;
	strcpy(aci.actionName, "hand_pose");
	strcpy(aci.localizedActionName, "Hand Pose");
	aci.countSubactionPaths = 2;
	aci.subactionPaths = handPath;
	result = xrCreateAction(*tsoActionSet, &aci, &poseAction);
	if (!tsoCheck(tsoInstance, result, "xrCreateAction XR_TYPE_ACTION_CREATE_INFO 2"))
	{
		return 0;
	}

	XrAction vibrateAction = XR_NULL_HANDLE;
	aci.type = XR_TYPE_ACTION_CREATE_INFO;
	aci.next = NULL;
	aci.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
	strcpy(aci.actionName, "vibrate_hand");
	strcpy(aci.localizedActionName, "Vibrate Hand");
	aci.countSubactionPaths = 2;
	aci.subactionPaths = handPath;
	result = xrCreateAction(*tsoActionSet, &aci, &vibrateAction);
	if (!tsoCheck(tsoInstance, result, "xrCreateAction XR_TYPE_ACTION_CREATE_INFO 3"))
	{
		return 0;
	}

	XrAction quitAction = XR_NULL_HANDLE;
	aci.type = XR_TYPE_ACTION_CREATE_INFO;
	aci.next = NULL;
	aci.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
	strcpy(aci.actionName, "quit_session");
	strcpy(aci.localizedActionName, "Quit Session");
	aci.countSubactionPaths = 2;
	aci.subactionPaths = handPath;
	result = xrCreateAction(*tsoActionSet, &aci, &quitAction);
	if (!tsoCheck(tsoInstance, result, "xrCreateAction XR_TYPE_ACTION_CREATE_INFO 4"))
	{
		return 0;
	}

	XrPath selectPath[2] = {XR_NULL_PATH, XR_NULL_PATH};
	XrPath squeezeValuePath[2] = {XR_NULL_PATH, XR_NULL_PATH};
	XrPath squeezeClickPath[2] = {XR_NULL_PATH, XR_NULL_PATH};
	XrPath posePath[2] = {XR_NULL_PATH, XR_NULL_PATH};
	XrPath hapticPath[2] = {XR_NULL_PATH, XR_NULL_PATH};
	XrPath menuClickPath[2] = {XR_NULL_PATH, XR_NULL_PATH};
	xrStringToPath(tsoInstance, "/user/hand/left/input/select/click", &selectPath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/input/select/click", &selectPath[1]);
	xrStringToPath(tsoInstance, "/user/hand/left/input/squeeze/value", &squeezeValuePath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/input/squeeze/value", &squeezeValuePath[1]);
	xrStringToPath(tsoInstance, "/user/hand/left/input/squeeze/click", &squeezeClickPath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/input/squeeze/click", &squeezeClickPath[1]);
	xrStringToPath(tsoInstance, "/user/hand/left/input/grip/pose", &posePath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/input/grip/pose", &posePath[1]);
	xrStringToPath(tsoInstance, "/user/hand/left/output/haptic", &hapticPath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/output/haptic", &hapticPath[1]);
	xrStringToPath(tsoInstance, "/user/hand/left/input/menu/click", &menuClickPath[0]);
	xrStringToPath(tsoInstance, "/user/hand/right/input/menu/click", &menuClickPath[1]);
	if (!tsoCheck(tsoInstance, result, "xrStringToPath"))
	{
		return 0;
	}

	// KHR Simple
	{
		XrPath interactionProfilePath = XR_NULL_PATH;
		xrStringToPath(tsoInstance, "/interaction_profiles/khr/simple_controller", &interactionProfilePath);
		XrActionSuggestedBinding bindings[8] = {
			{grabAction, selectPath[0]},
			{grabAction, selectPath[1]},
			{poseAction, posePath[0]},
			{poseAction, posePath[1]},
			{quitAction, menuClickPath[0]},
			{quitAction, menuClickPath[1]},
			{vibrateAction, hapticPath[0]},
			{vibrateAction, hapticPath[1]}
		};

		XrInteractionProfileSuggestedBinding suggestedBindings;
		suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
		suggestedBindings.next = NULL;
		suggestedBindings.interactionProfile = interactionProfilePath;
		suggestedBindings.suggestedBindings = bindings;
		suggestedBindings.countSuggestedBindings = sizeof( bindings ) / sizeof( bindings[0] );
		result = xrSuggestInteractionProfileBindings(tsoInstance, &suggestedBindings);
		if (!tsoCheck(tsoInstance, result, "xrSuggestInteractionProfileBindings"))
		{
			return 0;
		}
	}

	XrActionSpaceCreateInfo aspci = { 0 };
	aspci.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
	aspci.next = NULL;
	aspci.action = poseAction;
	XrPosef identity = { {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f} };
	aspci.poseInActionSpace = identity;

	aspci.subactionPath = handPath[0];
	result = xrCreateActionSpace(tsoSession, &aspci, &tsoHandSpace[0]);
	if (!tsoCheck(tsoInstance, result, "xrCreateActionSpace"))
	{
		return 0;
	}

	aspci.subactionPath = handPath[1];
	result = xrCreateActionSpace(tsoSession, &aspci, &tsoHandSpace[1]);
	if (!tsoCheck(tsoInstance, result, "xrCreateActionSpace"))
	{
		return 0;
	}

	XrSessionActionSetsAttachInfo sasai;
	sasai.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
	sasai.next = NULL;
	sasai.countActionSets = 1;
	sasai.actionSets = tsoActionSet;
	result = xrAttachSessionActionSets(tsoSession, &sasai);
	if (!tsoCheck(tsoInstance, result, "xrSessionActionSetsAttachInfo"))
	{
		return 0;
	}

	return 1;
}


int tsoCreateStageSpace(XrInstance tsoInstance, XrSystemId tsoSystemId, XrSession tsoSession, XrSpace * tsoStageSpace)
{
	XrResult result;
#if TSOPENXR_ENABLE_DEBUG
	if (tsoPrintAll)
	{
		uint32_t referenceSpacesCount;
		result = xrEnumerateReferenceSpaces(tsoSession, 0, &referenceSpacesCount, NULL);
		if (!tsoCheck(tsoInstance, result, "xrEnumerateReferenceSpaces"))
		{
			return 0;
		}

		XrReferenceSpaceType * referenceSpaces = malloc( referenceSpacesCount * sizeof(XrReferenceSpaceType) );
		int i;
		for( i = 0; i < referenceSpacesCount; i++ )
			referenceSpaces[i] = XR_REFERENCE_SPACE_TYPE_VIEW;
		result = xrEnumerateReferenceSpaces(tsoSession, referenceSpacesCount, &referenceSpacesCount, referenceSpaces );
		if (!tsoCheck(tsoInstance, result, "xrEnumerateReferenceSpaces"))
		{
			return 0;
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
	result = xrCreateReferenceSpace(tsoSession, &rsci, tsoStageSpace);
	if (!tsoCheck(tsoInstance, result, "xrCreateReferenceSpace"))
	{
		return 0;
	}

	return 1;
}


int tsoCreateSwapchains(XrInstance tsoInstance, XrSession tsoSession,
					  XrViewConfigurationView * tsoViewConfigs, int tsoViewConfigsCount,
					  tsoSwapchainInfo ** tsoSwapchains, // Will allocate to tsoViewConfigsCount
					  XrSwapchainImageOpenGLKHR *** tsoSwapchainImages,
					  uint32_t ** tsoSwapchainLengths) // Actually array of pointers to pointers
{
	int i;
	XrResult result;
	uint32_t swapchainFormatCount;
	result = xrEnumerateSwapchainFormats(tsoSession, 0, &swapchainFormatCount, NULL);
	if (!tsoCheck(tsoInstance, result, "xrEnumerateSwapchainFormats"))
	{
		return 0;
	}

	int64_t swapchainFormats[swapchainFormatCount];
	result = xrEnumerateSwapchainFormats(tsoSession, swapchainFormatCount, &swapchainFormatCount, swapchainFormats);
	if (!tsoCheck(tsoInstance, result, "xrEnumerateSwapchainFormats"))
	{
		return 0;
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
	if( tsoPrintAll )
	{
		TSOPENXR_INFO( "Formats:" );
		for( i = 0; i < swapchainFormatCount; i++ )
		{
			TSOPENXR_INFO( "%c%04x%c", ( i == selfmt )?'*':' ', (int)swapchainFormats[i], (i == swapchainFormatCount-1)?'\n':',' );
		} 
	}
#endif

	// For now we just pick the default one.
	int64_t swapchainFormatToUse = swapchainFormats[selfmt];

	*tsoSwapchains = realloc( *tsoSwapchains, tsoViewConfigsCount * sizeof( tsoSwapchainInfo ) );
	*tsoSwapchainLengths = realloc( *tsoSwapchainLengths, tsoViewConfigsCount * sizeof( uint32_t ) );
	for (uint32_t i = 0; i < tsoViewConfigsCount; i++)
	{
		XrSwapchainCreateInfo sci;
		sci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
		sci.next = NULL;
		sci.createFlags = 0;
		sci.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		sci.format = swapchainFormatToUse;
		sci.sampleCount = 1;
		sci.width = tsoViewConfigs[i].recommendedImageRectWidth;
		sci.height = tsoViewConfigs[i].recommendedImageRectHeight;
		sci.faceCount = 1;
		sci.arraySize = 1;
		sci.mipCount = 1;

		XrSwapchain swapchainHandle;
		result = xrCreateSwapchain(tsoSession, &sci, &swapchainHandle);
		if (!tsoCheck(tsoInstance, result, "xrCreateSwapchain"))
		{
			return 0;
		}

		(*tsoSwapchains)[i].handle = swapchainHandle;
		(*tsoSwapchains)[i].width = sci.width;
		(*tsoSwapchains)[i].height = sci.height;

		result = xrEnumerateSwapchainImages((*tsoSwapchains)[i].handle, 0, &(*tsoSwapchainLengths)[i], NULL);
		if (!tsoCheck(tsoInstance, result, "xrEnumerateSwapchainImages"))
		{
			return 0;
		}
	}

	*tsoSwapchainImages = realloc( *tsoSwapchainImages, tsoViewConfigsCount * sizeof( XrSwapchainImageOpenGLKHR * ) ); 
	for (uint32_t i = 0; i < tsoViewConfigsCount; i++)
	{
		(*tsoSwapchainImages)[i] = malloc( (*tsoSwapchainLengths)[i] * sizeof(XrSwapchainImageOpenGLKHR) );
		for (uint32_t j = 0; j < (*tsoSwapchainLengths)[i]; j++)
		{
			(*tsoSwapchainImages)[i][j].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
			(*tsoSwapchainImages)[i][j].next = NULL;
		}

		result = xrEnumerateSwapchainImages((*tsoSwapchains)[i].handle, (*tsoSwapchainLengths)[i], &(*tsoSwapchainLengths)[i],
											(XrSwapchainImageBaseHeader*)((*tsoSwapchainImages)[i]));

		if (!tsoCheck(tsoInstance, result, "xrEnumerateSwapchainImages"))
		{
			return 0;
		}
	}
	
	return 1;
}


int tsoBeginSession(XrInstance tsoInstance, XrSystemId tsoSystemId, XrSession tsoSession)
{
	XrResult result;
	XrViewConfigurationType stereoViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	XrSessionBeginInfo sbi;
	sbi.type = XR_TYPE_SESSION_BEGIN_INFO;
	sbi.next = NULL;
	sbi.primaryViewConfigurationType = stereoViewConfigType;

	result = xrBeginSession(tsoSession, &sbi);
	if (!tsoCheck(tsoInstance, result, "xrBeginSession"))
	{
		return 0;
	}

	return 1;
}


int tsoSyncInput(XrInstance tsoInstance, XrSession tsoSession, XrActionSet tsoActionSet)
{
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
	if (!tsoCheck(tsoInstance, result, "xrSyncActions"))
	{
		return 0;
	}

	// Todo's from original author.
	// AJT: TODO: xrGetActionStateFloat()
	// AJT: TODO: xrGetActionStatePose()

	return 1;
}

int tsoRenderFrame(XrInstance tsoInstance, XrSession tsoSession, XrViewConfigurationView * tsoViewConfigs, int tsoViewConfigsCount,
				 XrSpace tsoStageSpace, tsoRenderLayer_t RenderLayer, void * opaque )
{
	XrFrameState fs;
	fs.type = XR_TYPE_FRAME_STATE;
	fs.next = NULL;

	XrFrameWaitInfo fwi;
	fwi.type = XR_TYPE_FRAME_WAIT_INFO;
	fwi.next = NULL;

	XrResult result = xrWaitFrame(tsoSession, &fwi, &fs);
	if (!tsoCheck(tsoInstance, result, "xrWaitFrame"))
	{
		return 0;
	}

	XrFrameBeginInfo fbi;
	fbi.type = XR_TYPE_FRAME_BEGIN_INFO;
	fbi.next = NULL;
	result = xrBeginFrame(tsoSession, &fbi);
	if (!tsoCheck(tsoInstance, result, "xrBeginFrame"))
	{
		return 0;
	}

	int layerCount = 0;
	XrCompositionLayerProjection layer;
	const XrCompositionLayerBaseHeader * layers[1] = { (XrCompositionLayerBaseHeader *)&layer };
	layer.layerFlags = 0; //XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
	layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
	layer.next = NULL;
	layer.space = tsoStageSpace;


	XrView views[tsoViewConfigsCount];
	for (size_t i = 0; i < tsoViewConfigsCount; i++)
	{
		views[i].type = XR_TYPE_VIEW;
		views[i].next = NULL;
	}
	
	uint32_t viewCountOutput;
	XrViewState viewState;
	viewState.type = XR_TYPE_VIEW_STATE;
	viewState.next = NULL;

	XrViewLocateInfo vli;
	vli.type = XR_TYPE_VIEW_LOCATE_INFO;
	vli.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	tsoPredictedDisplayTime = vli.displayTime = fs.predictedDisplayTime;
	vli.space = tsoStageSpace;
	result = xrLocateViews( tsoSession, &vli, &viewState, tsoViewConfigsCount, &viewCountOutput, views );
	if (!tsoCheck(tsoInstance, result, "xrLocateViews"))
	{
		return 0;
	}

	XrCompositionLayerProjectionView projectionLayerViews[viewCountOutput];

	// We only support up to 1 layer.
	if (fs.shouldRender == XR_TRUE && XR_UNQUALIFIED_SUCCESS(result))
	{
		if (RenderLayer(tsoInstance, tsoSession, tsoViewConfigs, tsoViewConfigsCount,
						tsoStageSpace, fs.predictedDisplayTime, &layer, projectionLayerViews, views, viewCountOutput, opaque))
		{
			layerCount = 1;
		}
	}

	XrFrameEndInfo fei = { 0 };
	fei.type = XR_TYPE_FRAME_END_INFO;
	fei.displayTime = fs.predictedDisplayTime;
	fei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	fei.layerCount = layerCount;
	fei.layers = layers;

	result = xrEndFrame(tsoSession, &fei);
	if (!tsoCheck(tsoInstance, result, "xrEndFrame"))
	{
		return 0;
	}

	return 1;
}



int tsoHandleLoop( XrInstance tsoInstance )
{
	XrEventDataBuffer xrEvent;
	xrEvent.type = XR_TYPE_EVENT_DATA_BUFFER;
	xrEvent.next = NULL;

	XrResult result = xrPollEvent(tsoInstance, &xrEvent);

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
			tsoXRState = ssc->state;
			switch (tsoXRState)
			{
			case XR_SESSION_STATE_IDLE:
				// The initial state after calling xrCreateSession or returned to after calling xrEndSession.
				TSOPENXR_INFO("XR_SESSION_STATE_IDLE\n");
				break;
			case XR_SESSION_STATE_READY:
				// The application is ready to call xrBeginSession and sync its frame loop with the runtime.
				TSOPENXR_INFO("XR_SESSION_STATE_READY\n");
				if (!tsoBeginSession(tsoInstance, tsoSystemId, tsoSession))
				{
					return 0;
				}
				tsoSessionReady = 1;
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
				TSOPENXR_INFO("XR_SESSION_STATE_??? %d\n", (int)tsoXRState);
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
	return 1;
}

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

void tsoUtilInitProjectionMat(float* result, enum GraphicsAPI graphicsApi, const float tanAngleLeft,
							  const float tanAngleRight, const float tanAngleUp, float const tanAngleDown,
							  const float nearZ, const float farZ)
{
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
		result[0] = 2 / tanAngleWidth;
		result[4] = 0;
		result[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
		result[12] = 0;

		result[1] = 0;
		result[5] = 2 / tanAngleHeight;
		result[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
		result[13] = 0;

		result[2] = 0;
		result[6] = 0;
		result[10] = -1;
		result[14] = -(nearZ + offsetZ);

		result[3] = 0;
		result[7] = 0;
		result[11] = -1;
		result[15] = 0;
	}
	else
	{
		// normal projection
		result[0] = 2 / tanAngleWidth;
		result[4] = 0;
		result[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
		result[12] = 0;

		result[1] = 0;
		result[5] = 2 / tanAngleHeight;
		result[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
		result[13] = 0;

		result[2] = 0;
		result[6] = 0;
		result[10] = -(farZ + offsetZ) / (farZ - nearZ);
		result[14] = -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);

		result[3] = 0;
		result[7] = 0;
		result[11] = -1;
		result[15] = 0;
	}
}

#endif // TSOPENXR_IMPLEMENTATION

#endif // _TSOPENXR_H