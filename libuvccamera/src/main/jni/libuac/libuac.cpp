//
// Created by forrestluo on 2019/2/27.
//

#include "libuac.h"
#include "utils.h"
#include "utilbase.h"
#include "libusb/libusb/libusb.h"

#include <errno.h>    //for error handling
#include <string>
#include <sstream>
#include <stdlib.h>
#include <set>

namespace libuac {

int UACContext::init(std::string usbfs) {
    if(!usbContext_) {
        LOGD("init, usbfs: %s", usbfs.c_str());
        auto ret = libusb_init2(&usbContext_, usbfs.c_str());
        if(ret != 0) {
            LOGE("libusb_init failed, %d", ret);
            return ret;
        }
    }

    libusb_set_debug(usbContext_, LIBUSB_LOG_LEVEL_DEBUG);

    return 0;
}

void UACContext::dumpDevices() {
    ENTER();
    if(!usbContext_) {
        LOGE("uac context not inited");
        return;
    }

    libusb_device **list;
    auto size = libusb_get_device_list(usbContext_, &list);
    LOGD("devices size: %d", size);

	libusb_device *dev;
	int i = 0, j = 0;
	uint8_t path[8]; 

    for(int i = 0; i < size; i++) {
        auto dev = list[i];

		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			fprintf(stderr, "failed to get device descriptor");
			return;
		}

		LOGE("%04x:%04x (bus %d, device %d)",
			desc.idVendor, desc.idProduct,
			libusb_get_bus_number(dev), libusb_get_device_address(dev));

        UACDevice device;
        device.usbDevice_ = dev;

        device.scanControlInterface();
        device.scanStreamInterface();
	}

    libusb_free_device_list(list, 1);

    EXIT();
}

bool UACContext::isAnyDeviceOpened() {
    for(auto it = devices_.begin(); it != devices_.end(); it++) {
        if(it->second->isOpened_) {
            return true;
        }
    }

    return false;
}

void UACContext::startThread() {
    LOGI("start thread called, running_ %d", running_);
    if(running_) {
        return;
    }

    running_ = true;
    usbThread_ = std::thread([=](libusb_context *ctx) {

        LOGI("audio thread enter");
        while(running_ && isAnyDeviceOpened()) {
            libusb_handle_events(ctx);
        }
        running_ = false;
        LOGI("audio thread exit");
    }, usbContext_);
    usbThread_.detach();
}

int UACContext::destroy() {
    running_ = false;

    for(auto it = devices_.begin(); it != devices_.end(); it++) {
        if(it->second->usbDevice_) {
            libusb_unref_device(it->second->usbDevice_);
        }
    }

    libusb_exit(usbContext_);

    return 0;
}

std::shared_ptr<UACDevice> UACContext::findDevice(const int vid, const int pid, int fd, int busnum, int devaddr, const std::string sn, std::string usbfs) {
    std::shared_ptr<UACDevice> device;

    if(init(usbfs)) {
        LOGE("uac context not init yet");
        return device;
    }

    std::ostringstream oss;
    oss << vid << "_" << pid << "_" << sn;
    std::string key = oss.str();

    LOGE("we're here, key %s, vid %d, pid %d, sn %s, fd %d, bnum %d, daddr %d", key.c_str(), vid, pid, sn.c_str(), fd, busnum, devaddr);

    libusb_device *usbDevice = libusb_get_device_with_fd(usbContext_, vid, pid, sn.c_str(), fd, busnum, devaddr);
    LOGE("we're here, usbDevice %p", usbDevice);
    if(usbDevice){
        libusb_set_device_fd(usbDevice, fd);  // assign fd to libusb_device for non-rooted Android devices
        //libusb_ref_device(usbDevice);

        device.reset(new UACDevice());
        device->usbDevice_ = usbDevice;
        device->venderId_ = vid;
        device->productId_ = pid;

       // get config descriptor
        device->getConfig();

        // scan control interface
        device->scanControlInterface();

        if(!device->ctrlIf_) {
            LOGE("no audio control interface found");
            device.reset();
            return device;
        }

        // scan audio interface
        device->scanStreamInterface();

        if(device->streamIfs_.empty()) {
            LOGE("no audio stream interface found");
            device.reset();
            return device;
        }

        // select endpoint with max packet size by default
        device->selectedIf_ = device->streamIfs_.begin()->second;
        for(auto it = device->streamIfs_.begin(); it != device->streamIfs_.end(); it++) {
            if(it->second->epDescr_->wMaxPacketSize > device->selectedIf_->epDescr_->wMaxPacketSize) {
                device->selectedIf_ = it->second;
            }
        }

        devices_[key] = device;
    }

    return device;
}

