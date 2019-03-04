//
// Created by forrestluo on 2019/2/27.
//

#ifndef __LIBUAC_H_H
#define __LIBUAC_H_H

#include "libusb/libusb/libusb.h"
#include <thread>
#include <vector>

namespace libuac {

typedef std::string Bytes;

class UACInterface {
public:
    uint8_t altsettingIdx_;
    uint8_t intfIdx_;
    uint8_t endpointAddr_;
    size_t maxPackageSize_;

public:
    int claim(libusb_device_handle *devHandle);
};

class IAudioStreamCallback {
public:
    virtual void onStreaming(Bytes data) = 0;
};

class UACDevice {

public:
    int open();
    int startRecord(std::string path);
    int stopRecord();
    int close();
    void setAudioStreamCallback(std::shared_ptr<IAudioStreamCallback> cb);

private:
    void _scanControlInterface();
    void _scanAudioInterface();
    int _startStreaming();
    int _stopStreaming();
    void _transfer();

public:
    int venderId_;
    int productId_;

    bool isOpened_;
    bool isRecording_;
    std::string recordFilePath_;
    std::shared_ptr<IAudioStreamCallback> frameCallback_;

    libusb_device *usbDevice_;
    libusb_device_handle *usbDeviceHandle_;
    libusb_config_descriptor *config_;
    std::string deviceName_;
    std::shared_ptr<UACInterface> ctrlIf_;
    std::vector<std::shared_ptr<UACInterface>> streamIfs_;
};


class UACContext {
private:
    UACContext() {}
	~UACContext() {}

public:
    static UACContext& getInstance() {
        static UACContext instance;
        return instance;
    }

    int init();
    int unInit();
    std::shared_ptr<UACDevice> findDevice(const int vid, const int pid, 
                        const std::string sn, int fd, int busnum, int devaddr);
    std::vector<std::shared_ptr<UACDevice>> getDevices();

private:
    libusb_context *usbContext_;
    std::vector<std::shared_ptr<UACDevice>> devices_;
    std::thread usbThread_;

    bool running_ = false;
};


}

#endif //__LIBUAC_H_H
