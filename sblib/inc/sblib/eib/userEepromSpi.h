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
 * @file   userEepromSpi.h
 * @author Selfbus
 * @bug No known bugs.
 ******************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#ifndef SBLIB_KNX_USEREEPROM_SPI_H_
#define SBLIB_KNX_USEREEPROM_SPI_H_

#include <sblib/eib/userEepromBackend.h>
#include <sblib/spi_eeprom.h>

/**
 * The user EEPROM in an external serial EEPROM or FRAM of the 25xx family (@ref SpiEeprom).
 *
 * @details No RAM mirror and no cache: every write goes directly to the chip, in the order of the calls. So the
 *          load states, which ETS writes last, also reach the chip last. Bytes that don't change aren't written.
 *
 *          Layout in the chip, starting at @p chipAddress:
 *          @code
 *            +0   header: "SBUE", layout version, 0, size (16 bit, low byte first), CRC-16/CCITT of these 8 bytes
 *            +128 the user EEPROM, @ref size bytes
 *          @endcode
 *          The header has a write page of its own (128 bytes on the M95512-R): a power loss in a write cycle destroys
 *          the whole page, but data writes never touch the header page.
 *          The CRC only covers the header. It tells a used chip from an empty or foreign one, which reads as zeros
 *          like an empty flash. The first write formats it: zeros first, the header last. A CRC over the data would
 *          have to follow every write, and a power loss in between would invalidate the whole configuration.
 *
 * @note The chip is accessed only after @ref begin (called by @ref BcuDefault::begin before the bus starts).
 *       Reads before read zeros and are counted in @ref earlyReads.
 * @note If the chip doesn't answer or a write fails, @ref failed is set: from then on reads return zeros and writes
 *       are dropped. ETS notices this when it reads back.
 */
class UserEepromSpi : public UserEepromBackend
{
public:
    /**
     * @param eeprom      The chip, @ref begin of the backend calls its begin()
     * @param size        Size of the user EEPROM in bytes
     * @param chipAddress Address in the chip of the header, the user EEPROM follows @ref HEADER_SIZE bytes later
     */
    UserEepromSpi(SpiEeprom& eeprom, uint32_t size, uint32_t chipAddress = 0);

    /** Starts the chip and checks the header. */
    void begin() override;
    void read(uint32_t offset, uint8_t* buffer, uint32_t length) const override;
    void write(uint32_t offset, const uint8_t* buffer, uint32_t length) override;

    /** Waits until the last write cycle of the chip has finished. */
    void flush() override;

    /** Checks the header again. There is nothing cached, so there is nothing to discard. */
    void reload() override;

    /** An SPI transfer in an interrupt would break one of the main program, see @ref AddrTablesBCU2::enableGroupAddressCache */
    [[nodiscard]] bool readableInInterrupt() const override { return false; }

    /** @return true if the chip didn't answer in @ref begin or a write failed */
    [[nodiscard]] bool failed() const { return chipFailed; }

    /** @return true if the chip holds a user EEPROM with a valid header */
    [[nodiscard]] bool formatted() const { return valid; }

    /** @return Number of reads before @ref begin, they returned zeros */
    [[nodiscard]] uint32_t earlyReads() const { return earlyReadCount; }

    static constexpr uint32_t HEADER_SIZE = 128; //!< bytes reserved for the header in front of the data, one write page
    static constexpr uint8_t LAYOUT_VERSION = 2; //!< version of the layout in the chip (1: data at 16, never released)

private:
    /** The 8 bytes of the header with its CRC, 10 bytes in total */
    void makeHeader(uint8_t* header) const;

    /** Writes zeros into the whole user EEPROM, then the header. */
    void format();

    /** Writes @p length bytes at @p chipOffset (relative to the data) if they differ from the chip. */
    void writeChanged(uint32_t chipOffset, const uint8_t* buffer, uint32_t length);

    SpiEeprom& eeprom;
    const uint32_t chipAddress;
    bool started = false;
    bool valid = false;
    bool chipFailed = false;
    mutable uint32_t earlyReadCount = 0;
};

#endif /* SBLIB_KNX_USEREEPROM_SPI_H_ */
/** @}*/