std::map<std::string, std::shared_ptr<UACDevice>> UACContext::getDevices() {
    return devices_;
}

int UACInterface::claim(libusb_device_handle *devHandle) {

                    LOGE("we're here, devhandle %p, ifDescr %p", devHandle, ifDescr_);

    LOGD("before kernel active errno:%d, %s", errno, strerror(errno));
    int r = libusb_kernel_driver_active(devHandle, ifDescr_->bInterfaceNumber);
    LOGD("libusb_kernel_driver_active, %d, errno:%d, %s", r, errno, strerror(errno));
    if(r == 1) {
        // find out if kernel driver is attached
        LOGD("Kernel Driver Active");
        // detach it
        r = libusb_detach_kernel_driver(devHandle, ifDescr_->bInterfaceNumber);
        if(r == 0)  {
            LOGD("Kernel Driver Detached!");
        } else {
            LOGE("Kernel Driver Detached! errno:%d, %s", errno, strerror(errno));
        }
    }  
    LOGD("before claim interface %d, errno:%d, %s", ifDescr_->bInterfaceNumber, errno, strerror(errno));

    //claim interface
    r = libusb_claim_interface(devHandle, ifDescr_->bInterfaceNumber);
    if(r != 0) {  
        LOGD("Cannot Claim Interface %d, ret %d, errno:%d, %s", ifDescr_->bInterfaceNumber, r, errno, strerror(errno));
        return r;  
    }  
    LOGD("after claim_interface %d, errno:%d, %s", ifDescr_->bInterfaceNumber, errno, strerror(errno));

    return 0;
}

int UACInterface::release(libusb_device_handle *devHandle) {
    LOGD("releasing interface %d", ifDescr_->bInterfaceNumber);

    int idx = ifDescr_->bInterfaceNumber;
    int ret = libusb_release_interface(devHandle, idx);
    if(ret != 0) {
        LOGE("cannot release interface %d, ret %d(%s)", idx, ret, libusb_error_name(ret));
        return ret;
    }

    return ret;

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

    ret = libusb_reset_device(usbDeviceHandle_);
    if(ret != 0) {
        LOGE("libusb_reset_device failed, ret %d(%s)", ret, libusb_error_name(ret));
    }

    // claim interface
    ctrlIf_->claim(usbDeviceHandle_);
    ret = libusb_set_interface_alt_setting(usbDeviceHandle_, ctrlIf_->ifDescr_->bInterfaceNumber, ctrlIf_->ifDescr_->bAlternateSetting);
    LOGE("we're here, interface %d, altsetting %d", ctrlIf_->ifDescr_->bInterfaceNumber, ctrlIf_->ifDescr_->bAlternateSetting);
    if(ret < 0) {
        LOGE("libusb_set_interface_alt_setting failed, interface %d, altsetting %d, ret %d(%s)",
                        ctrlIf_->ifDescr_->bInterfaceNumber, ctrlIf_->ifDescr_->bAlternateSetting,
                        ret, libusb_error_name(ret));
        return ret;
    }

    selectedIf_->claim(usbDeviceHandle_);
    ret = libusb_set_interface_alt_setting(usbDeviceHandle_, selectedIf_->ifDescr_->bInterfaceNumber, selectedIf_->ifDescr_->bAlternateSetting);
    LOGE("we're here, interface %d, altsetting %d", selectedIf_->ifDescr_->bInterfaceNumber, selectedIf_->ifDescr_->bAlternateSetting);
    if(ret < 0) {
        LOGE("libusb_set_interface_alt_setting failed, interface %d, altsetting %d, ret %d(%s)",
                        selectedIf_->ifDescr_->bInterfaceNumber, selectedIf_->ifDescr_->bAlternateSetting,
                        ret, libusb_error_name(ret));
        return ret;

    }

    isOpened_ = true;

    UACContext::getInstance().startThread();

    // start to fetch audio data
    _startStreaming();

    return 0;
}

