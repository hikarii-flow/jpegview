#include "stdafx.h"

#include "ICCProfileTransform.h"
#include "SettingsProvider.h"


#ifndef WINXP

// This define is necessary for 32-bit builds to work, for some reason
#define CMS_DLL
#include "lcms2.h"
#define FLAGS (cmsFLAGS_BLACKPOINTCOMPENSATION|cmsFLAGS_COPY_ALPHA)
#define TYPE_LabA_8 (COLORSPACE_SH(PT_Lab)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_YMCK_8 (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(1)|DOSWAP_SH(1)|SWAPFIRST_SH(1))


void* ICCProfileTransform::sRGBProfile = NULL;
void* ICCProfileTransform::LabProfile = NULL;
void* ICCProfileTransform::CMYKProfile = NULL;


void* ICCProfileTransform::GetsRGBProfile() {
	if (sRGBProfile == NULL) {
		// Use try-catch to avoid crash if lcms2.dll not present
		try {
			sRGBProfile = cmsCreate_sRGBProfile();
		} catch (...) {}
	}
	return sRGBProfile;
}

void* ICCProfileTransform::GetLabProfile() {
	if (LabProfile == NULL) {
		LabProfile = cmsCreateLab4Profile(cmsD50_xyY());
	}
	return LabProfile;
}

void* ICCProfileTransform::GetCMYKProfile() {
	if (CMYKProfile == NULL) {
		CHAR path[MAX_PATH + 10];
		DWORD length = GetModuleFileNameA(NULL, path, MAX_PATH);
		if (length > 0 && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			PathRemoveFileSpecA(path);
			lstrcatA(path, "\\cmyk.icm");
		}
		CMYKProfile = cmsOpenProfileFromFile(path, "r");
	}
	return CMYKProfile;
}

void* ICCProfileTransform::CreateTransform(const void* profile, unsigned int size, PixelFormat format)
{
	if (profile == NULL || size == 0 || GetsRGBProfile() == NULL)
		return NULL; // No ICC Profile or lcms2.dll not found

	// Create transform from embedded profile to sRGB
	cmsUInt32Number inFormat, outFormat;
	switch (format) {
		case FORMAT_BGRA:
			inFormat = TYPE_BGRA_8;
			outFormat = TYPE_BGRA_8;
			break;
		case FORMAT_RGBA:
			inFormat = TYPE_RGBA_8;
			outFormat = TYPE_BGRA_8;
			break;
		case FORMAT_BGR:
			inFormat = TYPE_BGR_8;
			outFormat = TYPE_BGR_8;
			break;
		case FORMAT_RGB:
			inFormat = TYPE_RGB_8;
			outFormat = TYPE_BGR_8;
			break;
		case FORMAT_Lab:
			inFormat = TYPE_Lab_8;
			outFormat = TYPE_BGR_8;
			break;
		case FORMAT_LabA:
			inFormat = TYPE_LabA_8;
			outFormat = TYPE_BGRA_8;
			break;
		case FORMAT_YMCK:
			inFormat = TYPE_YMCK_8;
			outFormat = TYPE_BGRA_8;
			break;
		default:
			return NULL;
	}
	cmsHPROFILE hEmbeddedProfile = CSettingsProvider::This().UseEmbeddedColorProfiles() ? cmsOpenProfileFromMem(profile, size) : NULL;
	cmsHPROFILE hInProfile = hEmbeddedProfile;
	if (hInProfile == NULL) {
		switch (T_COLORSPACE(inFormat)) {
			// Use our own profiles for CMYK/Lab PSDs
			case PT_CMYK:
				hInProfile = GetCMYKProfile();
				break;
			case PT_Lab:
				hInProfile = GetLabProfile();
				break;
		}
	}

	cmsHTRANSFORM transform = cmsCreateTransform(hInProfile, inFormat, GetsRGBProfile(), outFormat, INTENT_RELATIVE_COLORIMETRIC, FLAGS);
	cmsCloseProfile(hEmbeddedProfile);
	return transform;
}

bool ICCProfileTransform::DoTransform(void* transform, const void* inputBuffer, void* outputBuffer, unsigned int width, unsigned int height, unsigned int stride) {
	unsigned int numPixels = width * height;
	if (transform == NULL || inputBuffer == NULL || outputBuffer == NULL || numPixels == 0)
		return false;

	int nchannels = T_CHANNELS(cmsGetTransformInputFormat(transform));
	if (stride == 0)
		stride = width * nchannels;
	cmsDoTransformLineStride(transform, inputBuffer, outputBuffer, width, height, stride, Helpers::DoPadding(width * nchannels, 4), stride * height, Helpers::DoPadding(width * nchannels, 4) * height);
	return true;
}

void ICCProfileTransform::DeleteTransform(void* transform)
{
	if (transform != NULL)
		cmsDeleteTransform(transform);
}

#else

// stub out lcms2 methods in an elegant way in XP build, as per suggestion https://github.com/sylikc/jpegview/commit/4b62f07e2a147a04a5014a5711d159670162e799#commitcomment-102738193

void* ICCProfileTransform::CreateTransform(const void* /* profile */, unsigned int /* size */, PixelFormat /* format */) {
	return NULL;
}

bool ICCProfileTransform::DoTransform(void* /* transform */, const void* /* inputBuffer */, void* /* outputBuffer */, unsigned int /* width */, unsigned int /* height */, unsigned int /* stride */) {
	return false;
}

void ICCProfileTransform::DeleteTransform(void* /* transform */) { }

#endif
