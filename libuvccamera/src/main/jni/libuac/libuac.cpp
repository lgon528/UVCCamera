//
// Created by forrestluo on 2019/2/27.
//

#include "libuac.h"
#include "../utilbase.h"
#include "../libusb/libusb/libusb.h"
#include <string>
#include <stdlib.h>

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

std::shared_ptr<UACDevice> UACContext::findDevice(const int vid, const int pid, const std::string sn, int fd, int busnum, int devaddr) {
    std::shared_ptr<UACDevice> device;
    if(!usbContext_) {
        LOGE("uac context not init yet");
        return device;
    }

    LOGE("we're here");
    for(auto it = devices_.begin(); it != devices_.end(); it++) {
        if((*it)->venderId_ == vid && (*it)->productId_ == pid) {
            return *it;
        }
    }
    
    LOGE("we're here");
    libusb_device *usbDevice = libusb_get_device_with_fd(usbContext_, vid, pid, sn.c_str(), fd, busnum, devaddr);
    if(usbDevice){
        libusb_set_device_fd(usbDevice, fd);  // assign fd to libusb_device for non-rooted Android devices
        libusb_ref_device(usbDevice);

        device.reset(new UACDevice());
        device->usbDevice_ = usbDevice;
        device->venderId_ = vid;
        device->productId_ = pid;

        devices_.push_back(device);
    }
    LOGE("we're here");

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
    LOGD("kernel detach errno:%d, %s", errno, strerror(errno));

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

    // ret = libusb_set_configuration(usbDeviceHandle_, config_->bConfigurationValue);
    // if(ret != 0) {
    //     LOGE("libusb_set_configuration failed, ret %d(%s)", ret, libusb_error_name(ret));
    //     return ret;
    // }

    // scan control interface
    _scanControlInterface();

    // scan audio interface
    _scanAudioInterface();

    // start to fetch audio data
    _startStreaming();

    return 0;
}

void UACDevice::_scanControlInterface() {

    // find control interface
    auto cnt = config_->bNumInterfaces;
    if(cnt > 0) {
        for(int interfaceIdx = 0; interfaceIdx < cnt; interfaceIdx++) {
            auto num_altsetting = config_->interface[interfaceIdx].num_altsetting;

            LOGE("we're here, num_altsetting: %d", cnt);
            for(int settingIdx = 0; settingIdx < num_altsetting; settingIdx++) {

                const libusb_interface_descriptor *ifDescr = &config_->interface[interfaceIdx].altsetting[settingIdx];

                LOGE("we're here, setting: len %d, type %d, ifNum %d, settingN %d, epNum %d, \
                        class %d, subclass %d, proto %d, strIf %d, extLen %d", ifDescr->bLength,
                        ifDescr->bDescriptorType, ifDescr->bInterfaceNumber, ifDescr->bAlternateSetting,
                        ifDescr->bNumEndpoints, ifDescr->bInterfaceClass, ifDescr->bInterfaceSubClass,
                        ifDescr->bInterfaceProtocol, ifDescr->iInterface, ifDescr->extra_length);

                if(ifDescr->bInterfaceClass == LIBUSB_CLASS_AUDIO && ifDescr->bInterfaceSubClass == 0x1) { // audio, control
                    ctrlIf_.reset(new UACInterface);
                    ctrlIf_->altsettingIdx_ = ifDescr->b
                    ctrlIf_->intfIdx_ = ifDescr->bInterfaceNumber;
                    if(ifDescr->endpoint) {
                        ctrlIf_->endpointAddr_ = ifDescr->endpoint->bEndpointAddress;
                    }

                    ctrlIf_->claim(usbDeviceHandle_);

                    break;
                }
            }
        }
    }
}

