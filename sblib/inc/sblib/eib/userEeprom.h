/*
 * userEeprom.h
 *
 *  Created on: 19.11.2021
 *      Author: dridders
 */

#ifndef SBLIB_KNX_USEREEPROM_H_
#define SBLIB_KNX_USEREEPROM_H_

#include <sblib/eib/memory.h>
#include <sblib/eib/userEepromBackend.h>
#include <sblib/platform.h>


/**
 * The user EEPROM
 * @details Can be accessed by name, like userEeprom.manuDataH() and as an array, like
 *          userEeprom[addr]. Please note that the @ref startAddress is subtracted.
 *          E.g. userEeprom[0x107] is the correct address for userEeprom.version() not userEeprom[0x07].
 *
 * @note KNX Spec. 3.0 9/4/1 Basic and System Components
 *      - BCU 1 (256 bytes), 3.1.10.3.1 p.13ff
 *      - BCU 2 (1024 bytes), 5.1.2.12.5 p.45ff
 *      - BIM M112, not in Spec. 3.0 some information in 06 Profiles 4.2.10 User EEPROM p. 41
 * @note KNX Spec. 3.0 06 Profiles 4.2.10 User EEPROM p. 41
 *      - BCU 1 0x119 - 0x1fe (only the range of the UserEEPROM, not the whole EEPROM)
 *      - BCU 2 0x119 - 0x46f (only the range of the UserEEPROM, not the whole EEPROM)
 *      - BIM M112 0x4000 - 0xcfff
 * @note The bytes are kept by a @ref UserEepromBackend. By default this is a mirror of the whole user EEPROM in RAM
 *       (@ref UserEepromMirror); the pointer interfaces (@ref operator[], @ref userEepromData, @ref addrTab) only
 *       work with it.
 */
class UserEeprom : public Memory
{
public:
    // Delete default and assigment constructors
    UserEeprom() = delete;
    UserEeprom(const UserEeprom&) = delete;
    UserEeprom(UserEeprom&&) = delete;
    UserEeprom& operator=(const UserEeprom&) = delete;
    UserEeprom& operator=(UserEeprom&&) = delete;

    /** User EEPROM with a RAM mirror (@ref UserEepromMirror), persisted in pages of @p flashSize bytes. */
    explicit UserEeprom(uint32_t start, uint32_t size, uint32_t flashSize);

    /**
     * User EEPROM with the given backend.
     *
     * @param backend Keeps the bytes, its @ref UserEepromBackend::size must be @p size. It has to live as long as the
     *                user EEPROM and isn't deleted with it, like the former RAM mirror (a destructor would cost
     *                about 750 bytes of flash in the MSA for the vtables of the derived classes).
     */
    UserEeprom(uint32_t start, uint32_t size, UserEepromBackend* backend);

    ///\todo Enable deprecation warning after making userEepromData private
    //[[deprecated("Use getUInt8, getUInt16, getUInt32 or the [] operator.")]]
    /** The RAM mirror of the backend, @c nullptr if it has none. */
    uint8_t* const userEepromData;

    /** @return The backend that keeps the bytes. */
    [[nodiscard]] UserEepromBackend* backend() const { return storage; }

