using System.Collections;
using System.Collections.Generic;
using System.Threading;
using System.Text;
using UnityEngine;
using System.IO;
using System;
using System.Runtime.InteropServices;


public interface ICanonCameraDelegate
{
	void PhotoDownloaded(string fileName);
	void LiveViewStarted();
}

public partial class CanonCamera : MonoBehaviour
{

	// I N S T R U C T I O N S
	// Every time you should use InitCamera -> CloseSession at the beginnig using camera;
	// Use SetPath to set directory where should put downloaded files;

	// OpenSession -> TakePhoto -> CloseSession     
	// OpenSession -> StartLiveview -> GetImagePackage -> StopLiveview -> CloseSession
	// OpenSession -> StartLiveview -> StartVideo -> StopVideo -> StopLiveview -> CloseSesison 
	//								->    TakeVideo(length)	   ->

	private IntPtr dllModule;
	private IntPtr dataBuffer;
	private int buffSize = 5000000; // bytes;

	void Awake()
	{
		dataBuffer = Marshal.AllocHGlobal(buffSize);
		IntPtr module = GetModuleHandle("CanonCamera");

		if (module != IntPtr.Zero)
		{
			FreeLibrary(module);
		}

		dllModule = LoadLibrary("CanonCamera");
	}

	void OnApplicationQuit()
	{
		Dispose();
		if (dllModule != IntPtr.Zero)
		{
			FreeLibrary(dllModule);
		}

		Marshal.FreeHGlobal(dataBuffer);
	}
}


public partial class CanonCamera
{

	public ICanonCameraDelegate canonCameraDelegate;

	private bool cameraInited = false;
	private bool openedSession = false;
	public bool EnabledLiveview { get; private set; }

	private string downloadPath;
	
	
	[DllImport("kernel32.dll")]
	private static extern IntPtr LoadLibrary(string moduleName);

	[DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
	private static extern IntPtr GetModuleHandle(string moduleName);
	
	[DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
	private static extern bool FreeLibrary(IntPtr module);

	[DllImport("CanonCamera", CallingConvention = CallingConvention.Cdecl, EntryPoint = "take_photo")]
	private static extern bool take_photo(SaveToMarker marker);
	
	[DllImport("CanonCamera", CallingConvention = CallingConvention.Cdecl, EntryPoint = "is_photo_ready")]
	private static extern bool is_photo_ready(StringBuilder str);

    [DllImport("CanonCamera", CallingConvention = CallingConvention.Cdecl, EntryPoint = "set_path")]
	private static extern void set_path([MarshalAs(UnmanagedType.LPStr)] string sText);

	[DllImport("CanonCamera", CallingConvention = CallingConvention.Cdecl, EntryPoint = "init_camera")]
    private static extern bool init_camera();

	[DllImport("CanonCamera", CallingConvention = CallingConvention.Cdecl, EntryPoint = "dispose")]
	private static extern void dispose();

	[DllImport("CanonCamera", CallingConvention = CallingConvention.Cdecl, EntryPoint = "start_liveview")]
	public static extern bool start_liveview();
	
	[DllImport("CanonCamera", CallingConvention = CallingConvention.Cdecl, EntryPoint = "is_liveview_ready")]
	public static extern bool is_liveview_ready();

	[DllImport("CanonCamera", CallingConvention = CallingConvention.Cdecl, EntryPoint = "stop_liveview")]
	public static extern bool stop_liveview();

	[DllImport("CanonCamera", CallingConvention = CallingConvention.Cdecl, EntryPoint = "open_session")]
	public static extern bool open_session();

	[DllImport("CanonCamera", CallingConvention = CallingConvention.Cdecl, EntryPoint = "close_session")]
	public static extern bool close_session();

	[System.Security.SuppressUnmanagedCodeSecurity]
	[DllImport("CanonCamera", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode, EntryPoint = "get_image_package")]
	public static extern void get_image_package(out IntPtr data, out int length);

	private void CheckPath()
	{
		if (downloadPath == null)
			throw new Exception("Unsetted path to put files");
	}

    public enum SaveToMarker { HOST, BOTH };

    public bool Inited
    {
        get { return cameraInited; }
    }

	public void Initialize()
	{
		cameraInited = init_camera();
	}

	public void OpenSession()
	{
		if (cameraInited)
		{
			openedSession = open_session();
		}
	}

	public void CloseSession()
	{
		if (cameraInited)
		{
			openedSession = !close_session();
		}
	}

	public void Dispose()
	{	
		if (cameraInited)
		{
			dispose();
		}
	}

	public void StartLiveview()
	{
		if (cameraInited && !EnabledLiveview)
		{
			EnabledLiveview = start_liveview();
			if (EnabledLiveview)
			{
				StartCoroutine(IsLiveviewReady());
			}
		}
	}

	private IEnumerator IsLiveviewReady()
	{
		while (!is_liveview_ready())
		{
			yield return null;
		}
		
		if (canonCameraDelegate != null)
		{
			canonCameraDelegate.LiveViewStarted();
		}
	}

	public void StopLiveview()
	{
		if (cameraInited && EnabledLiveview)
		{
			EnabledLiveview = !stop_liveview();
		}
	}

	public void SetPath(string path)
	{
		downloadPath = path;
		if (! Directory.Exists(path))
		{
			Directory.CreateDirectory(path);
		}
		set_path(path);
	}

	public void TakePhoto(SaveToMarker saveToMarker)
	{
		if (cameraInited)
		{
			CheckPath();
			bool result = take_photo(saveToMarker);
			if (result)
			{
				StartCoroutine(IsPhotoReady());
			}
		}
	}

	private IEnumerator IsPhotoReady()
	{
		StringBuilder str = new StringBuilder(50);
		while (!is_photo_ready(str))
		{
			yield return null;
		}	
		if (canonCameraDelegate != null)
		{
			canonCameraDelegate.PhotoDownloaded(downloadPath + str);
		}
	}

	public byte[] GetImagePackage()
	{
		if (cameraInited && EnabledLiveview)
		{
			CheckPath();
			int packLength = 0;
			byte[] buff = null;
			get_image_package(out dataBuffer, out packLength);
			if (packLength > 0)
			{
				buff = new byte[packLength];
				Marshal.Copy(dataBuffer, buff, 0, packLength);

			 	if (buff[0] == 255 &&  buff[1] == 216 &&						// marker of begining of JPEG
			 		buff[packLength - 2] == 255 && buff[packLength - 1] == 217) // marker of ending of JPEG
			 	{
					return buff;
				}
			}
		 }
		return null;
	}
}
