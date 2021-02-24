// CanonCamera.cpp: определяет экспортированные функции для приложения DLL.
//

#include "stdafx.h"
#include "EDSDK.h"
#include "EDSDKErrors.h"
#include "EDSDKTypes.h"
#include <thread>
#include <chrono>
#include <string>
#include <stdlib.h>
#include <stdio.h>


using namespace std;
char directory[300], file_name[50];
EdsCameraRef camera = NULL;

extern "C"
{
	__declspec(dllexport) bool open_session()
	{
		EdsError err = EdsOpenSession(camera);
		return (err == EDS_ERR_OK);
	}
}

extern "C"
{
	__declspec(dllexport) bool close_session()
	{
		EdsError err = EdsCloseSession(camera);
		return (err == EDS_ERR_OK);
	}
}

extern "C"
{
	void download_img(EdsBaseRef & object, EdsVoid * & context)
	{
		EdsStreamRef stream = NULL;
		EdsDirectoryItemInfo dirItemInfo;
		EdsError err;
		EdsGetDirectoryItemInfo(object, &dirItemInfo);
		char dir[300];
		int i = 0, length = 0;
		length = dirItemInfo.size;

		strcpy(dir, directory);

		strcpy(file_name, dirItemInfo.szFileName);
		strcat(dir, file_name);
		err = EdsCreateFileStream(dir, kEdsFileCreateDisposition_CreateAlways, kEdsAccess_ReadWrite, &stream);

		if (err != EDS_ERR_OK)
		{
			return;
		}
		err = EdsDownload(object, dirItemInfo.size, stream);
		if (err != EDS_ERR_OK)
		{
			return;
		}
		err = EdsDownloadComplete(object);
		if (err != EDS_ERR_OK)
		{
			return;
		}
		err = EdsRelease(stream);
		if (err != EDS_ERR_OK)
		{
			return;
		}
		stream = NULL;
		if (object)
			EdsRelease(object);
	}
}

extern "C"
{
	__declspec(dllexport) void set_path(char * path)
	{
		string str(path);

		for (int i = 0; i < str.length(); ++i)
			directory[i] = str[i];
	}
}

EdsError EDSCALLBACK handleObjectEvent(EdsObjectEvent event, EdsBaseRef object, EdsVoid * context)
{
	download_img(object, context);
	return EDS_ERR_OK;
}

extern "C"
{
	__declspec(dllexport) bool init_camera()
	{
		EdsCameraListRef cameraList = NULL;
		EdsUInt32 count = 0;
		camera = NULL;
		EdsError err;
		err = EdsTerminateSDK(); 	// For avoiding crash of iniialization
		err = EdsInitializeSDK();
		err = EdsGetCameraList(&cameraList);
		err = EdsGetChildCount(cameraList, &count);
		if (count > 0)
		{
			err = EdsGetChildAtIndex(cameraList, 0, &camera);
			err = EdsRelease(cameraList);
		}
		else return false;

		err = EdsSetObjectEventHandler(camera, kEdsObjectEvent_DirItemRequestTransfer, handleObjectEvent, NULL);
		if (err != EDS_ERR_OK) return false;
		return true;
	}
}

extern "C"
{
	bool is_liveview_ready()
	{
		EdsError err = EDS_ERR_OK;
		EdsEvfImageRef image = NULL;
		EdsStreamRef stream = NULL;
		unsigned char* data = NULL;
		EdsUInt64 size = 0;

		err = EdsCreateMemoryStream(0, &stream);
		if (err != EDS_ERR_OK) return false;

		err = EdsCreateEvfImageRef(stream, &image);
		if (err != EDS_ERR_OK) return false;

		err = EdsDownloadEvfImage(camera, image);
		if (err != EDS_ERR_OK) return false;

		err = EdsGetPointer(stream, (EdsVoid**)& data);
		if (err != EDS_ERR_OK) return false;

		err = EdsGetLength(stream, &size);
		if (err != EDS_ERR_OK) return false;

		if (stream != NULL)
		{
			EdsRelease(stream);
			stream = NULL;
		}

		if (image != NULL)
		{
			EdsRelease(image);
			image = NULL;
		}
		data = NULL;
		return true;
	}
}

