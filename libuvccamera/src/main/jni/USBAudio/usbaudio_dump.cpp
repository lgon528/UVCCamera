//
// Created by forrestluo on 2019/2/21.
//

/*
 *
 * Dumb userspace USB Audio receiver
 * Copyright 2012 Joel Stanley <joel@jms.id.au>
 *
 * Based on the following:
 *
 * libusb example program to measure Atmel SAM3U isochronous performance
 * Copyright (C) 2012 Harald Welte <laforge@gnumonks.org>
 *
 * Copied with the author's permission under LGPL-2.1 from
 * http://git.gnumonks.org/cgi-bin/gitweb.cgi?p=sam3u-tests.git;a=blob;f=usb-benchmark-project/host/benchmark.c;h=74959f7ee88f1597286cd435f312a8ff52c56b7e
 *
 * An Atmel SAM3U test firmware is also available in the above repository.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <jni.h>

#include <android/log.h>
#include "libusb/libusb/libusb.h"
#include "libusb/libusb/libusbi.h"

/*
#define LOGD(...) \
    __android_log_print(ANDROID_LOG_DEBUG, "UsbAudioNative", __VA_ARGS__)
*/

//#define UNUSED __attribute__((unused))

/* The first PCM stereo AudioStreaming endpoint. */
#define EP_ISO_IN	0x82
#define IFACE_NUM   2

static int do_exit = 1;
static struct libusb_device_handle *devh = NULL;

static unsigned long num_bytes = 0, num_xfer = 0;
static struct timeval tv_start;

static JavaVM* java_vm = NULL;

static jclass com_qcymall_recorder_usbaudio_AudioPlayback = NULL;
static jmethodID com_qcymall_recorder_usbaudio_AudioPlayback_write;

static void cb_xfr(struct libusb_transfer *xfr)
{
	unsigned int i;

    int len = 0;

    // Get an env handle
    JNIEnv * env;
    void * void_env;
    bool had_to_attach = false;
    jint status = java_vm->GetEnv(&void_env, JNI_VERSION_1_6);

    if (status == JNI_EDETACHED) {
        java_vm->AttachCurrentThread(&env, NULL);
        had_to_attach = true;
    } else {
        env = void_env;
    }

    // Create a jbyteArray.
    int start = 0;
    jbyteArray audioByteArray = (*env)->NewByteArray(env, 200 * xfr->num_iso_packets);

    for (i = 0; i < xfr->num_iso_packets; i++) {
        struct libusb_iso_packet_descriptor *pack = &xfr->iso_packet_desc[i];

        if (pack->status != LIBUSB_TRANSFER_COMPLETED) {
            LOGD("Error (status %d: %s) :", pack->status,
                    libusb_error_name(pack->status));
            /* This doesn't happen, so bail out if it does. */
            return;
//            exit(EXIT_FAILURE);
        }

        const uint8_t *data = libusb_get_iso_packet_buffer_simple(xfr, i);
//        writeToFile22(data, pack->actual_length);
        (*env)->SetByteArrayRegion(env, audioByteArray, len, pack->actual_length, data);
//        LOGD("JNI DataLength = %d; len = %d", pack->length, pack->actual_length);
        len += pack->actual_length;
    }

    // Call write()
    (*env)->CallStaticVoidMethod(env, com_qcymall_recorder_usbaudio_AudioPlayback,
            com_qcymall_recorder_usbaudio_AudioPlayback_write, audioByteArray, len);
    (*env)->DeleteLocalRef(env, audioByteArray);
    if ((*env)->ExceptionCheck(env)) {
        LOGD("Exception while trying to pass sound data to java");
        return;
    }

	num_bytes += len;
	num_xfer++;

    if (had_to_attach) {
        java_vm->DetachCurrentThread(java_vm);
    }


	if (libusb_submit_transfer(xfr) < 0) {
		LOGD("error re-submitting URB\n");
		exit(1);
	}
}


