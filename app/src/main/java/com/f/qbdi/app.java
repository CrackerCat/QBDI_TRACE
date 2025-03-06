package com.f.qbdi;

import android.app.Application;

import com.f.nativelib.NativeLib;

public class app extends Application {

    static {
        System.loadLibrary("nativelib");

    }

    @Override
    public void onCreate() {
        super.onCreate();

        new NativeLib().stringFromJNI();
    }

}
