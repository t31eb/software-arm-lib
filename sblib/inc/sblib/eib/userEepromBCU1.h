#ifndef SBLIB_KNX_USEREEPROM_BCU1_H_
#define SBLIB_KNX_USEREEPROM_BCU1_H_

#include <sblib/eib/userEeprom.h>
#include <sblib/bits.h>


/**
 * The BCU 1 user EEPROM
 * @details Can be accessed by name, like userEeprom.manuDataH() and as an array, like
 *          userEeprom[addr]. Please note that the @ref startAddress is subtracted.
 *          E.g. userEeprom[0x107] is the correct address for userEeprom.version() not userEeprom[0x07].
 *
 * @note KNX Spec. 3.0 9/4/1 Basic and System Components 3.1.10.3.1 p.13ff
 *       - EEPROM: 0x100 - 0x1ff (256 bytes)
 * @note KNX Spec. 3.0 06 Profiles 4.2.10 User EEPROM p. 41
 *       - User EEPROM: 0x119 - 0x1fe (230 bytes)
 */
class UserEepromBCU1 : public UserEeprom
{
public:
    // The total BCU1 EEPROM range is 0x100-0x1ff (System EEPROM and User EEPROM combined).
    // UserEepromBCU1 is implemented with this total BCU1 EEPROM range, which is fine,
    // but exposes BCU1 system EEPROM to the user, which is not great.
    explicit UserEepromBCU1() : UserEeprom(0x100, 256, 256) {}

    [[nodiscard]] uint8_t optionReg() const override { return field8(optionRegOffset); }
    void setOptionReg(const uint8_t value) override { setField8(optionRegOffset, value); }
    [[nodiscard]] uint8_t manuDataH() const override { return field8(manuDataHOffset); }
    void setManuDataH(const uint8_t value) override { setField8(manuDataHOffset, value); }
    [[nodiscard]] uint8_t manuDataL() const override { return field8(manuDataLOffset); }
    void setManuDataL(const uint8_t value) override { setField8(manuDataLOffset, value); }
    [[nodiscard]] uint8_t manufacturerH() const override { return field8(manufacturerHOffset); }
    void setManufacturerH(const uint8_t value) override { setField8(manufacturerHOffset, value); }
    [[nodiscard]] uint8_t manufacturerL() const override { return field8(manufacturerLOffset); }
    void setManufacturerL(const uint8_t value) override { setField8(manufacturerLOffset, value); }
    [[nodiscard]] uint8_t deviceTypeH() const override { return field8(deviceTypeHOffset); }
    void setDeviceTypeH(const uint8_t value) override { setField8(deviceTypeHOffset, value); }
    [[nodiscard]] uint8_t deviceTypeL() const override { return field8(deviceTypeLOffset); }
    void setDeviceTypeL(const uint8_t value) override { setField8(deviceTypeLOffset, value); }
    [[nodiscard]] uint8_t version() const override { return field8(versionOffset); }
    void setVersion(const uint8_t value) override { setField8(versionOffset, value); }
    [[nodiscard]] uint8_t checkLimit() const override { return field8(checkLimitOffset); }
    void setCheckLimit(const uint8_t value) override { setField8(checkLimitOffset, value); }
    [[nodiscard]] uint8_t appPeiType() const override { return field8(appPeiTypeOffset); }
    void setAppPeiType(const uint8_t value) override { setField8(appPeiTypeOffset, value); }
    [[nodiscard]] uint8_t syncRate() const override { return field8(syncRateOffset); }
    void setSyncRate(const uint8_t value) override { setField8(syncRateOffset, value); }
    [[nodiscard]] uint8_t portCDDR() const override { return field8(portCDDROffset); }
    void setPortCDDR(const uint8_t value) override { setField8(portCDDROffset, value); }
    [[nodiscard]] uint8_t portADDR() const override { return field8(portADDROffset); }
    void setPortADDR(const uint8_t value) override { setField8(portADDROffset, value); }
    [[nodiscard]] uint8_t runError() const override { return field8(runErrorOffset); }
    void setRunError(const uint8_t value) override { setField8(runErrorOffset, value); }
    [[nodiscard]] uint8_t routeCnt() const override { return field8(routeCntOffset); }
    void setRouteCnt(const uint8_t value) override { setField8(routeCntOffset, value); }
    [[nodiscard]] uint8_t maxRetransmit() const override { return field8(maxRetransmitOffset); }
    void setMaxRetransmit(const uint8_t value) override { setField8(maxRetransmitOffset, value); }
    [[nodiscard]] uint8_t confDesc() const override { return field8(confDescOffset); }
    void setConfDesc(const uint8_t value) override { setField8(confDescOffset, value); }
    [[nodiscard]] uint8_t assocTabPtr() const override { return field8(assocTabPtrOffset); }
    void setAssocTabPtr(const uint8_t value) override { setField8(assocTabPtrOffset, value); }
    [[nodiscard]] uint8_t commsTabPtr() const override { return field8(commsTabPtrOffset); }
    void setCommsTabPtr(const uint8_t value) override { setField8(commsTabPtrOffset, value); }
    [[nodiscard]] uint8_t usrInitPtr() const override { return field8(usrInitPtrOffset); }
    void setUsrInitPtr(const uint8_t value) override { setField8(usrInitPtrOffset, value); }
    [[nodiscard]] uint8_t usrProgPtr() const override { return field8(usrProgPtrOffset); }
    void setUsrProgPtr(const uint8_t value) override { setField8(usrProgPtrOffset, value); }
    [[nodiscard]] virtual uint8_t usrSavePtr() const { return field8(usrSavePtrOffset); }
    virtual void setUsrSavePtr(const uint8_t value) { setField8(usrSavePtrOffset, value); }
    [[nodiscard]] uint8_t addrTabSize() const override { return field8(addrTabSizeOffset); }
    void setAddrTabSize(const uint8_t value) override { setField8(addrTabSizeOffset, value); }
    [[nodiscard]] uint8_t* addrTab() const override { return &directAccess_8(addrTabOffset); }
    [[nodiscard]] uint16_t physicalAddress() const override { return makeWord(field8(addrTabOffset), field8(addrTabOffset + 1)); }
    void setPhysicalAddress(const uint16_t address) override
    {
        setField8(addrTabOffset, HIGH_BYTE(address));
        setField8(addrTabOffset + 1, lowByte(address));
    }
    [[nodiscard]] virtual uint8_t checksum() const { return field8(checksumOffset); }
    virtual void setChecksum(const uint8_t value) { setField8(checksumOffset, value); }

protected:
    // BCU1 has no variable EEPROM start or size, so make this constructor protected
    UserEepromBCU1(const uint32_t start, const uint32_t size, const uint32_t flashSize)
       :
        UserEeprom(start, size, flashSize) {}