#define NUM_TRANSFERS 10
#define PACKET_SIZE 200
#define NUM_PACKETS 10

static int benchmark_in(uint8_t ep)
{
	static uint8_t buf[PACKET_SIZE * NUM_PACKETS];
	static struct libusb_transfer *xfr[NUM_TRANSFERS];
	int num_iso_pack = NUM_PACKETS;
    int i;
    int rc;


	/* NOTE: To reach maximum possible performance the program must
	 * submit *multiple* transfers here, not just one.
	 *
	 * When only one transfer is submitted there is a gap in the bus
	 * schedule from when the transfer completes until a new transfer
	 * is submitted by the callback. This causes some jitter for
	 * isochronous transfers and loss of throughput for bulk transfers.
	 *
	 * This is avoided by queueing multiple transfers in advance, so
	 * that the host controller is always kept busy, and will schedule
	 * more transfers on the bus while the callback is running for
	 * transfers which have completed on the bus.
	 */
    for (i=0; i<NUM_TRANSFERS; i++) {
        xfr[i] = libusb_alloc_transfer(num_iso_pack);
        if (!xfr[i]) {
            LOGD("Could not allocate transfer");
            return -ENOMEM;
        }

        libusb_fill_iso_transfer(xfr[i], devh, ep, buf,
                sizeof(buf), num_iso_pack, cb_xfr, NULL, 1000);
        libusb_set_iso_packet_lengths(xfr[i], sizeof(buf)/num_iso_pack);

        rc = libusb_submit_transfer(xfr[i]);
        LOGD("libusb_submit_transfer %d %s", i, libusb_error_name(rc));
    }

	gettimeofday(&tv_start, NULL);

    return 1;
}

unsigned int measure(void)
{
	struct timeval tv_stop;
	unsigned int diff_msec;

	gettimeofday(&tv_stop, NULL);

	diff_msec = (tv_stop.tv_sec - tv_start.tv_sec)*1000;
	diff_msec += (tv_stop.tv_usec - tv_start.tv_usec)/1000;

	printf("%lu transfers (total %lu bytes) in %u miliseconds => %lu bytes/sec\n",
		num_xfer, num_bytes, diff_msec, (num_bytes*1000)/diff_msec);

    return num_bytes;
}


JNIEXPORT jint JNICALL
Java_com_qcymall_recorder_usbaudio_UsbAudio_measure(JNIEnv* env, jobject foo) {
    return measure();
}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved)
{
    LOGD("libusbaudio: loaded");
    java_vm = vm;

    return JNI_VERSION_1_6;
}


JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* reserved)
{
    JNIEnv * env;
    void * void_env;
    java_vm->GetEnv(&void_env, JNI_VERSION_1_6);
    env = void_env;

    (*env)->DeleteGlobalRef(env, com_qcymall_recorder_usbaudio_AudioPlayback);

    LOGD("libusbaudio: unloaded");
}

