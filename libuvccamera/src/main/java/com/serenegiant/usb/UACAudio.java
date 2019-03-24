package com.serenegiant.usb;

import android.text.TextUtils;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

/**
 * Created by forrestluo on 2019/2/21.
 */

public class UACAudio {
    private static final String TAG = UACAudio.class.getSimpleName();


    private static final String DEFAULT_USBFS = "/dev/bus/usb";

    private long nativePtr = 0;
    private USBMonitor.UsbControlBlock controlBlock;

    // control params
    private boolean isMuteAvailable = false;
    private boolean isMute = false;

    private int sampleRate = 0;
    private String supportedSampleRates = "";

    private int volume = 0;
    private int minVolume = 0;
    private int maxVolume = 100;
    private boolean isVolumeAvailable = false;

    private int bitResolution = 0;
    private int channelCount = 0;

    private boolean isOpened = false;

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

                if(nativePtr != 0) {
                    sampleRate = nativeGetSampleRate(nativePtr);
                    supportedSampleRates = nativeGetSupportSampleRates(nativePtr);
                    isMuteAvailable = nativeIsMuteAvailable(nativePtr);
                    isVolumeAvailable = nativeIsVolumeAvailable(nativePtr);
                    bitResolution = nativeGetBitResolution(nativePtr);
                    channelCount = nativeGetChannelCount(nativePtr);
                }
            }
        } catch (final Exception e) {
            e.printStackTrace();
        }
    }

    public boolean isValidAudioDevice() {
        return nativePtr != 0 && controlBlock != null;
    }

    public int getDeviceID() {
        if(controlBlock != null) return controlBlock.getDeviceId();

        return -1;
    }

    public String getDeviceName() {
        if(controlBlock != null) return controlBlock.getDeviceName();

        return "UAC AUDIO MIC";
    }

    public synchronized int open() {
        Log.i(TAG, "open device");
        if(nativePtr == 0) {
            Log.e(TAG, "invalid device");
            return -1;
        }

        int ret = nativeOpenDevice(nativePtr);
        if(ret != 0) {
            Log.e(TAG, "open device failed, ret: " + ret);
            return ret;
        }

        isMute = nativeIsMute(nativePtr);
        volume = nativeGetVolume(nativePtr);
        maxVolume = nativeGetMaxVolume(nativePtr);
        minVolume = nativeGetMinVolume(nativePtr);

        isOpened = true;

        return 0;
    }

    public synchronized int close() {
        Log.i(TAG, "close device");

        if (controlBlock != null) {
            controlBlock.close();
            controlBlock = null;
        }

        int ret = nativeCloseDevice(nativePtr);
        if(ret != 0) {
            Log.e(TAG, "close device failed");
            return ret;
        }

        isOpened = false;

        return 0;
    }

    public synchronized int getBitResolution() {
        Log.v(TAG, "getBitResolution");

        return bitResolution;
    }

    public synchronized List<Integer> getSupportSampleRates() {
        Log.v(TAG, "getSupportSampleRate");

        return parseSampleRates(supportedSampleRates);
    }

    public synchronized int getSampleRate() {
        Log.v(TAG, "getSampleRate");

        return sampleRate;
    }

    public synchronized int setSampleRate(int sampleRate) {

        Log.i(TAG, "setSampleRate " + sampleRate);

        int ret = nativeSetSampleRate(nativePtr, sampleRate);
        if(ret != 0) return ret;

        this.sampleRate = sampleRate;
        return 0;
    }

    public synchronized boolean isMuteAvailable() {
        Log.v(TAG, "isMuteAvailable");

        return isMuteAvailable;
    }
    
    public synchronized boolean isMute() {
        Log.v(TAG, "isMute");
        return isMute;
    }

    public synchronized int setMute(boolean isMute) {
        Log.i(TAG, "setMute " + isMute);

        int ret = nativeSetMute(nativePtr, isMute);
        if(ret != 0) return ret;

        this.isMute = isMute;

        return 0;
    }

    public synchronized int getChannelCount() {
        Log.v(TAG, "getChannelCount");

        return channelCount;
    }

    public synchronized boolean isVolumeAvailable() {
        Log.v(TAG, "isVolumeAvailable");

        return isVolumeAvailable;
    }

    public synchronized int getVolume() {
        Log.v(TAG, "getVolume, " + volume);
        return volume;
    }

    public synchronized int getMinVolume() {
        Log.v(TAG, "getMinVolume, " + minVolume);
        return minVolume;
    }

    public synchronized int getMaxVolume() {
        Log.v(TAG, "getMaxVolume, " + maxVolume);
        return maxVolume;
    }

    public synchronized int setVolume(int volume) {
        Log.i(TAG, "setVolume, " + volume);

        if(volume < 0) {
            volume = 0;
        }

        if(volume > maxVolume) {
            volume = maxVolume;
        }

        int ret = nativeSetVolume(nativePtr, volume);
        if(ret != 0) return ret;

        this.volume = volume;

        return 0;
    }

    public synchronized boolean isOpened() {
        return isOpened;
    }