int UACDevice::close() {
    isOpened_ = false;
    isRecording_ = false;

    ctrlIf_->release(usbDeviceHandle_);
    selectedIf_->release(usbDeviceHandle_);

    libusb_close(usbDeviceHandle_);
    libusb_unref_device(usbDevice_);

    return 0;
}

int UACDevice::getConfig() {

    auto ret = libusb_get_config_descriptor(usbDevice_, 0, &config_);
    if(ret != 0) {
        LOGE("libusb_get_config_descriptor failed, %d", ret);
        return ret;
    }

    return 0;
}

int UACDevice::_parseAudioControlSpecific(std::shared_ptr<UACInterface> interface, const unsigned char *extra, const int len) {
    if(!interface || !interface->isCtrl_) return -1;

    if(len < 3) {
        LOGE("invalid interface extra");
        return -1;
    }

    AudioControlSpecific acSpecific;
    int remainBytes = len;
    while(remainBytes > 0) {
        int pos = 0;
        uint8_t bLength = extra[pos++];
        uint8_t bDescriptorType = extra[pos++];
        uint8_t bDescriptorSubtype = extra[pos++];

        switch((ACSpecType)bDescriptorSubtype) {
        case ACSpecType::FEATURE_UNIT: {
                FeatureUnitDescriptor featureUnitDescr;
                featureUnitDescr.bUnitID = extra[pos++];
                featureUnitDescr.bSourceID = extra[pos++];
                featureUnitDescr.bControlSize = extra[pos++];

                if(featureUnitDescr.bControlSize > 0) {
                    uint8_t hv = extra[pos + featureUnitDescr.bControlSize -1];
                    uint8_t lv = extra[pos + featureUnitDescr.bControlSize -2];
                    featureUnitDescr.wBmaControls = hv << 8 | lv;
                }

                acSpecific.featureUnitDescr_ = featureUnitDescr;
            }
            break;
        default:
            LOGI("not supported ASSpecType: %d", bDescriptorSubtype);
            break;
        }

        remainBytes -= bLength;
        extra = extra + bLength;
    }

    interface->audioSpec_.acSpecific_ = acSpecific;

    return 0;
}


void UACDevice::scanControlInterface() {

    // find control interface
    auto cnt = config_->bNumInterfaces;
    if(cnt > 0) {
        for(int interfaceIdx = 0; interfaceIdx < cnt; interfaceIdx++) {
            auto num_altsetting = config_->interface[interfaceIdx].num_altsetting;

            LOGE("we're here, num_altsetting: %d", num_altsetting);
            for(int settingIdx = 0; settingIdx < num_altsetting; settingIdx++) {

                const libusb_interface_descriptor *ifDescr = &config_->interface[interfaceIdx].altsetting[settingIdx];

                LOGE("we're here, setting: len %d, type %d, ifNum %d, settingN %d, epNum %d,class %d, subclass %d, proto %d, strIf %d, extLen %d",
                        ifDescr->bLength, ifDescr->bDescriptorType, ifDescr->bInterfaceNumber, ifDescr->bAlternateSetting,
                        ifDescr->bNumEndpoints, ifDescr->bInterfaceClass, ifDescr->bInterfaceSubClass,
                        ifDescr->bInterfaceProtocol, ifDescr->iInterface, ifDescr->extra_length);

                if(ifDescr->bInterfaceClass == LIBUSB_CLASS_AUDIO && ifDescr->bInterfaceSubClass == 0x1) { // audio, control
                    LOGE("we're here, devhandle %p", usbDeviceHandle_);
                    ctrlIf_.reset(new UACInterface);
                    ctrlIf_->ifDescr_ = ifDescr;
                    ctrlIf_->epDescr_ = ifDescr->endpoint;
                    ctrlIf_->isCtrl_ = true;

                    _parseAudioControlSpecific(ctrlIf_, ifDescr->extra, ifDescr->extra_length);

                    break;
                }
            }
        }
    }
}

