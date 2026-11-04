#ifndef SBLIB_KNX_USEREEPROM_BCU2_H_
#define SBLIB_KNX_USEREEPROM_BCU2_H_

#include "sblib/eib/userEepromBCU1.h"
#include "sblib/eib/serial_number.h"

/**
 * The BCU 2 user EEPROM
 * @details Can be accessed by name, like userEeprom.manuDataH() and as an array, like
 *          userEeprom[addr]. Please note that the @ref startAddress is subtracted.
 *          E.g. userEeprom[0x107] is the correct address for userEeprom.version() not userEeprom[0x07].
 *
 * @note KNX Spec. 3.0 9/4/1 Basic and System Components 5.1.2.12 Memory Map p.43ff
 *       - EEPROM: 0x100 - 0x4df (992 bytes)
 *       - single chip protected EEPROM: 0x4e0 - 0x4ff (32 bytes)
 *       - total: 0x100 - 0x4ff (1024 bytes)
 * @note KNX Spec. 3.0 06 Profiles 4.2.10 User EEPROM p. 41
 *       - User EEPROM: 0x119 - 0x46f (855 bytes)
 */
class UserEepromBCU2 : public UserEepromBCU1
{
public:
    /// The total BCU2 EEPROM range is 0x100-0x4ff (System EEPROM, single chip protected EEPROM, User EEPROM combined).
    /// UserEepromBCU2 is implemented with this total BCU2 EEPROM range, which is fine,
    /// but exposes BCU2 system EEPROM to the user, which is not great.

    explicit UserEepromBCU2() : UserEepromBCU2(0x100, 1024, 1024) {}

    [[nodiscard]] virtual uint32_t serialSize() const { return KNX_SERIAL_NUMBER_LENGTH; };
    [[nodiscard]] virtual uint32_t orderSize() const { return 10; };
    [[nodiscard]] virtual uint32_t orderInfoSize() const { return ORDER_INFO_SIZE; };

    static constexpr uint32_t ORDER_INFO_SIZE = 10; //!< Length of the ordering information in bytes

    [[nodiscard]] virtual uint8_t appType() const { return field8(appTypeOffset); }
    virtual void setAppType(const uint8_t value) { setField8(appTypeOffset, value); }
    /** @return Load state of interface object @p objectIdx. */
    [[nodiscard]] virtual uint8_t loadState(const uint8_t objectIdx) const { return field8(loadStateOffset + objectIdx); }
    /** Set the load state of interface object @p objectIdx. Does not mark the memory as modified. */
    virtual void setLoadState(const uint8_t objectIdx, const uint8_t state) { setField8(loadStateOffset + objectIdx, state); }
    /**
     * Read @p length bytes from the load state of interface object @p objectIdx on.
     * More bytes than there are load states read on into the table pointers behind them,
     * which DMP_LoadStateMachineRead_RCo_Mem does today.
     */
    virtual void readLoadStates(const uint8_t objectIdx, uint8_t* buffer, const uint32_t length) const
    {
        readField(loadStateOffset + objectIdx, buffer, length);
    }
    [[nodiscard]] virtual uint16_t commsTabAddr() const { return field16(commsTabAddrOffset); }
    virtual void setCommsTabAddr(const uint16_t value) { setField16(commsTabAddrOffset, value); }
    [[nodiscard]] virtual uint16_t commsSeg0Addr() const { return field16(commsSeg0AddrOffset); }
    virtual void setCommsSeg0Addr(const uint16_t value) { setField16(commsSeg0AddrOffset, value); }
    [[nodiscard]] virtual uint16_t commsSeg1Addr() const { return field16(commsSeg1AddrOffset); }
    virtual void setCommsSeg1Addr(const uint16_t value) { setField16(commsSeg1AddrOffset, value); }
    [[nodiscard]] virtual uint16_t eibObjAddr() const { return field16(eibObjAddrOffset); }
    virtual void setEibObjAddr(const uint16_t value) { setField16(eibObjAddrOffset, value); }
    [[nodiscard]] virtual uint8_t eibObjCount() const { return field8(eibObjCountOffset); }
    virtual void setEibObjCount(const uint8_t value) { setField8(eibObjCountOffset, value); }
    [[nodiscard]] virtual uint16_t addrTabAddr() const { return field16(addrTabAddrOffset); }
    virtual void setAddrTabAddr(const uint16_t value) { setField16(addrTabAddrOffset, value); }
    [[nodiscard]] virtual uint16_t assocTabAddr() const { return field16(assocTabAddrOffset); }
    virtual void setAssocTabAddr(const uint16_t value) { setField16(assocTabAddrOffset, value); }
    /** Copy the serial (serialSize() bytes) into @p buffer. */
    virtual void readSerial(uint8_t* buffer) const { readField(serialOffset, buffer, serialSize()); }
    /** Set the serial (serialSize() bytes) from @p buffer. Does not mark the memory as modified. */
    virtual void writeSerial(const uint8_t* buffer) { writeField(serialOffset, buffer, serialSize()); }
    /** Set the first @p length bytes (at most @ref orderSize()) of the hardware type. Does not mark the memory as modified. */
    virtual void writeOrder(const uint8_t* buffer, const uint32_t length) { writeField(orderOffset, buffer, length); }

