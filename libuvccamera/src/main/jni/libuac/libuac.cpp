//
// Created by forrestluo on 2019/2/27.
//

#include "libuac.h"
#include "../utilbase.h"
#include "../libusb/libusb/libusb.h"

namespace libuac {

int UACContext::init() {
    auto ret = libusb_init(&usbContext_);
    if(ret != 0) {
        LOGE("libusb_init failed, %d", ret);
        return ret;
    }

    running_ = true;
    usbThread_ = std::thread([=](libusb_context *ctx) {
        while(running_) {
            libusb_handle_events(ctx);
        }
    }, usbContext_);
    usbThread_.detach();

    return 0;
}

int UACContext::unInit() {
    running_ = false;

    return 0;
}

std::shared_ptr<UACDevice> UACContext::findDevice(const int vid, const int pid, const std::string sn, int fd) {
    std::shared_ptr<UACDevice> device;
    if(!usbContext_) {
        LOGE("uac context not init yet");
        return device;
    }

    for(auto it = devices_.begin(); it != devices_.end(); it++) {
        if(it->venderId_ == vid && it->productId == pid) {
            return it;
        }
    }
    
    libusb_device *usbDevice = libusb_find_device(usbContext_, vid, pid, sn.c_str(), fd);
    if(usbDevice){
        libusb_set_device_fd(usbDevice, fd);  // assign fd to libusb_device for non-rooted Android devices
        libusb_ref_device(usbDevice);

        device->usbDevice_ = usbDevice;
        device->venderId_ = vid;
        device->productId_ = pid;

        devices_.push_back(device);
    }

    return device;
}

std::vector<std::shared_ptr<UACDevice>> UACContext::getDevices() {
    return devices_;
}

int UACInterface::claim(libusb_device_handle *handle) {
    int r = libusb_kernel_driver_active(handle, intfIdx_);
    LOGD("libusb_kernel_driver_active, %d", r);  
    if(r == 1) {
        //find out if kernel driver is attached  
        LOGD("Kernel Driver Active");  
        if(libusb_detach_kernel_driver(handle, intfIdx_) == 0) //detach it  
            LOGD("Kernel Driver Detached!");  
    }  
    LOGD("kernel detach errno:%d", errno);

    r = libusb_claim_interface(handle, intfIdx_);            //claim interface 0 (the first) of device (mine had jsut 1)  
    if(r != 0) {  
        LOGD("Cannot Claim Interface, %d", r);  
        return r;  
    }  
    LOGD("claim_interface errno:%d", errno);

    return 0;
}

int UACDevice::open() {

    if(!usbDevice_) {
        LOGE("usb device not found");
        return LIBUSB_ERROR_NO_DEVICE;
    }

    if(isOpened_) {
        LOGD("already opened before");
        return 0;
    }

    // open usb device
    auto ret = libusb_open(usbDevice_, &usbDeviceHandle_);
    if(ret != 0) {
        LOGE("libusb_open failed, %d", ret);
        return ret;
    }
    libusb_ref_device(usbDevice_);
    isOpened_ = true;

    // get config descriptor
    ret = libusb_get_config_descriptor(usbDevice_, 0, &config_);
    if(ret != 0) {
        LOGE("libusb_get_config_descriptor failed, %d", ret);
        return ret;
    }

    // scan control interface
    scanControlInterface();

    // scan audio interface
    scanAudioInterface();

    // start to fetch audio data
    startStreaming();

    return 0;
}

void UACDevice::scanControlInterface() {

    // find control interface
    auto cnt = config_->bNumInterfaces;
    if(cnt > 0) {
        for(int interfaceIdx = 0; interfaceIdx < cnt; interfaceIdx++) {
            libusb_interface_descriptor *ifDescr = config_->interface[interfaceIdx]->altsetting[0];

            LOGD("inteface:[intfIdx:%d, altsetting:%d, epNums:%d, class:%d, subClass:%d, protocol:%d]", 
                        ifDescr->bInterfaceNumber, ifDescr->bAlternateSetting, ifDescr->bNumEndpoints, 
                        ifDescr->bInterfaceClass, ifDescr->bInterfaceSubClass, ifDescr->bInterfaceProtocol);
            if(ifDescr->bInterfaceClass == LIBUSB_CLASS_AUDIO && ifDescr->bInterfaceSubClass == 0x1) { // audio, control
                controlInterface_.reset(new UACInterface);
                controlInterface_->intfIdx_ = ifDescr->bInterfaceNumber;
                if(ifDescr->bNumEndpoints && ifDescr->endpoint) {
                    controlInterface_->endpointAddr_ = ifDescr->endpoint->bEndpointAddress;
                }

                controlInterface_->claim(usbDeviceHandle_);

                break;
            }
        }
    }
}

void UACDevice::scanAudioInterface() {

    // find audio interface
    auto cnt = config_->bNumInterfaces;
    if(cnt > 0) {
        for(int interfaceIdx = 0; interfaceIdx < cnt; interfaceIdx++) {
            libusb_interface_descriptor *ifDescr = config_->interface[interfaceIdx]->altsetting[0];
            if(ifDescr->bInterfaceClass == LIBUSB_CLASS_AUDIO && ifDescr->bInterfaceSubClass == 0x2) { // audio, stream
                auto epCnt = ifDescr->bNumEndpoints;
                if(epCnt && ifDescr->endpoint) {
                    std::shared_ptr<UACInterface> interface = std::make_shared<UACInterface>();
                    interface->intfIdx_ = ifDescr->bInterfaceNumber;
                    interface->endpointAddr_ = ifDescr->endpoint->bEndpointAddress;
                    interface->claim(usbDeviceHandle_);

                    streamInterfaces_->push_back(interface);
                }
            }
        }
    }
}


int UACDevice::startStreaming() {
    // transfer

    if(isOpened_) {
        // retransfer
    }

    return 0;
}

int UACDevice::stopStreaming() {

    return 0;
}

int UACDevice::close() {
    isOpened_ = false;
    isRecording_ = false;
    libusb_unref_device(usbDevice_);
    libusb_close(usbDeviceHandle_);

    return 0;
}

int UACDevice::startRecord() {
    if(!isOpened_) {
        LOGI("please open device first");
        return -1;
    }

    isRecording_ = true;

    return 0;
}

int UACDevice::stopRecord() {
    isRecording_ = false;

    return 0;
}


}