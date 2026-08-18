package org.sauerquest.vr;

import android.app.Activity;
import android.view.Surface;

// JNI bridge to libsauerquest.so. Method set mirrors QuakeQuest's
// GLES3JNILib.java (github.com/Team-Beef-Studios/QuakeQuest, GPLv2).
public class SauerQuestJNILib
{
	public static native long onCreate( Activity obj );
	public static native void onStart( long handle, Object obj );
	public static native void onResume( long handle );
	public static native void onPause( long handle );
	public static native void onStop( long handle );
	public static native void onDestroy( long handle );

	public static native void onSurfaceCreated( long handle, Surface s );
	public static native void onSurfaceChanged( long handle, Surface s );
	public static native void onSurfaceDestroyed( long handle );
}
