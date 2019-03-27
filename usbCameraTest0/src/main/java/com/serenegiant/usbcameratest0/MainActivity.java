/*
 *  UVCCamera
 *  library and sample to access to UVC web camera on non-rooted Android device
 *
 * Copyright (c) 2014-2017 saki t_saki@serenegiant.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 *  All files in the folder are under this Apache License, Version 2.0.
 *  Files in the libjpeg-turbo, libusb, libuvc, rapidjson folder
 *  may have a different license, see the respective files.
 */

package com.serenegiant.usbcameratest0;

import android.hardware.usb.UsbDevice;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;
import android.widget.Toast;

import com.serenegiant.common.BaseActivity;
import com.serenegiant.usb.CameraDialog;
import com.serenegiant.usb.IAudioStreamCallback;
import com.serenegiant.usb.UACAudio;
import com.serenegiant.usb.USBMonitor;
import com.serenegiant.usb.USBMonitor.OnDeviceConnectListener;
import com.serenegiant.usb.USBMonitor.UsbControlBlock;
import com.serenegiant.usb.UVCCamera;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.ByteBuffer;

public class MainActivity extends BaseActivity implements CameraDialog.CameraDialogParent {
	private static final boolean DEBUG = true;	// TODO set false when production
	private static final String TAG = "MainActivity";

    private final Object mSync = new Object();
    // for accessing USB and USB camera
    private USBMonitor mUSBMonitor;
	private UVCCamera mUVCCamera;
	private SurfaceView mUVCCameraView;
	// for open&start / stop&close camera preview
	private ImageButton mCameraButton;
	private Surface mPreviewSurface;
	private int mPreviewWidth = UVCCamera.DEFAULT_PREVIEW_WIDTH;
	private int mPreviewHeight = UVCCamera.DEFAULT_PREVIEW_HEIGHT;
	private boolean isActive, isPreview;

	private UACAudio mUACAudio;
	private int sampleRate = 48000;
	private int bufferSizeInBytes = 0;
	private AudioTrack audioTrack;
	private ByteBuffer byteBuffer;
	private String filePath;

