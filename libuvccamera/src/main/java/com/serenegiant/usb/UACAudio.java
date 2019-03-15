package com.serenegiant.usb;

import android.text.TextUtils;
import android.util.Log;

/**
 * Created by forrestluo on 2019/2/21.
 */

public class UACAudio {
    private static final String TAG = UACAudio.class.getSimpleName();


    private static final String DEFAULT_USBFS = "/dev/bus/usb";

    private long nativePtr = 0;
    private USBMonitor.UsbControlBlock controlBlock;

    static {
        System.loadLibrary("uac");
    }

    public UACAudio(USBMonitor.UsbControlBlock controlBlock) {
        try {
            if(controlBlock != null) {
                this.controlBlock = controlBlock.clone();
                nativePtr = nativeGetDevice(
                        controlBlock.getVenderId(),
                        controlBlock.getProductId(),
                        controlBlock.getFileDescriptor(),
                        controlBlock.getBusNum(),
                        controlBlock.getDevNum(),
                        controlBlock.getSerial(),
                        getUSBFSName(controlBlock));
            }
        } catch (final Exception e) {
            e.printStackTrace();
        }
    }

    public boolean isValidAudioDevice() {
        return nativePtr != 0 && controlBlock != null;
    }

    public synchronized int open() {
        Log.d(TAG, "open device");
        if(nativePtr == 0) {
            Log.e(TAG, "invalid device");
            return -1;
        }

        return nativeOpenDevice(nativePtr);
    }

    public synchronized int close() {
        Log.d(TAG, "close device");
        return nativeCloseDevice(nativePtr);
    }

//
//    public synchronized int startRecord(String path) {
//        Log.d(TAG, "startRecord");
//        return nativeStartRecord(cptr, path);
//    }
//
//    public synchronized int stopRecord() {
//        Log.d(TAG, "stopRecord");
//        return nativeStopRecord(cptr);
//    }

    public synchronized void setAudioStreamCallback(IAudioStreamCallback cb) {
        Log.d(TAG, "setAudioStreamCallback, cb: " + cb);
        nativeSetAudioStreamCallback(nativePtr, cb);
    }

    private final String getUSBFSName(final USBMonitor.UsbControlBlock ctrlBlock) {
        String result = null;
        final String name = ctrlBlock.getDeviceName();
        final String[] v = !TextUtils.isEmpty(name) ? name.split("/") : null;
        if ((v != null) && (v.length > 2)) {
            final StringBuilder sb = new StringBuilder(v[0]);
            for (int i = 1; i < v.length - 2; i++)
                sb.append("/").append(v[i]);
            result = sb.toString();
        }
        if (TextUtils.isEmpty(result)) {
            Log.w(TAG, "failed to get USBFS path, try to use default path:" + name);
            result = DEFAULT_USBFS;
        }
        return result;
    }

    private native int nativeInit(String usbfs);
    private native long nativeGetDevice(int vid, int pid, int fd, int busnum, int devaddr, String sn, String usbfs);
    private native int nativeOpenDevice(long nativeptr);
    private native int nativeCloseDevice(long nativeptr);
    private native int nativeStartRecord(long nativeptr, String path);
    private native int nativeStopRecord(long nativeptr);
    private native void nativeSetAudioStreamCallback(long nativeptr, IAudioStreamCallback cb);
}
