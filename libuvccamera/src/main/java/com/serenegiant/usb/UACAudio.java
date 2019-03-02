package com.serenegiant.usb;

import android.os.Handler;
import android.util.Log;

/**
 * Created by forrestluo on 2019/2/21.
 */

public class UACAudio {
    private static final String TAG = UACAudio.class.getSimpleName();

    static {
        System.loadLibrary("uacaudio");
    }

    private USBMonitor.UsbControlBlock mCtrBlock;

    /**
     * open uacaudio
     * @param ctrlBlock
     * @return
     */
    public synchronized int open(final USBMonitor.UsbControlBlock ctrlBlock) {
        mCtrBlock = ctrlBlock;
        return 0;
    }

    public native int setup();
    public native void close();
    public native void loop();
    public native boolean stop();
    public native int measure();
    public native String hellow();

    private boolean isRecord;
    public int startRecord(){
        if (isRecord){
            return 0;
        }
        int result = setup();
        if (result >= 0) {
            isRecord = true;
            new Thread(new Runnable() {
                public void run() {
                    loop();
                }
            }).start();
            return result;
        }else{

            return result;
        }
    }

    public void stopRecord(){
        Log.d(TAG, "Stop pressed");
        isRecord = false;
        stop();
        Handler h = new Handler();
        h.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (!isRecord) {
                    close();
                }
            }
        }, 100);
    }

    public boolean isRecord() {
        return isRecord;
    }
}
