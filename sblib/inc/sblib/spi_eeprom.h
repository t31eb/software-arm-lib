/*
 *  spi_eeprom.h - Driver for serial EEPROM and FRAM of the 25xx family.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */
#ifndef SBLIB_SPI_EEPROM_H_
#define SBLIB_SPI_EEPROM_H_

#include <sblib/spi.h>

#include <cstdint>

/**
 * The connection between @ref SpiEeprom and the memory chip.
 *
 * @details Keeps the driver independent of the hardware, the unit tests connect an emulated chip here.
 */
class SpiEepromBus
{
public:
    virtual ~SpiEepromBus() = default;

    /** Prepares the bus, the chip is deselected afterwards. */
    virtual void begin() = 0;

    /** Activates chip select. */
    virtual void select() = 0;

    /** Deactivates chip select, all bytes of @ref transfer have left the bus at this point. */
    virtual void deselect() = 0;

    /**
     * Exchanges bytes with the chip. Returns after all bytes are transferred.
     *
     * @param txData Bytes to send, nullptr sends 0xFF
     * @param rxData Receives the bytes, nullptr discards them
     * @param length Number of bytes
     */
    virtual void transfer(const uint8_t* txData, uint8_t* rxData, uint32_t length) = 0;
};

/**
 * @ref SpiEepromBus on a SSP port of the LPC11xx in SPI mode 0, with 12 MHz at a 48 MHz system clock.
 *
 * @details Chip select is a GPIO. The SSEL function of the port would go high between two bytes,
 *          but a command of the memory chip has to stay selected until its last byte.
 */
class SpiEepromBusSsp final : public SpiEepromBus, private SPI
{
public:
    /**
     * @param spiPort   SPI_PORT_0 or SPI_PORT_1
     * @param pinClock  Pin for SCK
     * @param pinMiso   Pin for MISO
     * @param pinMosi   Pin for MOSI
     * @param pinSelect Pin for chip select, used as GPIO
     */
    SpiEepromBusSsp(int spiPort, uint32_t pinClock, uint32_t pinMiso, uint32_t pinMosi, uint32_t pinSelect);

    void begin() override;
    void select() override;
    void deselect() override;
    void transfer(const uint8_t* txData, uint8_t* rxData, uint32_t length) override;

private:
    uint32_t pinClock;
    uint32_t pinMiso;
    uint32_t pinMosi;
    uint32_t pinSelect;
};

/**
 * The properties of a memory chip.
 */
struct SpiEepromType
{
    uint32_t size;           //!< Size in bytes
    uint16_t pageSize;       //!< Size of a write page in bytes, 0 if the chip has no pages
    uint16_t writeTimeoutMs; //!< Longest time to wait for the end of a write cycle
};

constexpr SpiEepromType SPI_EEPROM_M95512R = {0x10000, 128, 10};  //!< ST M95512-R, 64 KB, write cycle 5 ms
constexpr SpiEepromType SPI_EEPROM_25LC512 = {0x10000, 128, 10};  //!< Microchip 25LC512, 64 KB, write cycle 5 ms
constexpr SpiEepromType SPI_FRAM_MB85RS256B = {0x8000, 0, 1};     //!< Fujitsu MB85RS256B, 32 KB, no write cycle

/**
 * Driver for memory chips with the commands WREN, RDSR, READ, WRITE and a 16 bit address.
 *
 * @details Addresses are physical addresses of the chip. Mapping them to virtual addresses is up to
 *          the caller.
 *
 *          A write returns after the last byte is on its way into the chip, the write cycle still runs.
 *          The next command waits for it, so the order of writes is kept. To be sure data
 *          has arrived, e.g. before writing a load state, call @ref waitReady.
 *
 * @warning Waiting for a write cycle blocks. Use the driver from the main loop only, not from an interrupt.
 */
class SpiEeprom
{
public:
    SpiEeprom() = delete;

    /**
     * @param bus  The connection to the chip
     * @param type The properties of the chip, e.g. @ref SPI_EEPROM_M95512R
     */
    SpiEeprom(SpiEepromBus& bus, const SpiEepromType& type);

    /**
     * Prepares the bus and checks the chip. Call this in the application setup, not in a constructor of a global object.
     *
     * @return True if a chip answers and accepts writes. False if there is no chip, it is busy
     *         for too long or a part of it is write protected by its block protect bits.
     */
    [[nodiscard]] bool begin();

    /**
     * Reads a block of bytes.
     *
     * @param address Address in the chip
     * @param buffer  Receives @p length bytes
     * @param length  Number of bytes
     * @return        True if successful, false if the block doesn't fit into the chip or the chip stays busy.
     *                A @p length of 0 does nothing and is successful.
     */
    [[nodiscard]] bool read(uint32_t address, uint8_t* buffer, uint32_t length);

    /**
     * Writes a block of bytes, page boundaries are handled here.
     *
     * @param address Address in the chip
     * @param buffer  Provides @p length bytes
     * @param length  Number of bytes
     * @return        True if successful, false if the block doesn't fit into the chip, the chip stays
     *                busy or doesn't accept the write. Then a part of the block may be written.
     *                A @p length of 0 does nothing and is successful.
     */
    [[nodiscard]] bool write(uint32_t address, const uint8_t* buffer, uint32_t length);

    /**
     * Waits for the end of a running write cycle.
     *
     * @return True if the chip is ready, false if it is still busy after the write timeout of its type.
     */
    [[nodiscard]] bool waitReady();

    /** @return Size of the chip in bytes */
    [[nodiscard]] uint32_t size() const;

private:
    [[nodiscard]] uint8_t readStatus();
    [[nodiscard]] bool blockFits(uint32_t address, uint32_t length) const;
    [[nodiscard]] bool writeChunk(uint32_t address, const uint8_t* buffer, uint32_t length);
    void command(uint8_t instruction);
    void startAccess(uint8_t instruction, uint32_t address);

    SpiEepromBus& bus;
    const SpiEepromType type;
    bool writeCycleRunning = false; //!< True if the chip may still be in a write cycle
};

#endif /* SBLIB_SPI_EEPROM_H_ */
