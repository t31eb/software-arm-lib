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
 * @file   userEepromFlash.h
 * @author Selfbus
 * @bug No known bugs.
 ******************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#ifndef SBLIB_KNX_USEREEPROM_FLASH_H_
#define SBLIB_KNX_USEREEPROM_FLASH_H_

#include <sblib/eib/userEepromBackend.h>
#include <sblib/platform.h>

class UserEepromFlash;

/**
 * One page of the flash in RAM, the write cache of one or more @ref UserEepromFlash.
 *
 * @details Backends that share a cache need the RAM of one page together, e.g. the user EEPROM and a
 *          @ref UserMemoryExtension in the flash. The cache holds a page of one backend at a time; a backend that
 *          writes to a page of its own writes the changed page of the other one back first.
 */
class FlashPageCache
{
public:
    FlashPageCache() = default;
    FlashPageCache(const FlashPageCache&) = delete;
    FlashPageCache& operator=(const FlashPageCache&) = delete;

private:
    friend class UserEepromFlash;

    alignas(FLASH_RAM_BUFFER_ALIGNMENT) uint8_t data[FLASH_PAGE_SIZE] {}; //!< word aligned, otherwise iapProgram fails
    UserEepromFlash* owner = nullptr; //!< backend whose page is in @ref data, @c nullptr if none
    uint32_t page = 0;                //!< page number (offset / @ref FLASH_PAGE_SIZE) in the backend of @ref owner
    bool dirty = false;               //!< @ref data holds changes that aren't in the flash
};

/**
 * Non-volatile memory in the flash, read directly from the flash, written through a cache of one flash page
 * (@ref FlashPageCache).
 *
 * @details Saves the RAM mirror: instead of the whole user EEPROM only one page of @ref FLASH_PAGE_SIZE bytes is
 *          kept in RAM, shared with further backends of the same cache. Two layouts:
 *          - User EEPROM (@ref UserEepromFlash(uint32_t, FlashPageCache&)): same flash layout as
 *            @ref UserEepromMirror with a page size of a whole sector (MASK0701). The user EEPROM starts at the
 *            beginning of the last sector, it is valid if its last byte isn't 0xff. Both backends can replace each
 *            other without a new download. While the flash isn't valid, all bytes read as 0, like the mirror of an
 *            empty flash. The first write back to a not yet valid flash erases the sector and programs all pages
 *            (zeros apart from the cached page). The last byte marks the flash as valid and reads as 0 as soon as
 *            its page was written back (with the mirror after the next @ref flush).
 *          - Range of the flash (@ref UserEepromFlash(uint32_t, FlashPageCache&, uint32_t)): bytes at a fixed place
 *            in the flash, taken as they are, e.g. for a @ref UserMemoryExtension. No valid marker, erased bytes
 *            read as 0xff, every write back erases and programs only its page. A range of a @ref MemMapper is kept
 *            the same way, at the flash page of its allocation table entry (see the MultiSensorAktor).
 *
 *          Both layouts:
 *          - Reading: from the cache if the page is cached, otherwise directly from the flash.
 *          - Writing: into the cache. Writing to a page that isn't cached writes the cached page back first (erase
 *            page, program page) if it was changed, also if it belongs to another backend of the cache. Writing
 *            bytes that don't change anything doesn't mark the page as changed.
 *          - @ref flush writes the cached page back if it belongs to this backend.
 *
 * @warning A write back erases one flash page, with interrupts disabled. On the LPC11xx this takes about 100 ms.
 *          Outside of @ref flush the backend asks its @ref FlashGuard to pause the bus first.
 * @note A power loss while a page is written back loses this page. If it's the last page of a user EEPROM, the whole
 *       user EEPROM becomes invalid and reads as zeros (the mirror loses everything if the power fails while the
 *       sector is erased).
 */
class UserEepromFlash : public UserEepromBackend
{
public:
    /**
     * User EEPROM at the start of the last flash sector, in the layout of @ref UserEepromMirror.
     *
     * @param size  Size of the user EEPROM in bytes, at most @ref FLASH_SECTOR_SIZE
     * @param cache Write cache, may be shared with other backends; it has to live as long as the backend
     */
    UserEepromFlash(uint32_t size, FlashPageCache& cache);

    /**
     * Range of the flash, taken as it is (no valid marker, erased bytes read as 0xff).
     *
     * @param size        Size of the range in bytes
     * @param cache       Write cache, may be shared with other backends; it has to live as long as the backend
     * @param flashOffset Start of the range, offset from the start of the flash, a multiple of @ref FLASH_PAGE_SIZE.
     *                    Calls @ref fatalError() if it isn't, or if the range ends behind the flash.
     */
    UserEepromFlash(uint32_t size, FlashPageCache& cache, uint32_t flashOffset);

    void read(uint32_t offset, uint8_t* buffer, uint32_t length) const override;
    void write(uint32_t offset, const uint8_t* buffer, uint32_t length) override;
    void flush() override;
    void reload() override;

    /** @return Number of pages written back to the flash since the start, for measurements. */
    [[nodiscard]] uint32_t pageWrites() const { return pageWriteCount; }

private:
    /** @return Address in the flash of the byte at @p offset. */
    [[nodiscard]] uint8_t* flashAddress(uint32_t offset) const;

    /** @return true if page number @p page (offset / @ref FLASH_PAGE_SIZE) of this backend is in the cache. */
    [[nodiscard]] bool isCached(uint32_t page) const;

    /** Loads page number @p page into the cache. */
    void load(uint32_t page);

    /** Writes the cached page back to the flash, the cache has to hold a page of this backend. */
    void writeBack();

    /** Makes a not yet valid user EEPROM valid: all pages zero, apart from the cached one. */
    void format();

    /** Programs page number @p page from the cache, setting the valid marker if it's the last page of a user EEPROM. */
    void program(uint32_t page);

    FlashPageCache& cache;
    const uint32_t flashStart; //!< offset of the first byte from the start of the flash
    const bool marker;         //!< user EEPROM with a valid marker, otherwise a range taken as it is
    bool valid = false;        //!< the flash holds a user EEPROM (always true for a range), see @ref UserEepromFlash
    uint32_t pageWriteCount = 0;
};

#endif /* SBLIB_KNX_USEREEPROM_FLASH_H_ */
/** @}*/