extern "C"
{
	__declspec(dllexport) bool start_liveview()
	{
		EdsError err = EDS_ERR_OK;

		close_session();
		open_session();

		EdsUInt32 device = kEdsPropID_Evf_OutputDevice;

		err = EdsGetPropertyData(camera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
		if (err != EDS_ERR_OK) return false;


		device |= kEdsEvfOutputDevice_PC;
		err = EdsSetPropertyData(camera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
		if (err != EDS_ERR_OK) return false;
		return true;
	}
}

extern "C"
{
	__declspec(dllexport) bool stop_liveview()
	{
		EdsError err = EDS_ERR_OK;
		EdsUInt32 device;
		err = EdsGetPropertyData(camera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
		if (err != EDS_ERR_OK) return false;

		device &= ~kEdsEvfOutputDevice_PC;

		err = EdsSetPropertyData(camera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
		if (err != EDS_ERR_OK) return false;

		close_session();
		return true;
	}
}

extern "C"
{
	bool is_photo_ready(char * buff)
	{
		EdsGetEvent();

		if (file_name[0] == '\0') return false;
		strcpy_s(buff, sizeof(file_name), file_name);
		return true;
	}
}

extern "C"
{
	__declspec(dllexport) bool take_photo(int saveMarker = 0)
	{
		EdsError err = EDS_ERR_OK;
		for (int i = 0; i < 50; ++i)
		{
			file_name[i] = '\0';
		}

		open_session();


		EdsInt32 saveTarget = NULL;

		if (saveMarker == 0)
		{
			saveTarget = kEdsSaveTo_Host;
		}
		else if (saveMarker == 1)
		{
			saveTarget = kEdsSaveTo_Both;
		}

		err = EdsSetPropertyData(camera, kEdsPropID_SaveTo, 0, 4, &saveTarget);
		EdsCapacity newCapacity = { 0x7FFFFFFF, 0x1000, 1 };
		err = EdsSetCapacity(camera, newCapacity);

		err = EdsSendCommand(camera, kEdsCameraCommand_TakePicture, 0);
		return true;
	}
}

extern "C"
{
	__declspec(dllexport) void dispose()
	{
		close_session();
		EdsTerminateSDK();
	}
}

extern "C"
{
	__declspec(dllexport) void get_image_package(unsigned char* & buff, int & length)
	{
		EdsEvfImageRef image = NULL;
		EdsStreamRef stream = NULL;
		EdsUInt64 size = 0;
		EdsError err = NULL;
		unsigned char * data = NULL;

		err = EdsCreateMemoryStream(0, &stream);
		if (err != EDS_ERR_OK)
		{
			data = NULL;
			length = 0;
			return;
		}
		err = EdsCreateEvfImageRef(stream, &image);
		if (err != EDS_ERR_OK)
		{
			data = NULL;
			length = 0;
			return;
		}
		err = EdsDownloadEvfImage(camera, image);
		if (err != EDS_ERR_OK)
		{
			data = NULL;
			length = 0;
			return;
		}
		err = EdsGetPointer(stream, (EdsVoid**)& data);
		if (err != EDS_ERR_OK)
		{
			data = NULL;
			length = 0;
			return;
		}
		err = EdsGetLength(stream, &size);
		if (err != EDS_ERR_OK)
		{
			data = NULL;
			length = 0;
			return;
		}

		length = size;
		memcpy(buff, data, length);

		if (stream != NULL) {
			EdsRelease(stream);
			stream = NULL;
		}

		if (image != NULL)
		{
			EdsRelease(image);
			image = NULL;
		}
	}
}

