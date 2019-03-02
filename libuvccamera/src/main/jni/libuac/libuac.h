//
// Created by forrestluo on 2019/2/27.
//

#ifndef __LIBUAC_H_H
#define __LIBUAC_H_H

#include "libusb/libusb/libusb.h"
#include <thread>
#include <vector>

namespace libuac {

class UACInterface {
public:
    uint8_t intfIdx_;
    uint8_t endpointAddr_;

public:
    int claim(libusb_device_handle *devHandle);
};

class UACDevice {

std::function<void(std::string data)> callback;

public:
    int open();
    int startRecord();
    int stopRecord();
    int close();
    void setCallback();

private:
    void scanControlInterface();
    void scanAudioInterface();
    int startStreaming();
    int stopStreaming();

public:
    int vendorId_;
    int productId_;

    bool isOpened_;
    bool isRecording_;

    libusb_device *usbDevice_;
    libusb_device_handle *usbDeviceHandle_;
    libusb_config_descriptor *config_;
    std::string deviceName_;
    std::shared_ptr<UACInterface> controlInterface_;
    std::vector<std::shared_ptr<UACInterface>> streamInterfaces_;
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
    std::shared_ptr<UACDevice> findDevice(const int vid, const int pid, const std::string sn, int fd);
    std::vector<std::shared_ptr<UACDevice>> getDevices();

private:
    libusb_context *usbContext_;
    std::vector<std::shared_ptr<UACDevice>> devices_;
    std::thread usbThread_;

    bool running_ = false;
};


}

#endif //__LIBUAC_H_H