int UACDevice::_parseAudioStreamSpecific(std::shared_ptr<UACInterface> interface, const unsigned char *extra, const int len){
    if(!interface || interface->isCtrl_) return -1;
    
    if(len < 3) {
        LOGE("invalid interface extra");
        return -1;
    }

    AudioStreamSpecific asSpecific;

    int remainBytes = len;
    while(remainBytes > 0) {
        int pos = 0;
        uint8_t bLength = extra[pos++];
        uint8_t bDescriptorType = extra[pos++];
        uint8_t bDescriptorSubtype = extra[pos++];

        switch((ASSpecType)bDescriptorSubtype) {
        case ASSpecType::AS_GENERAL: {
                ASGeneralInterfaceDescriptor generalDescr;
                generalDescr.bTerminalLink = extra[pos++];
                generalDescr.bDelay = extra[pos++];
                uint8_t lv = extra[pos++];
                uint8_t hv = extra[pos++];
                generalDescr.wFormatTag = hv << 8 | lv;

                asSpecific.asGeneralIfDescr_ = generalDescr;
            }
            break;
        case ASSpecType::FORMAT_TYPE: {
                uint8_t bFormatType = extra[pos++];
                if((FormatType)bFormatType == FormatType::FORMAT_TYPE_I){
                    FormatTypeIDescriptor formatTypeI;
                    formatTypeI.bNrChannels = extra[pos++];
                    formatTypeI.bSubFrameSize = extra[pos++];
                    formatTypeI.bBitResolution = extra[pos++];
                    formatTypeI.bSamFreqType = extra[pos++];
                    uint8_t lv = extra[pos++];
                    uint8_t mv = extra[pos++];
                    uint8_t hv = extra[pos++];
                    formatTypeI.tSamFreq = hv << 16 | mv << 8 | lv;
                    asSpecific.formatTypeIDescr_ = formatTypeI;
                } else {
                    LOGI("not supported format type: %d", bFormatType);
                }
            }
            break;
        default:
            LOGI("not supported ASSpecType: %d", bDescriptorSubtype);
            break;
        }

        remainBytes -= bLength;
        extra = extra + bLength;
    }

    interface->audioSpec_.asSpecific_ = asSpecific;

    return 0;
}