    [[nodiscard]] virtual uint8_t padding1() const { return field8(padding1Offset); }
    virtual void setPadding1(const uint8_t value) { setField8(padding1Offset, value); }
    [[nodiscard]] virtual uint8_t serviceControl() const { return field8(serviceControlOffset); }
    virtual void setServiceControl(const uint8_t value) { setField8(serviceControlOffset, value); }
    [[nodiscard]] virtual uint8_t padding2() const { return field8(padding2Offset); }
    virtual void setPadding2(const uint8_t value) { setField8(padding2Offset, value); }
    /** Copy the orderInfo (orderInfoSize() bytes) into @p buffer. */
    virtual void readOrderInfo(uint8_t* buffer) const { readField(orderInfoOffset, buffer, orderInfoSize()); }
    /** Set the orderInfo (orderInfoSize() bytes) from @p buffer. Does not mark the memory as modified. */
    virtual void writeOrderInfo(const uint8_t* buffer) { writeField(orderInfoOffset, buffer, orderInfoSize()); }

protected:
    // BCU2 has no variable EEPROM start or size, so make this constructor protected
    UserEepromBCU2(const uint32_t start, const uint32_t size, const uint32_t flashSize)
       :
        UserEepromBCU1(start, size, flashSize) {}

private:
    ///\todo refactor PropertiesBCU2, so that it doesn't need to access private members of UserEepromBCU2,
    ///      then remove this friend declaration
    friend class PropertiesBCU2;

    static constexpr uint32_t manufacturerHOffset = 0x03; //!< 0x0103: Software manufacturer high byte
    static constexpr uint32_t portADDROffset = 0x0c; //!< 0x010c: Port A DDR settings

    static constexpr uint32_t appTypeOffset = 0x015; //!< 0x0115: \todo Application program type: 0=BCU2, else BCU1

    static constexpr uint32_t loadStateOffset = 0x370;      //!< 0x0470: Load state of the system interface objects
    static constexpr uint32_t addrTabAddrOffset = 0x378;    //!< 0x0478: Address of the address table
    static constexpr uint32_t assocTabAddrOffset = 0x37A;   //!< 0x047a: Address of the association table
    static constexpr uint32_t commsTabAddrOffset = 0x37C;   //!< 0x047c: Address of the communication object table
    static constexpr uint32_t commsSeg0AddrOffset = 0x37E;  //!< 0x047e: Address of communication object memory segment 0 // TODO needs implementation, see handleTaskCtrl2(...) in properties.cpp
    static constexpr uint32_t commsSeg1AddrOffset = 0x380;  //!< 0x0480: Address of communication object memory segment 1 // TODO needs implementation, see handleTaskCtrl2(...) in properties.cpp
    static constexpr uint32_t eibObjAddrOffset = 0x382;     //!< 0x0482: Address of the application program EIB objects, 0 if unused. // TODO needs implementation, see handleTaskCtrl1(...) in properties.cpp
    static constexpr uint32_t eibObjCountOffset = 0x384;    //!< 0x0484: Number of application program EIB objects. // TODO needs implementation, see handleTaskCtrl1(...) in properties.cpp
    static constexpr uint32_t padding1Offset = 0x385;       //!< 0x0485: Padding 1
    static constexpr uint32_t serviceControlOffset = 0x386; //!< 0x0486: Service control
    static constexpr uint32_t padding2Offset = 0x388;       //!< 0x0488: Padding 2
    static constexpr uint32_t serialOffset = 0x38A;         //!< 0x048A-0x48f: Hardware serial number (4 byte aligned)

    static constexpr uint32_t orderOffset = 0x390;     //!< 0x0490-0x0499: Hardware Type
    static constexpr uint32_t orderInfoOffset = 0x39A; //!< 0x049A-0x04A3: Ordering information

    ///\todo It's here (and maybe invalid offsets) because PropertiesBCU2 needs them, which also needs to be refactored
    static constexpr uint32_t addrTabMcbOffset = 0x3A4;   //!< 0x04A4-0x04AB:
    static constexpr uint32_t assocTabMcbOffset = 0x3AC;  //!< 0x04AC-0x04B3:
    static constexpr uint32_t commsTabMcbOffset = 0x3B4;  //!< 0x04B4-0x04BB:
    static constexpr uint32_t eibObjMcbOffset = 0x3BC;    //!< 0x04BC-0x04C3:
    static constexpr uint32_t commsSeg0McbOffset = 0x3C3; //!< 0x04C4-0x04CB: ///\todo address missmatch?
    static constexpr uint32_t eibObjVerOffset = 0x3CC;    //!< 0x04CC-0x04D0: Application program 1 version
    static constexpr uint32_t commsSeg0VerOffset = 0x3D1; //!< 0x04D1-0x04D5: Application program 2 version
    // end todo
};

#endif /* SBLIB_KNX_USEREEPROM_BCU2_H_ */