    [[nodiscard]] virtual uint8_t optionReg() const = 0;
    virtual void setOptionReg(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t manuDataH() const = 0;
    virtual void setManuDataH(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t manuDataL() const = 0;
    virtual void setManuDataL(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t manufacturerH() const = 0;
    virtual void setManufacturerH(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t manufacturerL() const = 0;
    virtual void setManufacturerL(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t deviceTypeH() const = 0;
    virtual void setDeviceTypeH(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t deviceTypeL() const = 0;
    virtual void setDeviceTypeL(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t version() const = 0;
    virtual void setVersion(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t checkLimit() const = 0;
    virtual void setCheckLimit(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t appPeiType() const = 0;
    virtual void setAppPeiType(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t syncRate() const = 0;
    virtual void setSyncRate(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t portCDDR() const = 0;
    virtual void setPortCDDR(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t portADDR() const = 0;
    virtual void setPortADDR(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t runError() const = 0;
    virtual void setRunError(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t routeCnt() const = 0;
    virtual void setRouteCnt(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t maxRetransmit() const = 0;
    virtual void setMaxRetransmit(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t confDesc() const = 0;
    virtual void setConfDesc(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t assocTabPtr() const = 0;
    virtual void setAssocTabPtr(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t commsTabPtr() const = 0;
    virtual void setCommsTabPtr(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t usrInitPtr() const = 0;
    virtual void setUsrInitPtr(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t usrProgPtr() const = 0;
    virtual void setUsrProgPtr(uint8_t value) = 0;

    [[nodiscard]] virtual uint8_t addrTabSize() const = 0;
    virtual void setAddrTabSize(uint8_t value) = 0;
    [[nodiscard]] virtual uint8_t* addrTab() const = 0;
    /** @return Own physical address, the first entry of the address table (high byte first). */
    [[nodiscard]] virtual uint16_t physicalAddress() const = 0;
    /** Set the own physical address in the first entry of the address table. Does not mark the memory as modified. */
    virtual void setPhysicalAddress(uint16_t address) = 0;

    /**
     * Access the user EEPROM like an ordinary array.
     *
     * @param address  The address of the data byte to access.
     * @return The data byte.
     * @warning Needs a backend with a RAM mirror, calls @ref fatalError() otherwise.
     */
    uint8_t& operator[](uint32_t address) const override;

    /**
     * Access the user EEPROM like an ordinary array.
     *
     * @param address  The address of the data byte to access.
     * @return The data byte.
     */
    uint8_t& operator[](uint32_t address) override;

    [[nodiscard]] uint8_t getUInt8(uint32_t address) const override;
    [[nodiscard]] uint16_t getUInt16(uint32_t address) const override;

    [[nodiscard]] bool readBlock(uint32_t virtAddress, uint8_t* buffer, uint32_t length) const override;

    /**
     * @copydoc Memory::writeBlock
     * @note Marks the user EEPROM as modified, like @ref modified(true).
     */
    [[nodiscard]] bool writeBlock(uint32_t virtAddress, const uint8_t* buffer, uint32_t length) override;

    /**
     * Mark/unmark the user EEPROM as modified. The EEPROM will be written to flash when the
     * bus is idle, all telegrams are processed, and no direct data connection is open.
     *
     * @param newModified Set true to mark the eeprom as modified
     *
     */
    void modified(bool newModified);

    /**
     * Test if the user EEPROM is modified.
     */
    [[nodiscard]] bool isModified() const;

    [[nodiscard]] bool writeDelayElapsed() const;

    /**
     * If user-eeprom is modified, changes are written to the mcu's flash
     * @warn While the eeprom is written, all interrupts are disabled.
     */
    void writeUserEeprom();

    /** Reads the user EEPROM from the backend's persistent copy again, unsaved changes are lost. */
    void readUserEeprom();

protected:
    /**
     * @brief Access the internal @ref userEepromData array directly by raw index, without
     *        any @ref startAddress offset subtraction.
     *
     * @param index  Zero-based index into @ref userEepromData. Must satisfy
     *               <tt>startAddress + index</tt> within the valid array range,
     *               otherwise @ref fatalError() is called.
     * @return Reference to the byte at @ref userEepromData[index].
     * @warning This method is intended for internal use only. Needs a backend with a RAM mirror, calls
     *          @ref fatalError() otherwise.
     */
    [[nodiscard]] uint8_t& directAccess_8(uint32_t index) const;

    /**
     * @brief Value of a byte of the user EEPROM, by offset from its start (no @ref startAddress involved).
     * @details Used by the named getters of the system fields. Replaces the former references of directAccess_8/_16.
     *          Without a range check: @p offset is a constant of the derived class and always inside.
     */
    [[nodiscard]] uint8_t field8(uint32_t offset) const;

    /**
     * @brief Set a byte of the user EEPROM, by offset from its start.
     * @details Does not mark the memory as modified, like an assignment through the former references.
     *          Call @ref modified() to persist the change.
     */
    void setField8(uint32_t offset, uint8_t value);

    /**
     * @brief Value of a 16 bit field of the user EEPROM, low byte first, by offset from its start.
     * @details Read byte by byte, so @p offset may be odd.
     */
    [[nodiscard]] uint16_t field16(uint32_t offset) const;

    /**
     * @brief Set a 16 bit field of the user EEPROM, low byte first, by offset from its start.
     * @details Written byte by byte, so @p offset may be odd. Does not mark the memory as modified.
     */
    void setField16(uint32_t offset, uint16_t value);

    /** @brief Copy @p length bytes of the user EEPROM, by offset from its start, into @p buffer. */
    void readField(uint32_t offset, uint8_t* buffer, uint32_t length) const;

    /**
     * @brief Copy @p length bytes from @p buffer into the user EEPROM, by offset from its start.
     * @details Does not mark the memory as modified.
     */
    void writeField(uint32_t offset, const uint8_t* buffer, uint32_t length);

    bool userEepromModified = false;
    uint32_t writeUserEepromTime = 0;

private:
    UserEepromBackend* const storage; //!< keeps the bytes
};


#endif /* SBLIB_KNX_USEREEPROM_H_ */
