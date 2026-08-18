package org.sauerquest.vr;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

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

	@Override protected void onCreate(Bundle icicle)
	{
		Log.v(TAG, "SauerQuestActivity::onCreate()");
		super.onCreate(icicle);

		SurfaceView view = new SurfaceView(this);
		setContentView(view);
		view.getHolder().addCallback(this);

		mNativeHandle = SauerQuestJNILib.onCreate(this);
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
