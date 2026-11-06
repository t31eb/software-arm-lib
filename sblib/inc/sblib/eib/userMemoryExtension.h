/**************************************************************************//**
 * @addtogroup SBLIB_MAIN_GROUP Selfbus KNX-Library
 * @defgroup SBLIB_SUB_GROUP_KNX KNX
 * @ingroup SBLIB_MAIN_GROUP
 * @brief
 * @details
 *
 *
 * @{
 *
 * @file   userMemoryExtension.h
 * @author Selfbus
 * @bug No known bugs.
 ******************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#ifndef SBLIB_KNX_USERMEMORY_EXTENSION_H_
#define SBLIB_KNX_USERMEMORY_EXTENSION_H_

#include <sblib/eib/memory.h>
#include <sblib/eib/userEepromBackend.h>

/**
 * A further range of non-volatile configuration memory beside the user EEPROM, e.g. a segment of the product database
 * outside of it (MultiSensorAktor: 0x6000-0x6EFF).
 *
 * @details The replacement of a @ref MemMapper range for applications with an external EEPROM: the bytes are kept by
 *          a @ref UserEepromBackend, e.g. a second @ref UserEepromSpi in the same chip, without a RAM buffer and
 *          without a flash sector. Register it with @ref BcuDefault::setUserMemoryExtension; memory telegrams and
 *          @ref BcuDefault::readUserMemory / @ref BcuDefault::writeUserMemory reach it then, and it is flushed before
 *          the user EEPROM (the load states).
 */
class UserMemoryExtension : public Memory
{
public:
    UserMemoryExtension() = delete;
    UserMemoryExtension(const UserMemoryExtension&) = delete;
    UserMemoryExtension& operator=(const UserMemoryExtension&) = delete;

    /**
     * @param start   Virtual (ETS) address of the first byte
     * @param size    Size in bytes
     * @param backend Keeps the bytes, its size must be @p size; it has to live as long as the extension
     */
    UserMemoryExtension(uint32_t start, uint32_t size, UserEepromBackend* backend);

    /** No pointers into the extension, use @ref readBlock / @ref writeBlock. Calls @ref fatalError(). */
    uint8_t& operator[](uint32_t address) const override;
    uint8_t& operator[](uint32_t address) override;

    [[nodiscard]] uint8_t getUInt8(uint32_t address) const override;
    [[nodiscard]] uint16_t getUInt16(uint32_t address) const override;
    [[nodiscard]] bool readBlock(uint32_t virtAddress, uint8_t* buffer, uint32_t length) const override;
    [[nodiscard]] bool writeBlock(uint32_t virtAddress, const uint8_t* buffer, uint32_t length) override;

    /** @return The backend that keeps the bytes. */
    [[nodiscard]] UserEepromBackend* backend() const { return storage; }

private:
    UserEepromBackend* const storage;
};

#endif /* SBLIB_KNX_USERMEMORY_EXTENSION_H_ */
/** @}*/
