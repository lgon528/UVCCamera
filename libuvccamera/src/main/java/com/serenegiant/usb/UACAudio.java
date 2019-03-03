package com.serenegiant.usb;

import android.os.Handler;
import android.util.Log;

/**
 * Created by forrestluo on 2019/2/21.
 */

public class UACAudio {
    private static final String TAG = UACAudio.class.getSimpleName();

    static {
        System.loadLibrary("uac");
    }

    private long cptr;
    private USBMonitor.UsbControlBlock mCtrBlock;

    public UACAudio(USBMonitor.UsbControlBlock ctrBlock) {
        mCtrBlock = ctrBlock;
    }

    public synchronized int init(){
        Log.d(TAG, "init");
        int ret = nativeInit();
        if(ret != 0) {
            Log.e(TAG, "libuac init failed");
            return ret;
        }

        cptr = nativeGetDevice(mCtrBlock.getVenderId(), mCtrBlock.getProductId(),
                mCtrBlock.getFileDescriptor(), mCtrBlock.getSerial());

        if(cptr == 0) {
            Log.e(TAG, "libuac device not found");
            return -1;
        }

        return 0;
    }

    public synchronized int open() {
        Log.d(TAG, "open device");
        return nativeOpenDevice(cptr);
    }


    public synchronized int startRecord(String path) {
        Log.d(TAG, "startRecord");
        return nativeStartRecord(cptr, path);
    }

    public synchronized int stopRecord() {
        Log.d(TAG, "stopRecord");
        return nativeStopRecord(cptr);
    }

    public synchronized void setAudioStreamCallback(IAudioStreamCallback cb) {
        Log.d(TAG, "setAudioStreamCallback, cb: " + cb);
        nativeSetAudioStreamCallback(cptr, cb);
    }

    private native int nativeInit();
    private native long nativeGetDevice(int vid, int pid, int fd, String sn);
    private native int nativeOpenDevice(long devPtr);
    private native int nativeStartRecord(long devPtr, String path);
    private native int nativeStopRecord(long devPtr);
    private native void nativeSetAudioStreamCallback(long devPtr, IAudioStreamCallback cb);
}
