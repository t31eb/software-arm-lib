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
 * @file   userEepromBackend.h
 * @author Selfbus
 * @bug No known bugs.
 ******************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#ifndef SBLIB_KNX_USEREEPROM_BACKEND_H_
#define SBLIB_KNX_USEREEPROM_BACKEND_H_

#include <cstdint>

/**
 * Lets a backend stop bus access while it writes the flash outside of @ref UserEepromBackend::flush.
 *
 * @details While the flash is erased or programmed, interrupts are disabled and the bus driver misses edges.
 *          @ref BcuDefault pauses the bus for @ref UserEepromBackend::flush itself; a backend that has to write
 *          in between (e.g. when its cache is full) asks the guard.
 */
class FlashGuard
{
public:
    virtual ~FlashGuard() = default;

    /** Called before the backend writes the flash. Returns when the bus is paused. */
    virtual void pauseForFlash() = 0;

    /** Called after the backend has written the flash. */
    virtual void resumeAfterFlash() = 0;
};

/**
 * Non-volatile storage behind a @ref UserEeprom.
 *
 * @details The @ref UserEeprom checks the range and keeps track of modifications; the backend only moves bytes.
 *          All offsets are relative to the start of the user EEPROM. Implementations:
 *          - @ref UserEepromMirror: the whole user EEPROM as a mirror in RAM, persisted in one flash page.
 *            Default, used by all masks.
 *          - @ref UserEepromFlash: reads directly from the flash, writes through a cache of one flash page.
 */
class UserEepromBackend
{
public:
    UserEepromBackend() = delete;
    UserEepromBackend(const UserEepromBackend&) = delete;
    UserEepromBackend& operator=(const UserEepromBackend&) = delete;
    virtual ~UserEepromBackend() = default;

    /** @param size Size of the user EEPROM in bytes */
    explicit UserEepromBackend(const uint32_t size) : backendSize(size) {}

    /**
     * Prepares the backend for use, e.g. a peripheral it needs.
     *
     * @details Called by @ref BcuDefault::begin before the bus starts. The backends of the internal flash are usable
     *          right after their construction, before @c main().
     */
    virtual void begin() {}

    /** Copies @p length bytes starting at @p offset into @p buffer. The range is checked by the caller. */
    virtual void read(uint32_t offset, uint8_t* buffer, uint32_t length) const = 0;

    /** Copies @p length bytes from @p buffer to @p offset. The range is checked by the caller. */
    virtual void write(uint32_t offset, const uint8_t* buffer, uint32_t length) = 0;

    /** Makes all changes persistent. */
    virtual void flush() = 0;

    /**
     * Reads the persistent content again, changes that weren't written yet are lost.
     * @note A backend with a cache may have written some changes before @ref flush already
     *       (@ref UserEepromFlash on a change of the page), they stay.
     */
    virtual void reload() = 0;

    /**
     * @return The RAM mirror of the whole user EEPROM, or @c nullptr if the backend has none.
     * @note Only for the pointer interfaces of BCU1 and System B (@ref UserEeprom::userEepromData).
     */
    [[nodiscard]] virtual uint8_t* mirror() const { return nullptr; }

    /**
     * @return true if @ref read may be called in an interrupt, also while the main program is in the middle of an
     *         access. The bus interrupt looks up group addresses in the address table (@ref AddrTables::indexOfAddr).
     */
    [[nodiscard]] virtual bool readableInInterrupt() const { return true; }

    /** Sets the guard to ask before writing the flash outside of @ref flush, @c nullptr for none. */
    void setFlashGuard(FlashGuard* guard) { flashGuard = guard; }

    /** @return Size of the user EEPROM in bytes */
    [[nodiscard]] uint32_t size() const { return backendSize; }

protected:
    FlashGuard* flashGuard = nullptr; //!< see @ref setFlashGuard
    const uint32_t backendSize;       //!< size of the user EEPROM in bytes
};

#endif /* SBLIB_KNX_USEREEPROM_BACKEND_H_ */
/** @}*/
