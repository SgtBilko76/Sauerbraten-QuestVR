package org.sauerquest.vr;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

// Lifecycle/JNI bridge Activity, structurally adapted from QuakeQuest's
// GLES3JNIActivity.java (github.com/Team-Beef-Studios/QuakeQuest, GPLv2).
// Phase 1 trims out asset-copying and storage-permission handling, which
// belong to Phase 9 once there is real Sauerbraten game data to package.
@SuppressLint("SdCardPath")
public class SauerQuestActivity extends Activity implements SurfaceHolder.Callback
{
	private static final String TAG = "SauerQuest";

	static {
		try {
			System.loadLibrary("openxr_loader");
		} catch (Throwable e) {
			Log.w(TAG, "openxr_loader not preloaded (expected: it's dlopen'd at runtime)", e);
		}
		System.loadLibrary("sauerquest");
	}

	private SurfaceHolder mSurfaceHolder;
	private long mNativeHandle;

	// SDL2's Android backend (third_party/SDL2/src/core/android/SDL_android.c)
	// resolves a handful of platform services (storage paths, clipboard,
	// toast, etc.) via static methods it looks up by name on whatever
	// Activity class calls its nativeSetupJNI(). Upstream SDL apps get this
	// for free because they subclass org.libsdl.app.SDLActivity, which
	// implements the full set; this port uses its own Activity instead, so
	// nativeSetupJNI() is invoked directly from native code (see
	// vr_glue/sauerquest_vr_bootstrap.c's onCreate handler) against this
	// class. getContext() is the one static method actually exercised by
	// this port's code paths so far (SDL_AndroidGetInternalStoragePath(),
	// needed by every relative-path file load) -- the many other lookups
	// SDL_android.c performs against methods this class doesn't implement
	// are expected to fail (SDL_android.c's nativeSetupJNI clears the
	// resulting exceptions instead of leaving them pending).
	private static SauerQuestActivity sInstance;
	public static android.content.Context getContext() { return sInstance; }

	@Override protected void onCreate(Bundle icicle)
	{
		Log.v(TAG, "SauerQuestActivity::onCreate()");
		super.onCreate(icicle);

		sInstance = this;

		// Sauerbraten's own file I/O (src/shared/stream.cpp's openfile())
		// uses plain fopen() with paths relative to the process's working
		// directory, not SDL_RWFromFile() -- so it has no idea APK assets
		// exist at all (only SDL2_image's IMG_Load, used for texture
		// loading, goes through SDL_RWFromFile's asset-manager fallback).
		// Extract the bundled data/packages assets to internal storage
		// once at startup, then chdir() there in native code (see
		// vr_glue/sauerquest_vr_bootstrap.c's AppThreadFunction, right
		// before it calls android_sauer_main()) so every relative fopen()
		// the engine does just works against real files.
		copyAssetTree("data", getFilesDir());
		copyAssetTree("packages", getFilesDir());

		SurfaceView view = new SurfaceView(this);
		setContentView(view);
		view.getHolder().addCallback(this);

		mNativeHandle = SauerQuestJNILib.onCreate(this);
	}

	// Recursively copies assetSubdir (and everything under it) from the
	// APK's assets/ into destDir/assetSubdir. AssetManager.list() (unlike
	// the native AAssetManager C API) reliably enumerates subdirectories,
	// which is why this runs on the Java side rather than in native code.
	private void copyAssetTree(String assetSubdir, File destDir)
	{
		AssetManager assets = getAssets();
		try {
			String[] entries = assets.list(assetSubdir);
			if (entries == null || entries.length == 0) {
				// A leaf file, not a directory: list() returns empty for those.
				File outFile = new File(destDir, assetSubdir);
				outFile.getParentFile().mkdirs();
				try (InputStream in = assets.open(assetSubdir);
				     OutputStream out = new FileOutputStream(outFile)) {
					byte[] buf = new byte[64 * 1024];
					int n;
					while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
				}
				return;
			}
			for (String entry : entries) {
				copyAssetTree(assetSubdir + "/" + entry, destDir);
			}
		} catch (IOException e) {
			Log.e(TAG, "copyAssetTree failed for " + assetSubdir, e);
		}
	}

	@Override protected void onStart()
	{
		super.onStart();
		if (mNativeHandle != 0) SauerQuestJNILib.onStart(mNativeHandle, this);
	}

	@Override protected void onResume()
	{
		super.onResume();
		if (mNativeHandle != 0) SauerQuestJNILib.onResume(mNativeHandle);
	}

	@Override protected void onPause()
	{
		if (mNativeHandle != 0) SauerQuestJNILib.onPause(mNativeHandle);
		super.onPause();
	}

	@Override protected void onStop()
	{
		if (mNativeHandle != 0) SauerQuestJNILib.onStop(mNativeHandle);
		super.onStop();
	}

	@Override protected void onDestroy()
	{
		if (mSurfaceHolder != null && mNativeHandle != 0) {
			SauerQuestJNILib.onSurfaceDestroyed(mNativeHandle);
		}
		if (mNativeHandle != 0) SauerQuestJNILib.onDestroy(mNativeHandle);
		super.onDestroy();
		mNativeHandle = 0;
	}

	@Override public void surfaceCreated(SurfaceHolder holder)
	{
		if (mNativeHandle != 0) {
			SauerQuestJNILib.onSurfaceCreated(mNativeHandle, holder.getSurface());
			mSurfaceHolder = holder;
		}
	}

	@Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
	{
		if (mNativeHandle != 0) {
			SauerQuestJNILib.onSurfaceChanged(mNativeHandle, holder.getSurface());
			mSurfaceHolder = holder;
		}
	}

	@Override public void surfaceDestroyed(SurfaceHolder holder)
	{
		if (mNativeHandle != 0) {
			SauerQuestJNILib.onSurfaceDestroyed(mNativeHandle);
			mSurfaceHolder = null;
		}
	}

	// Called from native VR_Shutdown() via JNI CallVoidMethod.
	public void shutdown()
	{
		finishAffinity();
		System.exit(0);
	}
}
