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

enum class ASSpecType : uint8_t {
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


enum class AudioControl: uint16_t {
    MUTE                = 0x1,
    VOLUME              = 0x1 << 1,
    BASS                = 0x1 << 2,
    MID                 = 0x1 << 3,
    TREBLE              = 0x1 << 4,
    GRAPHIC_EQUALIZER   = 0x1 << 5,
    GAIN                = 0x1 << 6,
    DELAY               = 0x1 << 7,
    BASS_BOOST          = 0x1 << 8,
    LOUDNESS            = 0x1 << 9,
};

enum class ACSpecType : uint8_t {
    AC_DESCRIPTOR_UNDEFINED = 0x0,
    HEADER                  = 0x1,
    INPUT_TERMINAL          = 0x2,
    OUTPUT_TERMINAL         = 0x3,
    MIXER_UNIT              = 0x4,
    SELECTOR_UNIT           = 0x5,
    FEATURE_UNIT            = 0x6,
    PROCESSING_UNIT         = 0x7,
    EXTENSION_UNIT          = 0x8,
};


enum class AudioControlRequestType : uint8_t {
    SET_REQUEST_TO_IF = 0x21,
    SET_REQUEST_TO_EP = 0x22,
    GET_REQUEST_TO_IF = 0xA1,
    GET_REQUEST_TO_EP = 0xA2,
};

enum class AudioSpecRequestCode : uint8_t {
    REQUEST_CODE_UNDEFINED  = 0x00,
    SET_CUR                 = 0x01,
    GET_CUR                 = 0x81,

    SET_MIN                 = 0x02,
    GET_MIN                 = 0x82,

    SET_MAX                 = 0x03,
    GET_MAX                 = 0x83,

    SET_RES                 = 0x04,
    GET_RES                 = 0x84,

    SET_MEM                 = 0x05,
    GET_MEM                 = 0x85,

    GET_STAT                = 0xFF,
};

enum class FeatureUnitControlSelector : uint8_t {
    FU_CONTROL_UNDEFINED        = 0x00,
    MUTE_CONTROL                = 0x01,
    VOLUME_CONTROL              = 0x02,
    BASS_CONTROL                = 0x03,
    MID_CONTROL                 = 0x04,
    TREBLE_CONTROL              = 0x05,
    GRAPHIC_EQUALIZER_CONTROL   = 0x06,
    AUTOMATIC_GAIN_CONTROL      = 0x07,
    DELAY_CONTROL               = 0x08,
    BASS_BOOST_CONTROL          = 0x09,
    LOUDNESS_CONTROL            = 0x0A,
};

struct FeatureUnitDescriptor {
    /**
    Constant uniquely identifying the Unit within the audio function. This value is used in all requests to address this Unit.
    */
    uint8_t bUnitID;

    /**
    ID of the Unit or Terminal to which this Feature Unit is connected.
    */
    uint8_t bSourceID;

    /**
    Size in bytes of an element of the bmaControls() array: n
    */
    uint8_t bControlSize;

    /**
    A bit set to 1 indicates that the mentioned Control is supported for master channel 0:
        D0: Mute
        D1: Volume
        D2: Bass
        D3: Mid
        D4: Treble
        D5: Graphic Equalizer
        D6: Automatic Gain
        D7: Delay
        D8: Bass Boost
        D9: Loudness
        D10..(n*8-1): Reserved
    */
    uint16_t wBmaControls;
};

struct AudioControlSpecific {
    FeatureUnitDescriptor featureUnitDescr_;
};


union AudioSpecific {
    AudioSpecific() {}
    AudioStreamSpecific asSpecific_;
    AudioControlSpecific acSpecific_;
};

class UACInterface {
public:
    int claim(libusb_device_handle *devHandle);
    int release(libusb_device_handle *devHandle);

public:
    const libusb_interface_descriptor *ifDescr_;
    const libusb_endpoint_descriptor *epDescr_;

    bool isCtrl_ = false;
    AudioSpecific audioSpec_;
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

    int getSampleRate();
    std::string getSupportSampleRates();
    int setSampleRate(int sampleRate);

    int getChannelCount();
    int getBitResolution();

    bool isVolumeAvailable();
    int getVolume();
    int getMaxVolume();
    int setVolume(int volume);

    bool isMuteAvailable();
    int setMute(bool isMute);
    bool isMute();


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
    std::string deviceName_;
    std::string recordFilePath_;
    std::shared_ptr<IAudioStreamCallback> streamCallback_;
    std::shared_ptr<UACInterface> ctrlIf_;
    std::map<int, std::shared_ptr<UACInterface>> streamIfs_;
    std::shared_ptr<UACInterface> selectedIf_;

    libusb_device *usbDevice_;
    libusb_device_handle *usbDeviceHandle_;
    libusb_config_descriptor *config_;
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