JNIEXPORT jint JNICALL
Java_com_qcymall_recorder_usbaudio_UsbAudio_setup(JNIEnv* env, jobject foo)
{
	int rc;
    uint8_t data[0x03];
    uint8_t data2[23];
    int *configCount;
	rc = libusb_init(NULL);
	if (rc < 0) {
		LOGD("Error initializing libusb: %d %s\n", rc, libusb_error_name(rc));
        return -1000 + rc;
	}

    // discover devices
    libusb_device **list;
    libusb_device *found = NULL;
    ssize_t cnt = libusb_get_device_list(NULL, &list);
    ssize_t i = 0;
    int err = 0;
    if (cnt >= 0){
        for (i = 0; i < cnt; i++) {
            libusb_device *device = list[i];


        }
        libusb_free_device_list(list, 1);
    }


    /* This device is the TI PCM2900C Audio CODEC default VID/PID. */
	devh = libusb_open_device_with_vid_pid(NULL, 0x0c76, 0x161F);
	if (!devh) {
		LOGD("Error finding USB device\n");
        libusb_exit(NULL);
        return -1100;
	}

    rc = libusb_kernel_driver_active(devh, IFACE_NUM);
    if (rc == 1) {
        rc = libusb_detach_kernel_driver(devh, IFACE_NUM);
        if (rc < 0) {
            LOGD("Could not detach kernel driver: %s\n",
                    libusb_error_name(rc));
            libusb_close(devh);
            libusb_exit(NULL);
            return -2000 + rc;
        }
    }

	rc = libusb_claim_interface(devh, IFACE_NUM);
	if (rc < 0) {
		LOGD("Error claiming interface: %s\n", libusb_error_name(rc));
        libusb_close(devh);
        libusb_exit(NULL);
        return -3000+rc;
    }

	rc = libusb_set_interface_alt_setting(devh, IFACE_NUM, 1);
	if (rc < 0) {
		LOGD("Error setting alt setting: %s\n", libusb_error_name(rc));
        libusb_close(devh);
        libusb_exit(NULL);
        return -4000+rc;
	}

    rc = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_ENDPOINT,
                                  0x01, 0x0100, EP_ISO_IN,
                                  data, sizeof(data), 0);
    if (rc == sizeof(data))
    {
        LOGD("set mic config success:0x%x:0x%x:0x%x\n",
               data[0], data[1], data[2]);
    }
    else
    {
        LOGD("set mic config fail %d\n", rc);
        return -5000+rc;
    }


    // Get write callback handle
    jclass clazz = (*env)->FindClass(env, "com/qcymall/recorder/usbaudio/AudioPlayback");
    if (!clazz) {
        LOGD("Could not find au.id.jms.usbaudio.AudioPlayback");
        libusb_close(devh);
        libusb_exit(NULL);
        return -6100;
    }
    com_qcymall_recorder_usbaudio_AudioPlayback = (*env)->NewGlobalRef(env, clazz);

    com_qcymall_recorder_usbaudio_AudioPlayback_write = (*env)->GetStaticMethodID(env,
            com_qcymall_recorder_usbaudio_AudioPlayback, "write", "([BI)V");
    if (!com_qcymall_recorder_usbaudio_AudioPlayback_write) {
        LOGD("Could not find au.id.jms.usbaudio.AudioPlayback");
        (*env)->DeleteGlobalRef(env, com_qcymall_recorder_usbaudio_AudioPlayback);
        libusb_close(devh);
        libusb_exit(NULL);
        return -7100;
    }


    // Good to go
    do_exit = 0;
    LOGD("Starting capture");
	if ((rc = benchmark_in(EP_ISO_IN)) < 0) {
        LOGD("Capture failed to start: %d", rc);
        return -8000+rc;
    }

    return 0;
}


JNIEXPORT void JNICALL
Java_com_qcymall_recorder_usbaudio_UsbAudio_stop(JNIEnv* env, jobject foo) {
    do_exit = 1;
    measure();
}

JNIEXPORT bool JNICALL
Java_com_qcymall_recorder_usbaudio_UsbAudio_close(JNIEnv* env, jobject foo) {
//    fclose(fp);
    if (do_exit == 0) {
        return false;
    }
	libusb_release_interface(devh, IFACE_NUM);
	if (devh)
		libusb_close(devh);
	libusb_exit(NULL);
    return true;
}


JNIEXPORT void JNICALL
Java_com_qcymall_recorder_usbaudio_UsbAudio_loop(JNIEnv* env, jobject foo) {
	while (!do_exit) {
		int rc = libusb_handle_events(NULL);
		if (rc != LIBUSB_SUCCESS)
			break;
	}
}

JNIEXPORT jstring JNICALL
Java_com_qcymall_recorder_usbaudio_UsbAudio_hellow(JNIEnv *env, jobject instance) {

    return (*env)->NewStringUTF(env, "hellow");
}
