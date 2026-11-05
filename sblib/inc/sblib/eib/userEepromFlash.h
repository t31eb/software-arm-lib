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

/**
 * The user EEPROM in the last flash sector, read directly from the flash, written through a cache of one flash page.
 *
 * @details Saves the RAM mirror: instead of the whole user EEPROM only one page of @ref FLASH_PAGE_SIZE bytes is
 *          kept in RAM. Same flash layout as @ref UserEepromMirror with a page size of a whole sector (MASK0701):
 *          the user EEPROM starts at the beginning of the last sector, it is valid if its last byte isn't 0xff.
 *          Both backends can replace each other without a new download.
 *
 *          - Reading: from the cache if the page is cached, otherwise directly from the flash. While the flash
 *            isn't valid, all bytes read as 0, like the mirror of an empty flash.
 *          - Writing: into the cache. Writing to another page writes the cached page back first (erase page,
 *            program page) if it was changed. Writing bytes that don't change anything doesn't mark the page as
 *            changed.
 *          - @ref flush writes the cached page back. The first write back to a not yet valid flash erases the sector
 *            and programs all pages (zeros apart from the cached page).
 *          - The last byte marks the flash as valid and reads as 0 as soon as its page was written back (with the
 *            mirror after the next @ref flush).
 *
 * @warning A write back erases one flash page, with interrupts disabled. On the LPC11xx this takes about 100 ms.
 *          Outside of @ref flush the backend asks its @ref FlashGuard to pause the bus first.
 * @note A power loss while a page is written back loses this page. If it's the last page, the whole user EEPROM
 *       becomes invalid and reads as zeros (the mirror loses everything if the power fails while the sector is
 *       erased).
 */
class UserEepromFlash : public UserEepromBackend
{
public:
    /** @param size Size of the user EEPROM in bytes, at most @ref FLASH_SECTOR_SIZE */
    explicit UserEepromFlash(uint32_t size);

    void read(uint32_t offset, uint8_t* buffer, uint32_t length) const override;
    void write(uint32_t offset, const uint8_t* buffer, uint32_t length) override;
    void flush() override;
    void reload() override;

    /** @return Number of pages written back to the flash since the start, for measurements. */
    [[nodiscard]] uint32_t pageWrites() const { return pageWriteCount; }

private:
    static constexpr uint32_t NO_PAGE = 0xffffffff;

    /** @return Address in the flash of the user EEPROM byte at @p offset. */
    [[nodiscard]] uint8_t* flashAddress(uint32_t offset) const;

    /** Loads page number @p page (offset / @ref FLASH_PAGE_SIZE) into the cache. */
    void load(uint32_t page);

    /** Writes the cached page back to the flash. */
    void writeBack();

    /** Makes a not yet valid flash valid: all pages zero, apart from the cached one. */
    void format();

    /** Programs page number @p page from the cache, setting the valid marker if it's the last page. */
    void program(uint32_t page);

    alignas(FLASH_RAM_BUFFER_ALIGNMENT) uint8_t cache[FLASH_PAGE_SIZE]; //!< word aligned, otherwise iapProgram fails
    uint32_t cachedPage = NO_PAGE; //!< page number in @ref cache, @ref NO_PAGE if none
    bool dirty = false;            //!< @ref cache holds changes that aren't in the flash
    bool valid = false;            //!< the flash holds a user EEPROM, see @ref UserEepromFlash
    uint32_t pageWriteCount = 0;
};

#endif /* SBLIB_KNX_USEREEPROM_FLASH_H_ */
/** @}*/