void UACDevice::scanStreamInterface() {

    // find audio interface
    auto cnt = config_->bNumInterfaces;
    LOGE("we're here, cnt: %d", cnt);
    if(cnt > 0) {
        for(int interfaceIdx = 0; interfaceIdx < cnt; interfaceIdx++) {
            auto num_altsetting = config_->interface[interfaceIdx].num_altsetting;

            LOGE("we're here, num_altsetting: %d", num_altsetting);
            for(int settingIdx = 0; settingIdx < num_altsetting; settingIdx++) {

                const libusb_interface_descriptor *ifDescr = &config_->interface[interfaceIdx].altsetting[settingIdx];

                LOGE("we're here, setting: len %d, type %d, ifNum %d, settingN %d, epNum %d, mclass %d, subclass %d, proto %d, strIf %d, extLen %d, ep %p",
                        ifDescr->bLength, ifDescr->bDescriptorType, ifDescr->bInterfaceNumber, ifDescr->bAlternateSetting,
                        ifDescr->bNumEndpoints, ifDescr->bInterfaceClass, ifDescr->bInterfaceSubClass,
                        ifDescr->bInterfaceProtocol, ifDescr->iInterface, ifDescr->extra_length, ifDescr->endpoint);
                if(ifDescr->extra) {
                    LOGE("we're here, inteface extra: %s", bin2str(ifDescr->extra, ifDescr->extra_length).c_str());
                }
                if(ifDescr->endpoint && ifDescr->endpoint->extra) {
                    LOGE("we're here, ep extra: %s, maxLen %d", bin2str(ifDescr->endpoint->extra, ifDescr->endpoint->extra_length).c_str(), ifDescr->endpoint->wMaxPacketSize);
                }

                if(ifDescr->bInterfaceClass == LIBUSB_CLASS_AUDIO && ifDescr->bInterfaceSubClass == 0x2) { // audio, stream

                    if(ifDescr->endpoint) {
                        std::shared_ptr<UACInterface> interface = std::make_shared<UACInterface>();
                        interface->ifDescr_ = ifDescr;
                        interface->epDescr_ = ifDescr->endpoint;

                        _parseAudioStreamSpecific(interface, ifDescr->extra, ifDescr->extra_length);

                        if(interface->audioSpec_.asSpecific_.formatTypeIDescr_.bBitResolution != 0x10) { // not 16bit
                            continue;
                        }

                        int key = ifDescr->bInterfaceNumber << 8 | ifDescr->bAlternateSetting;
                        auto it = streamIfs_.find(key);
                        if(it == streamIfs_.end()) {
                            streamIfs_[key] = interface;
                        }

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

    /* per packet */
    unsigned char *pktbuf;
    struct libusb_iso_packet_descriptor *pkt;
    std::string recv;

    LOGE("we're here, _process_payload_iso, num_iso_packets: %d", transfer->num_iso_packets);
    for (int packet_id = 0; packet_id < transfer->num_iso_packets; ++packet_id) {

        pkt = transfer->iso_packet_desc + packet_id;

        if (pkt->status != LIBUSB_TRANSFER_COMPLETED) {
            LOGE("bad packet:status=%d,actual_length=%d", pkt->status, pkt->actual_length);

			libusb_clear_halt(device->usbDeviceHandle_, device->selectedIf_->epDescr_->bEndpointAddress);
            continue;
        }


        pktbuf = libusb_get_iso_packet_buffer_simple(transfer, packet_id);
        if(pktbuf == NULL)
        {
            LOGE("receive pktbuf null\n");
            continue;
        }

        LOGE("we're here, per packet, len %d|%d", pkt->length, pkt->actual_length);
        //LOGE("we're here, per packet, len %d|%d, buf: %s", pkt->length, pkt->actual_length, bin2str(pktbuf, pkt->actual_length).c_str());

        recv.append((char*)pktbuf, pkt->actual_length);

    }   // for

    if(device->isRecording_ ) {
        // todo save recv data to file
    }

    if(device->streamCallback_) {
        device->streamCallback_->onStreaming(recv);
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

    LOGE("we're here, ifs size: %d", streamIfs_.size());
    if(streamIfs_.size() <= 0 || !selectedIf_){
        LOGE("no valid interfaces");
        return;
    }

    auto endpoint = selectedIf_->epDescr_;

    uint8_t endpointAddress = endpoint->bEndpointAddress;

    int val = endpoint->wMaxPacketSize;
    int r = val & 0x07ff;

    int ep_type = (enum libusb_transfer_type) (endpoint->bmAttributes & 0x3);
    if (ep_type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS
            || ep_type == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
        r *= (1 + ((val >> 11) & 3));
    }
    size_t endpoint_bytes_per_packet = r;
    size_t total_transfer_size = endpoint_bytes_per_packet * PACKETS_PER_TRANSFER;
    libusb_transfer* transfers[NUM_TRANSFER_BUFS];
    uint8_t* transfer_bufs[NUM_TRANSFER_BUFS];

    LOGD("Set up the transfers\n");

    LOGD("before fill EndpointAddress:%d, per_packet:%d, packets:%d, total_transfer_size:%d\n",
                       endpointAddress, endpoint_bytes_per_packet, PACKETS_PER_TRANSFER, total_transfer_size);
    for (int transfer_id = 0; transfer_id < NUM_TRANSFER_BUFS; ++transfer_id)
    {
        LOGD("fill transfer_id:%d\n", transfer_id);
        libusb_transfer *transfer = libusb_alloc_transfer(PACKETS_PER_TRANSFER);
        transfers[transfer_id] = transfer;
        transfer_bufs[transfer_id] = (unsigned char *)malloc(total_transfer_size);
        if(transfer_bufs[transfer_id] == nullptr) {
            continue;
        }
        memset(transfer_bufs[transfer_id], 0, total_transfer_size);
        libusb_fill_iso_transfer(transfer, usbDeviceHandle_,
            endpointAddress,
            transfer_bufs[transfer_id], total_transfer_size,
            PACKETS_PER_TRANSFER, _stream_callback,
            (void *)this, 5000);

        libusb_set_iso_packet_lengths(transfer, endpoint_bytes_per_packet);

        LOGD("fill transfer_id:%d finished\n", transfer_id);
    }

    LOGD("before submit errno:%d\n", errno);

    for (int transfer_id = 0; transfer_id < NUM_TRANSFER_BUFS; transfer_id++) {
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
    streamCallback_ = cb;
}

int UACDevice::getSampleRate() {
    if(!selectedIf_) return 0;

    return selectedIf_->audioSpec_.asSpecific_.formatTypeIDescr_.tSamFreq;
}

std::string UACDevice::getSupportSampleRates() {
    std::set<int> rates;

    for(auto it = streamIfs_.begin(); it != streamIfs_.end(); it++) {
        auto intf = it->second;
        rates.insert(intf->audioSpec_.asSpecific_.formatTypeIDescr_.tSamFreq);
    }

    std::ostringstream oss;
    for(auto it = rates.begin(); it != rates.end(); it++) {
        oss << *it << ",";
    }

    return oss.str();
}


int UACDevice::setSampleRate(int sampleRate) {
    // todo set sample rate

    return 0;
}

int UACDevice::getChannelCount() {
    if(!selectedIf_) return 0;

    return selectedIf_->audioSpec_.asSpecific_.formatTypeIDescr_.bNrChannels;
}

int UACDevice::getBitResolution() {
    if(!selectedIf_) return 0;

    return selectedIf_->audioSpec_.asSpecific_.formatTypeIDescr_.bBitResolution;
}

bool UACDevice::isVolumeAvailable() {
    auto feature = ctrlIf_->audioSpec_.acSpecific_.featureUnitDescr_;

    return feature.wBmaControls | (int)AudioControl::VOLUME ? true : false;
}

int UACDevice::getVolume() {
    return _getVolumeRequest(AudioSpecRequestCode::GET_CUR);
}

int UACDevice::getMaxVolume() {
    return _getVolumeRequest(AudioSpecRequestCode::GET_MAX);
}

int UACDevice::getMinVolume() {
    return _getVolumeRequest(AudioSpecRequestCode::GET_MIN);
}

int UACDevice::_getVolumeRequest(AudioSpecRequestCode requestCode) {

    if(!usbDeviceHandle_) {
        LOGE("invalid device");
        return -1;
    }

    int len = 2;
    uint8_t buf[len] = {};

    int ret = libusb_control_transfer(usbDeviceHandle_, (uint8_t)AudioControlRequestType::GET_REQUEST_TO_IF,
                (uint8_t)requestCode, (uint8_t)FeatureUnitControlSelector::VOLUME_CONTROL << 8,
                ctrlIf_->ifDescr_->bInterfaceNumber|(ctrlIf_->audioSpec_.acSpecific_.featureUnitDescr_.bUnitID << 8), buf, len, 500);
    if(ret < 0) {
        LOGE("_getVolumeRequest failed, ret %d(%s)", ret, libusb_error_name(ret));
        return 0;
    }

    uint16_t volume = buf[1] << 8 | buf[0];

    return volume;
}

int UACDevice::setVolume(int volume) {
    if(!usbDeviceHandle_) {
        LOGE("invalid device");
        return -1;
    }

    int len = 2;
    uint8_t buf[len] = {};
    memset(buf, 0, len);

/*
    if(volume < 0x8001 || volume > 0x7fff) {
        LOGE("invalid param");
        return -1;
    }
    */

    buf[0] = volume;
    buf[1] = volume >> 8;

    int ret = libusb_control_transfer(usbDeviceHandle_, (uint8_t)AudioControlRequestType::SET_REQUEST_TO_IF,
                (uint8_t)AudioSpecRequestCode::SET_CUR, (uint8_t)FeatureUnitControlSelector::VOLUME_CONTROL << 8,
                ctrlIf_->ifDescr_->bInterfaceNumber|(ctrlIf_->audioSpec_.acSpecific_.featureUnitDescr_.bUnitID << 8), buf, len, 500);
    if(ret < 0) {
        LOGE("setVolume failed, ret %d(%s)", ret, libusb_error_name(ret));
        return ret;
    }


    return 0;
}

bool UACDevice::isMuteAvailable() {
    auto feature = ctrlIf_->audioSpec_.acSpecific_.featureUnitDescr_;

    return feature.wBmaControls | (int)AudioControl::MUTE ? true : false;

    return false;
}

int UACDevice::setMute(bool isMute) {
    if(!usbDeviceHandle_) {
        LOGE("invalid device");
        return -1;
    }

    int len = 1;
    uint8_t buf[len] = {};

    buf[0] = isMute ? 1 : 0;

    int ret = libusb_control_transfer(usbDeviceHandle_, (uint8_t)AudioControlRequestType::SET_REQUEST_TO_IF,
                (uint8_t)AudioSpecRequestCode::SET_CUR, (uint8_t)FeatureUnitControlSelector::MUTE_CONTROL << 8,
                ctrlIf_->ifDescr_->bInterfaceNumber|(ctrlIf_->audioSpec_.acSpecific_.featureUnitDescr_.bUnitID << 8), buf, len, 500);
    if(ret < 0) {
        LOGE("set mute failed, ret %d(%s)", ret, libusb_error_name(ret));
        return ret;
    }

    return 0;
}

bool UACDevice::isMute() {
    if(!usbDeviceHandle_) {
        LOGE("invalid device");
        return -1;
    }

    int len = 1;
    uint8_t buf[len] = {};

    int ret = libusb_control_transfer(usbDeviceHandle_, (uint8_t)AudioControlRequestType::GET_REQUEST_TO_IF,
                (uint8_t)AudioSpecRequestCode::GET_CUR, (uint8_t)FeatureUnitControlSelector::MUTE_CONTROL << 8,
                ctrlIf_->ifDescr_->bInterfaceNumber|(ctrlIf_->audioSpec_.acSpecific_.featureUnitDescr_.bUnitID << 8), buf, len, 500);
    if(ret < 0) {
        LOGE("get mute failed, ret %d(%s)", ret, libusb_error_name(ret));
        return false;
    }

    LOGE("we're here, isMute: %s", bin2str(buf, len).c_str());

    bool isMute = buf[0] ? true : false;

    return isMute;
}


}