	@Override
	protected void onCreate(final Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		mCameraButton = (ImageButton)findViewById(R.id.camera_button);
		mCameraButton.setOnClickListener(mOnClickListener);

		mUVCCameraView = (SurfaceView)findViewById(R.id.camera_surface_view);
		mUVCCameraView.getHolder().addCallback(mSurfaceViewCallback);

		mUSBMonitor = new USBMonitor(this, mOnDeviceConnectListener);

		//最小缓存区
		bufferSizeInBytes = AudioTrack.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT);
		byteBuffer = ByteBuffer.allocate(bufferSizeInBytes);
		byteBuffer.clear();
		//创建AudioTrack对象   依次传入 :流类型、采样率（与采集的要一致）、音频通道（采集是IN 播放时OUT）、量化位数、最小缓冲区、模式
		audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC,sampleRate,AudioFormat.CHANNEL_OUT_MONO,AudioFormat.ENCODING_PCM_16BIT, bufferSizeInBytes, AudioTrack.MODE_STREAM);
	}

	@Override
	protected void onStart() {
		super.onStart();
		if (DEBUG) Log.v(TAG, "onStart:");
		synchronized (mSync) {
			if (mUSBMonitor != null) {
				mUSBMonitor.register();
			}
		}
	}

	@Override
	protected void onStop() {
		if (DEBUG) Log.v(TAG, "onStop:");
		synchronized (mSync) {
			if (mUSBMonitor != null) {
				mUSBMonitor.unregister();
			}
		}
		super.onStop();
	}

	@Override
	protected void onDestroy() {
		if (DEBUG) Log.v(TAG, "onDestroy:");
		synchronized (mSync) {
			isActive = isPreview = false;
			if (mUVCCamera != null) {
				mUVCCamera.destroy();
				mUVCCamera = null;
			}
			if (mUSBMonitor != null) {
				mUSBMonitor.destroy();
				mUSBMonitor = null;
			}

			if(mPreviewSurface != null) {
				mPreviewSurface.release();
				mPreviewSurface = null;
			}

			if(mUACAudio != null) {
				mUACAudio.close();
				mUACAudio = null;
			}
		}

		mUVCCameraView = null;
		mCameraButton = null;
		super.onDestroy();
	}

	private int count = 0;
	private final OnClickListener mOnClickListener = new OnClickListener() {
		@Override
		public void onClick(final View view) {
			Log.i(TAG,"we're here, cur filepath: " + filePath);
//			filePath = Environment.getExternalStorageDirectory().getPath() + "/justfortest/1551755170047";
//			playAudio();
			if (mUVCCamera == null && mUACAudio == null) {
				// XXX calling CameraDialog.showDialog is necessary at only first time(only when app has no permission).
				CameraDialog.showDialog(MainActivity.this);
			} else {
				synchronized (mSync) {
					if(mUVCCamera != null) {
						mUVCCamera.destroy();
						mUVCCamera = null;
						isActive = isPreview = false;
					}

					if(mUACAudio != null) {
						mUACAudio.close();
						mUACAudio = null;
					}

//					playAudio();
					audioTrack.stop();
				}
			}
		}
	};

	private void playAudio(){
		try {
			int chunklen = 2 * bufferSizeInBytes;
			byte[] chunk = new byte[chunklen];
			int sum = 0;

			FileInputStream inputStream = new FileInputStream(filePath);

			audioTrack.play();
			while(true) {
				try {
					int len = inputStream.read(chunk);
					Log.e(TAG, "we're here, len: " + len);
					if(len < 0) break;

					sum += len;

					audioTrack.write(chunk, 0, len);
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
			audioTrack.stop();
			Log.e(TAG, "we're here, totalSize: " + sum);
			Toast.makeText(MainActivity.this, "play audio finished", Toast.LENGTH_SHORT).show();
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		}
	}

	private final OnDeviceConnectListener mOnDeviceConnectListener = new OnDeviceConnectListener() {
		@Override
		public void onAttach(final UsbDevice device) {
			if (DEBUG) Log.v(TAG, "onAttach:" + device);
			Toast.makeText(MainActivity.this, "USB_DEVICE_ATTACHED", Toast.LENGTH_SHORT).show();
		}

		@Override
		public void onConnect(final UsbDevice device, final UsbControlBlock ctrlBlock, final boolean createNew) {
			if (DEBUG) Log.v(TAG, "onConnect:" + device);

			synchronized (mSync) {
				if (mUVCCamera != null) {
					mUVCCamera.destroy();
				}
				isActive = isPreview = false;
			}
			queueEvent(new Runnable() {
				@Override
				public void run() {
					synchronized (mSync) {

						UACAudio audio = new UACAudio(ctrlBlock);
						if(audio.isValidAudioDevice()) {
							int ret = audio.open();
							if (ret != 0) {
								Log.e(TAG, "open audio failed, ret: " + ret);
								return;
							}
							Log.e(TAG, "we're here, device: " + audio);
							audioTrack.play();
							audio.setAudioStreamCallback(new IAudioStreamCallback() {
								@Override
								public void onStreaming(byte[] data) {
//									Log.i(TAG, "onStreaming, size: " + data.length);
//									Log.i(TAG, "onStreaming, content: " + byte2hex(data));
//									byte[] frame = data;
									byte[] frame = stereo2mono(data);
//									Log.i(TAG, "onStreaming, frame: " + byte2hex(frame));

									audioTrack.write(frame, 0, frame.length);
								}
							});

							mUACAudio = audio;
						}

						final UVCCamera camera = new UVCCamera(ctrlBlock);
						boolean isCamera = camera.isUVCDevice();
						if(!isCamera) return;

						camera.open();
						if (DEBUG) Log.i(TAG, "supportedSize:" + camera.getSupportedSize());
						mPreviewWidth = camera.getSupportedSizeList().get(0).width;
						mPreviewHeight = camera.getSupportedSizeList().get(0).height;
						try {
							camera.setPreviewSize(mPreviewWidth, mPreviewHeight, UVCCamera.FRAME_FORMAT_MJPEG);
						} catch (final IllegalArgumentException e) {
							try {
								// fallback to YUV mode
								camera.setPreviewSize(mPreviewWidth, mPreviewHeight, UVCCamera.DEFAULT_PREVIEW_MODE);
							} catch (final IllegalArgumentException e1) {
								Log.e(TAG, "setPreviewSize exception");
								e1.printStackTrace();
								camera.destroy();
								return;
							}
						}
						mPreviewSurface = mUVCCameraView.getHolder().getSurface();
						if (mPreviewSurface != null) {
							isActive = true;
							camera.setPreviewDisplay(mPreviewSurface);
							camera.startPreview();
							isPreview = true;
						}
						synchronized (mSync) {
							mUVCCamera = camera;
						}


						filePath = Environment.getExternalStorageDirectory().getPath() + "/justfortest/" + System.currentTimeMillis();
						File file = new File(filePath);
						if(!file.exists()) {
							try {
								file.createNewFile();
							} catch (IOException e) {
								e.printStackTrace();
							}
						}




					}
				}
			}, 0);
		}

		@Override
		public void onDisconnect(final UsbDevice device, final UsbControlBlock ctrlBlock) {
			if (DEBUG) Log.v(TAG, "onDisconnect:" + device);
			// XXX you should check whether the comming device equal to camera device that currently using
			queueEvent(new Runnable() {
				@Override
				public void run() {
					synchronized (mSync) {
						if (mUVCCamera != null) {
							mUVCCamera.close();
							if (mPreviewSurface != null) {
//								mPreviewSurface.release();
								mPreviewSurface = null;
							}
							isActive = isPreview = false;

							mUVCCamera = null;
						}
					}
				}
			}, 0);
		}

		@Override
		public void onDettach(final UsbDevice device) {
			if (DEBUG) Log.v(TAG, "onDettach:" + device);
			Toast.makeText(MainActivity.this, "USB_DEVICE_DETACHED", Toast.LENGTH_SHORT).show();
		}

		@Override
		public void onCancel(final UsbDevice device) {
		}
	};

	/**
	 * to access from CameraDialog
	 * @return
	 */
	@Override
	public USBMonitor getUSBMonitor() {
		return mUSBMonitor;
	}

	@Override
	public void onDialogResult(boolean canceled) {
		if (canceled) {
			runOnUiThread(new Runnable() {
				@Override
				public void run() {
					// FIXME
				}
			}, 0);
		}
	}

	private final SurfaceHolder.Callback mSurfaceViewCallback = new SurfaceHolder.Callback() {
		@Override
		public void surfaceCreated(final SurfaceHolder holder) {
			if (DEBUG) Log.v(TAG, "surfaceCreated:");
		}

		@Override
		public void surfaceChanged(final SurfaceHolder holder, final int format, final int width, final int height) {
			if ((width == 0) || (height == 0)) return;
			if (DEBUG) Log.v(TAG, "surfaceChanged:");
			mPreviewSurface = holder.getSurface();
			synchronized (mSync) {
				if (isActive && !isPreview && (mUVCCamera != null) && mPreviewSurface != null) {
					mUVCCamera.setPreviewDisplay(mPreviewSurface);
					mUVCCamera.startPreview();
					isPreview = true;
				}
			}
		}

		@Override
		public void surfaceDestroyed(final SurfaceHolder holder) {
			if (DEBUG) Log.v(TAG, "surfaceDestroyed:");
			synchronized (mSync) {
				if (mUVCCamera != null) {
					mUVCCamera.stopPreview();
				}
				isPreview = false;
			}
			mPreviewSurface = null;
		}
	};

	private static final char[] digits = new char[]{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
	/**
	 * 二进制转化为十六进制字符串
	 * @param buffer
	 * @return
	 */
	public synchronized String byte2hex(byte [] buffer) {
		StringBuilder strBuilder = new StringBuilder();
		if (buffer != null && buffer.length != 0) {
			char ch;
			char cl;

			for (int i = 0; i < buffer.length; ++i) {
				byte b = buffer[i];
				cl = digits[b & 15];
				b = (byte) (b >>> 4);
				ch = digits[b & 15];

				strBuilder.append(ch).append(cl);
			}

			return strBuilder.toString();
		} else {
			return null;
		}
	}


	private byte[] stereo2mono(byte[] src) {

		byte[] dst = new byte[src.length/2];
		for(int i = 0; i < src.length/4; i++) {
			dst[2*i] = src[4*i];
			dst[2*i+1] = src[4*i + 1];
		}

		return dst;
	}

	private byte[] stereo2mono2(byte[] src) {

		ByteBuffer sbf = ByteBuffer.allocate(src.length);
		sbf.put(src);

		byte[] dst = new byte[src.length/2];
		sbf.get(dst, 0, src.length/2);

		return dst;
	}

	private byte[] mono2stereo(byte[] src) {

		ByteBuffer sbf = ByteBuffer.allocate(src.length);
		sbf.put(src);

		ByteBuffer dst = ByteBuffer.allocate(src.length*2);
		while(sbf.remaining() > 0) {
			byte b1 = sbf.get();
			byte b2 = sbf.get();
			dst.put(b1);
			dst.put(b2);
			dst.put(b1);
			dst.put(b2);
		}

		return dst.array();
	}

}
