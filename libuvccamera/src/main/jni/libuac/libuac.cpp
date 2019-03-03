//
// Created by forrestluo on 2019/2/27.
//

#include "libuac.h"
#include "../utilbase.h"
#include "../libusb/libusb/libusb.h"
#include <string>

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
        if((*it)->venderId_ == vid && (*it)->productId_ == pid) {
            return *it;
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
            const libusb_interface_descriptor *ifDescr = &config_->interface[interfaceIdx].altsetting[0];

            LOGD("inteface:[intfIdx:%d, altsetting:%d, epNums:%d, class:%d, subClass:%d, protocol:%d]", 
                        ifDescr->bInterfaceNumber, ifDescr->bAlternateSetting, ifDescr->bNumEndpoints, 
                        ifDescr->bInterfaceClass, ifDescr->bInterfaceSubClass, ifDescr->bInterfaceProtocol);
            if(ifDescr->bInterfaceClass == LIBUSB_CLASS_AUDIO && ifDescr->bInterfaceSubClass == 0x1) { // audio, control
                ctrlIf_.reset(new UACInterface);
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

void UACDevice::_scanAudioInterface() {

    // find audio interface
    auto cnt = config_->bNumInterfaces;
    if(cnt > 0) {
        for(int interfaceIdx = 0; interfaceIdx < cnt; interfaceIdx++) {
            const libusb_interface_descriptor *ifDescr = &config_->interface[interfaceIdx].altsetting[0];
            if(ifDescr->bInterfaceClass == LIBUSB_CLASS_AUDIO && ifDescr->bInterfaceSubClass == 0x2) { // audio, stream
                // auto epCnt = ifDescr->bNumEndpoints;
                // if(epCnt && ifDescr->endpoint) {
                if(ifDescr->endpoint) {
                    std::shared_ptr<UACInterface> interface = std::make_shared<UACInterface>();
                    interface->intfIdx_ = ifDescr->bInterfaceNumber;
                    interface->endpointAddr_ = ifDescr->endpoint->bEndpointAddress;
                    interface->maxPackageSize_ = libusb_get_max_iso_packet_size(usbDevice_, interface->endpointAddr_);
                    interface->claim(usbDeviceHandle_);

                    streamIfs_.push_back(interface);
                }
            }
        }
    }
}


int UACDevice::_startStreaming() {
    // transfer
    if(isOpened_) {
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
    // todo make a const
    const int LIBUAC_NUM_TRANSFER_BUFS = 10;
    int packets_per_transfer = 16;

    uint8_t endpointAddress = streamIfs_[0]->endpointAddr_;
    size_t endpoint_bytes_per_packet = streamIfs_[0]->maxPackageSize_;
    size_t total_transfer_size = endpoint_bytes_per_packet * packets_per_transfer;
    libusb_transfer* transfers[LIBUAC_NUM_TRANSFER_BUFS];
    uint8_t* transfer_bufs[LIBUAC_NUM_TRANSFER_BUFS];


    LOGD("Set up the transfers\n");

    LOGD("before fill EndpointAddress:%d, per_packet:%d, packets:%d, total_transfer_size:%d\n",
                       endpointAddress, endpoint_bytes_per_packet, packets_per_transfer, total_transfer_size);
    for (int transfer_id = 0; transfer_id < LIBUAC_NUM_TRANSFER_BUFS; ++transfer_id)
    {
        LOGD("fill transfer_id:%d\n", transfer_id);
        libusb_transfer *transfer = libusb_alloc_transfer(packets_per_transfer);
        transfers[transfer_id] = transfer;
        transfer_bufs[transfer_id] = (unsigned char *)malloc(total_transfer_size);
        memset(transfer_bufs[transfer_id], 0, total_transfer_size);
        libusb_fill_iso_transfer(transfer, usbDeviceHandle_,
            endpointAddress,
            transfer_bufs[transfer_id], total_transfer_size,
            packets_per_transfer, _stream_callback,
            (void *)this, 1000);

        libusb_set_iso_packet_lengths(transfer, endpoint_bytes_per_packet);
        
    }

    LOGD("before submit errno:%d\n", errno);
    for (int transfer_id = 0; transfer_id < LIBUAC_NUM_TRANSFER_BUFS; transfer_id++) {
        LOGD("submit transfer_id:%d\n", transfer_id);
        auto ret = libusb_submit_transfer(transfers[transfer_id]);
        if (ret != 0) {
            LOGE("libusb_submit_transfer failed: %d, errno:%d\n", ret, errno);
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