    UserEepromBCU1(const uint32_t start, const uint32_t size, UserEepromBackend* backend)
       :
        UserEeprom(start, size, backend) {}

private:
    static constexpr uint32_t optionRegOffset = 0x00;         //!< 0x0100: EEPROM option register
    static constexpr uint32_t manuDataHOffset = 0x01;         //!< 0x0101: Manufacturing data high byte
    static constexpr uint32_t manuDataLOffset = 0x02;         //!< 0x0102: Manufacturing data low byte
    static constexpr uint32_t manufacturerHOffset = 0x03;     //!< 0x0103: Software manufacturer high byte
    static constexpr uint32_t manufacturerLOffset = 0x04;     //!< 0x0104: Software manufacturer low byte
    static constexpr uint32_t deviceTypeHOffset = 0x05;       //!< 0x0105: Device type high byte
    static constexpr uint32_t deviceTypeLOffset = 0x06;       //!< 0x0106: Device type low byte
    static constexpr uint32_t versionOffset = 0x07;           //!< 0x0107: Software version
    static constexpr uint32_t checkLimitOffset = 0x08;        //!< 0x0108: EEPROM check limit
    static constexpr uint32_t appPeiTypeOffset = 0x09;        //!< 0x0109: PEI type that the application program requires
    static constexpr uint32_t syncRateOffset = 0x0a;          //!< 0x010a: Baud rate for serial synchronous PEI
    static constexpr uint32_t portCDDROffset = 0x0b;          //!< 0x010b: Port C DDR settings (PEI type 17)
    static constexpr uint32_t portADDROffset = 0x0c;          //!< 0x010c: Port A DDR settings
    static constexpr uint32_t runErrorOffset = 0x0d;          //!< 0x010d: Runtime error flags
    static constexpr uint32_t routeCntOffset = 0x0e;          //!< 0x010e: Routing count constant
    static constexpr uint32_t maxRetransmitOffset = 0x0f;     //!< 0x010f: NAK and BUSY retransmit limit
    static constexpr uint32_t confDescOffset = 0x10;          //!< 0x0110: Configuration descriptor
    static constexpr uint32_t assocTabPtrOffset = 0x11;       //!< 0x0111: Pointer to association table
    static constexpr uint32_t commsTabPtrOffset = 0x12;       //!< 0x0112: Pointer to communication objects table
    static constexpr uint32_t usrInitPtrOffset = 0x13;        //!< 0x0113: Pointer to user initialization function
    static constexpr uint32_t usrProgPtrOffset = 0x14;        //!< 0x0114: Pointer to user program function
    static constexpr uint32_t usrSavePtrOffset = 0x15;        //!< 0x0115: Pointer to user save function (BCU1 only)
    static constexpr uint32_t addrTabSizeOffset = 0x16;       //!< 0x0116: Size of the address table
    static constexpr uint32_t addrTabOffset = 0x17;           //!< 0x0117+: Address table, 2 bytes per entry. Real array size is addrTabSize*2
    static constexpr uint32_t user230bytesStartOffset = 0x19; //!< 0x0119: User EEPROM: 230 bytes (BCU1)
    static constexpr uint32_t checksumOffset = 0xff;          //!< 0x01ff: EEPROM checksum (BCU1 only)
};

#endif /* SBLIB_KNX_USEREEPROM_BCU1_H_ */