void UACDevice::_scanAudioInterface() {

    // find audio interface
    auto cnt = config_->bNumInterfaces;
    LOGE("we're here, cnt: %d", cnt);
    if(cnt > 0) {
        for(int interfaceIdx = 0; interfaceIdx < cnt; interfaceIdx++) {
            auto num_altsetting = config_->interface[interfaceIdx].num_altsetting;

            LOGE("we're here, num_altsetting: %d", cnt);
            for(int settingIdx = 0; settingIdx < num_altsetting; settingIdx++) {

                const libusb_interface_descriptor *ifDescr = &config_->interface[interfaceIdx].altsetting[settingIdx];

                LOGE("we're here, setting: len %d, type %d, ifNum %d, settingN %d, epNum %d, \
                        class %d, subclass %d, proto %d, strIf %d, extLen %d, ep %p", ifDescr->bLength,
                        ifDescr->bDescriptorType, ifDescr->bInterfaceNumber, ifDescr->bAlternateSetting,
                        ifDescr->bNumEndpoints, ifDescr->bInterfaceClass, ifDescr->bInterfaceSubClass,
                        ifDescr->bInterfaceProtocol, ifDescr->iInterface, ifDescr->extra_length, ifDescr->endpoint);

                if(ifDescr->bInterfaceClass == LIBUSB_CLASS_AUDIO && ifDescr->bInterfaceSubClass == 0x2) { // audio, stream
                    // auto epCnt = ifDescr->bNumEndpoints;
                    // if(epCnt && ifDescr->endpoint) {
                    if(ifDescr->endpoint) {
                        std::shared_ptr<UACInterface> interface = std::make_shared<UACInterface>();
                        interface->intfIdx_ = ifDescr->bInterfaceNumber;
                        interface->endpointAddr_ = ifDescr->endpoint->bEndpointAddress;
                        int maxSize = libusb_get_max_iso_packet_size(usbDevice_, interface->endpointAddr_);
                        interface->maxPackageSize_ = maxSize > 0 ? maxSize : ifDescr->endpoint->wMaxPacketSize;

                        interface->claim(usbDeviceHandle_);

                        streamIfs_.push_back(interface);

                        LOGD("we're here, ep maxPackageSize: %d, %d", interface->maxPackageSize_, ifDescr->endpoint->wMaxPacketSize);
                    }
                }
            }
        }
    } else {
    LOGE("we're here, no inteface");
    }
}


int UACDevice::_startStreaming() {
    // transfer
    if(isOpened_) {
        LOGE("we're here");
        _transfer();
    }

    return 0;
}

static void _process_payload_iso(libusb_transfer *transfer) {
    if (transfer->type != LIBUSB_TRANSFER_TYPE_ISOCHRONOUS)
    {
        LOGE("not isoc packet\n");
        return;
    }

    UACDevice *device = (UACDevice*)transfer->user_data;
    if(!device) {
        LOGE("device not found");
        return;
    }

    // todo make a const
    int packets_per_transfer = 16;
    size_t maxPackageSize = device->streamIfs_[0]->maxPackageSize_;

    /* per packet */
    unsigned char *pktbuf;
    struct libusb_iso_packet_descriptor *pkt;
    std::string recv;
    for (int packet_id = 0; packet_id < transfer->num_iso_packets; ++packet_id) {

        pkt = &transfer->iso_packet_desc[packet_id];

        if (pkt->status != LIBUSB_TRANSFER_COMPLETED) {
            LOGE("bad packet:status=%d,actual_length=%d", pkt->status, pkt->actual_length);
            continue;
        }


        pktbuf = libusb_get_iso_packet_buffer_simple(transfer, packet_id);
        if(pktbuf == NULL)
        {
            LOGE("receive pktbuf null\n");
        }

        recv.append((char*)pktbuf, pkt->length);

    }   // for

    if(device->isRecording_ ) {
        // todo save recv data to file
    }

    if(device->frameCallback_) {
        device->frameCallback_->onStreaming(recv);
    }


    if (recv.size() > maxPackageSize * transfer->num_iso_packets) {
        LOGE("Error: incoming transfer had more data than we thought.\n");
        return;
    }

}

static void _stream_callback(libusb_transfer *transfer)
{
    UACDevice *device = (UACDevice*)transfer->user_data;
    if(!device) {
        LOGE("device not found");
        return;
    }

    //printf("do callback\n");
    switch (transfer->status)
    {
        case LIBUSB_TRANSFER_COMPLETED:
            if (transfer->num_iso_packets) {
                /* This is an isochronous mode transfer, so each packet has a payload transfer */
                _process_payload_iso(transfer);
            }
            break;
        case LIBUSB_TRANSFER_NO_DEVICE:
            device->isOpened_ = 0;  // this needs for unexpected disconnect of cable otherwise hangup
            // pass through to following lines
        case LIBUSB_TRANSFER_CANCELLED:
        case LIBUSB_TRANSFER_ERROR:
            break;
        case LIBUSB_TRANSFER_TIMED_OUT:
        case LIBUSB_TRANSFER_STALL:
        case LIBUSB_TRANSFER_OVERFLOW:
            break;
    }

    if(device->isOpened_)
    {// retransfer
        if (libusb_submit_transfer(transfer) < 0) {
            LOGE("libusb_submit_transfer err.\n");
        }
    }

}

