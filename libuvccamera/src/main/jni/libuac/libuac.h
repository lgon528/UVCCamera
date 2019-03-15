//
// Created by forrestluo on 2019/2/27.
//

#ifndef __LIBUAC_H_H
#define __LIBUAC_H_H

#include "libusb/libusb/libusb.h"
#include <thread>
#include <vector>
#include <map>

namespace libuac {

typedef std::string Bytes;

const int PACKETS_PER_TRANSFER = 16;
const int NUM_TRANSFER_BUFS = 10;

enum class ASIDSubtype : uint8_t {
    AS_DESCRIPTOR_UNDEFINED = 0x00,
    AS_GENERAL              = 0x01,
    FORMAT_TYPE             = 0x02,
    FORMAT_SPECIFIC         = 0x03,
};

enum class FormatType : uint8_t {
    FORMAT_TYPE_UNDEFINED   = 0x00,
    FORMAT_TYPE_I           = 0x01,
    FORMAT_TYPE_II          = 0x02,
    FORMAT_TYPE_III         = 0x03,
};

enum class FormatTypeI : uint8_t {
    TYPE_I_UNDEFINED    = 0x0000,
    PCM                 = 0x0001,
    PCM8                = 0x0002,
    IEEE_FLOAT          = 0x0003,
    ALAW                = 0x0004,
    MULAW               = 0x0005,
};

struct ASGeneralInterfaceDescriptor {
    /**
    The Terminal ID of the Terminal to which the endpoint of this interface is connected.
    */
    uint8_t bTerminalLink;

    /**
    Interface delay.
    */
    uint8_t bDelay;

    /**
    The Audio Data Format that has to be used to communicate with this interface
    TYPE_I_UNDEFINED 0x0000
    PCM 0x0001
    PCM8 0x0002
    IEEE_FLOAT 0x0003
    ALAW 0x0004
    MULAW 0x0005
    */
    uint16_t wFormatTag;
};

struct FormatTypeIDescriptor {
    /**
     Indicates the number of physical channels in the audio data stream.
    */
    uint8_t bNrChannels;

    /**
     The number of bytes occupied by one audio subframe. Can be 1, 2, 3 or 4.
    */
    uint8_t bSubFrameSize;

    /**
     The number of effectively used bits from the available bits in an audio subframe.
    */
    uint8_t bBitResolution;

    /**
     Indicates how the sampling frequency can be programmed:
         0: Continuous sampling frequency
         1..255: The number of discrete sampling frequencies supported by the
         isochronous data endpoint of the AudioStreaming interface (ns)
    */
    uint8_t bSamFreqType;

    /**
     Sampling frequency in Hz for this isochronous data endpoint
    */
    uint64_t tSamFreq;
};

struct AudioStreamSpecific {
    ASGeneralInterfaceDescriptor asGeneralIfDescr_{};
    FormatTypeIDescriptor formatTypeIDescr_{};
};

class UACInterface {
public:
    const libusb_interface_descriptor *ifDescr_;
    const libusb_endpoint_descriptor *epDescr_;

    AudioStreamSpecific asSpecific_;

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


    int getConfig();
    void scanControlInterface();
    void scanStreamInterface();

private:
    int _startStreaming();
    int _stopStreaming();
    int _parseAudioControlSpecific(std::shared_ptr<UACInterface> interface, const unsigned char *extra, const int len);
    int _parseAudioStreamSpecific(std::shared_ptr<UACInterface> interface, const unsigned char *extra, const int len);
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
    std::map<int, std::shared_ptr<UACInterface>> streamIfs_;
    std::shared_ptr<UACInterface> selectedIf_;
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

    int init(std::string usbfs);
    int destroy();
    std::shared_ptr<UACDevice> findDevice(const int vid, const int pid, 
                        int fd, int busnum, int devaddr, const std::string sn, std::string usbfs);
    std::map<std::string, std::shared_ptr<UACDevice>> getDevices();

    bool isAnyDeviceOpened();
    void startThread();
    void dumpDevices();

private:
    libusb_context *usbContext_;
    std::map<std::string, std::shared_ptr<UACDevice>> devices_;
    std::thread usbThread_;

    bool running_ = false;
};


}

#endif //__LIBUAC_H_H
