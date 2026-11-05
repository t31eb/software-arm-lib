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
 * @file   userEepromMirror.h
 * @author Selfbus
 * @bug No known bugs.
 ******************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#ifndef SBLIB_KNX_USEREEPROM_MIRROR_H_
#define SBLIB_KNX_USEREEPROM_MIRROR_H_

#include <sblib/eib/userEepromBackend.h>

/**
 * The whole user EEPROM as a mirror in RAM, persisted in the last flash sector.
 *
 * @details The sector is divided into pages of @ref flashSize bytes. Every @ref flush writes the mirror into the
 *          next page and erases the sector only if it wraps around. A page is in use if its last byte isn't 0xff,
 *          the content of the last page in use is loaded. If no page is in use, the mirror is filled with zeros.
 * @note This is the former implementation inside of @ref UserEeprom, unchanged.
 */
class UserEepromMirror : public UserEepromBackend
{
public:
    /**
     * Allocates the mirror and loads it from the flash.
     *
     * @param size      Size of the user EEPROM in bytes
     * @param flashSize Size of one page in the flash sector, at least @p size
     */
    UserEepromMirror(uint32_t size, uint32_t flashSize);

    void read(uint32_t offset, uint8_t* buffer, uint32_t length) const override;
    void write(uint32_t offset, const uint8_t* buffer, uint32_t length) override;

    /**
     * Writes the mirror into the next free page of the flash sector.
     * @warning While the flash is written, all interrupts are disabled.
     */
    void flush() override;
    void reload() override;
    [[nodiscard]] uint8_t* mirror() const override { return data; }

    [[nodiscard]] uint32_t flashSize() const;
    [[nodiscard]] uint32_t numEepromPages() const;
    [[nodiscard]] uint8_t* lastEepromPage() const;
    [[nodiscard]] uint8_t* flashSectorAddress() const;

    /**
     * Finds the last valid page in the flash sector.
     *
     * @details The search is done backwards from the end of the mcu flash in @ref flashSize steps.
     *
     * @return If successful: address of the last valid flash page, otherwise @c nullptr
     */
    [[nodiscard]] uint8_t* findValidPage() const;

private:
    uint8_t* const data; //!< the mirror, word aligned by @c new, otherwise iapProgram fails; never freed
    const uint32_t pageSize;
};

#endif /* SBLIB_KNX_USEREEPROM_MIRROR_H_ */
/** @}*/
