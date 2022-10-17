# Meta Quest verison of tsopenxr

I mean, it works.  Follow Android NDK+SDK directions from https://github.com/cnlohr/rawdrawandroid then just plug in your quest, accept the developer settings and type `make clean all run` and the test app should JustWork.

# OpenXR Loader

Be aware the the copy of `libopenxr_loader.so` is from https://developer.oculus.com/downloads/package/oculus-openxr-mobile-sdk/, but heavily stripped.  This will likely break things if significant updates are made in the future.

This issue should go away now that the OpenXR loader is intended to be open source, but I still have been unable to get that working.