void UACDevice::_transfer() {
        LOGE("we're here");
    // todo make a const
    const int LIBUAC_NUM_TRANSFER_BUFS = 10;
    int packets_per_transfer = 16;

    LOGE("we're here, ifs size: %d", streamIfs_.size());
    if(streamIfs_.size() <= 0){
        LOGE("no valid interfaces");
        return;
    }
    uint8_t endpointAddress = streamIfs_[0]->endpointAddr_;
    size_t endpoint_bytes_per_packet = streamIfs_[0]->maxPackageSize_;
    size_t total_transfer_size = endpoint_bytes_per_packet * packets_per_transfer;
    libusb_transfer* transfers[LIBUAC_NUM_TRANSFER_BUFS];
    uint8_t* transfer_bufs[LIBUAC_NUM_TRANSFER_BUFS];

    // todo select altenatesetting
    //libusb_set_interface_alt_setting(usbDeviceHandle_, streamIfs_[0]->intfIdx_,

        LOGE("we're here");

    LOGD("Set up the transfers\n");

    LOGD("before fill EndpointAddress:%d, per_packet:%d, packets:%d, total_transfer_size:%d\n",
                       endpointAddress, endpoint_bytes_per_packet, packets_per_transfer, total_transfer_size);
    for (int transfer_id = 0; transfer_id < LIBUAC_NUM_TRANSFER_BUFS; ++transfer_id)
    {
        LOGD("fill transfer_id:%d\n", transfer_id);
        libusb_transfer *transfer = libusb_alloc_transfer(packets_per_transfer);
        transfers[transfer_id] = transfer;
        transfer_bufs[transfer_id] = (unsigned char *)malloc(total_transfer_size);
        if(transfer_bufs[transfer_id] == nullptr) {
            continue;
        }
        memset(transfer_bufs[transfer_id], 0, total_transfer_size);
        libusb_fill_iso_transfer(transfer, usbDeviceHandle_,
            endpointAddress,
            transfer_bufs[transfer_id], total_transfer_size,
            packets_per_transfer, _stream_callback,
            (void *)this, 1000);

        libusb_set_iso_packet_lengths(transfer, endpoint_bytes_per_packet);

        LOGD("fill transfer_id:%d finished\n", transfer_id);
        
    }

    LOGD("before submit errno:%d\n", errno);

    for (int transfer_id = 0; transfer_id < LIBUAC_NUM_TRANSFER_BUFS; transfer_id++) {
        LOGD("submit transfer_id:%d\n", transfer_id);
        auto ret = libusb_submit_transfer(transfers[transfer_id]);
        if (ret != 0) {
            LOGE("libusb_submit_transfer failed: %d(%s), errno:%d\n", ret, libusb_error_name(ret), errno);
            break;
        }
        LOGD("submit transfer_id:%d finish\n", transfer_id);
    }

    LOGD("after submit errno:%d\n", errno);

}


int UACDevice::_stopStreaming() {

    return 0;
}

int UACDevice::close() {
    isOpened_ = false;
    isRecording_ = false;
    libusb_unref_device(usbDevice_);
    libusb_close(usbDeviceHandle_);

    return 0;
}

int UACDevice::startRecord(std::string path) {
    if(!isOpened_) {
        LOGE("please open device first");
        return -1;
    }
    
    if(isRecording_) {
        LOGE("already in recording state, please call stopRecord first");
        return -1;
    }

    LOGI("start recording to file %s", path.c_str());

    isRecording_ = true;
    recordFilePath_ = path;

    return 0;
}

int UACDevice::stopRecord() {
    isRecording_ = false;

    return 0;
}

void UACDevice::setAudioStreamCallback(std::shared_ptr<IAudioStreamCallback> cb) {
    frameCallback_ = cb;
}


}