//
//    public synchronized int startRecord(String path) {
//        Log.v(TAG, "startRecord");
//        return nativeStartRecord(cptr, path);
//    }
//
//    public synchronized int stopRecord() {
//        Log.v(TAG, "stopRecord");
//        return nativeStopRecord(cptr);
//    }

    public synchronized void setAudioStreamCallback(IAudioStreamCallback cb) {
        Log.v(TAG, "setAudioStreamCallback, cb: " + cb);
        nativeSetAudioStreamCallback(nativePtr, cb);
    }

    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("[")
            .append("supportSampleRates:").append(supportedSampleRates).append(",")
            .append("sampleRate:").append(sampleRate).append(",")
            .append("bitResolution:").append(bitResolution).append(",")
            .append("channelCount:").append(channelCount).append(",")
            .append("isMuteAvailable:").append(isMuteAvailable).append(",")
            .append("isMute:").append(isMute).append(",")
            .append("isVolumeAvailable:").append(isVolumeAvailable).append(",")
            .append("minVolume:").append(minVolume).append(",")
            .append("volume:").append(volume).append(",")
            .append("maxVolume:").append(maxVolume)
            .append("]");

        return sb.toString();
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

    private List<Integer> parseSampleRates(String rates) {
        List<Integer> sampleRates = new ArrayList<>();

        if(!TextUtils.isEmpty(rates)) {
            String[] rs = rates.split(",");
            for (String s : rs) {
                if (TextUtils.isDigitsOnly(s)) {
                    sampleRates.add(Integer.valueOf(s));
                }
            }
        }

        return sampleRates;
    }

    // device operation
    private native int nativeInit(String usbfs);
    private native long nativeGetDevice(int vid, int pid, int fd, int busnum, int devaddr, String sn, String usbfs);
    private native int nativeOpenDevice(long nativeptr);
    private native int nativeCloseDevice(long nativeptr);
    private native void nativeSetAudioStreamCallback(long nativeptr, IAudioStreamCallback cb);

    // record function
    private native int nativeStartRecord(long nativeptr, String path);
    private native int nativeStopRecord(long nativeptr);

    // bit resolution
    private native int nativeGetBitResolution(long nativePtr);

    // channel count
    private native int nativeGetChannelCount(long nativePtr);

    // sample rate
    private native int nativeGetSampleRate(long nativePtr);
    private native int nativeSetSampleRate(long nativePtr, int sampleRate);
    private native String nativeGetSupportSampleRates(long nativePtr);

    // mute
    private native boolean nativeIsMuteAvailable(long nativePtr);
    private native boolean nativeIsMute(long nativePtr);
    private native int nativeSetMute(long nativePtr, boolean isMute);

    // volume
    private native boolean nativeIsVolumeAvailable(long nativePtr);
    private native int nativeGetVolume(long nativePtr);
    private native int nativeSetVolume(long nativePtr, int volume);
    private native int nativeGetMaxVolume(long nativePtr);
    private native int nativeGetMinVolume(long nativePtr);
}
