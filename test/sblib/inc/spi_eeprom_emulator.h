/*
 * Emulation of a serial EEPROM or FRAM of the 25xx family on the bus of the SpiEeprom driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#ifndef TEST_SBLIB_SPI_EEPROM_EMULATOR_H_
#define TEST_SBLIB_SPI_EEPROM_EMULATOR_H_

#include <sblib/spi_eeprom.h>

#include <cstdint>
#include <vector>

/**
 * A memory chip with the behavior a driver has to cope with.
 *
 * @details - EEPROM: Data is collected in a page latch and written when chip select goes high. A write
 *            crossing a page boundary wraps around to the start of the page. During the write cycle
 *            the chip answers RDSR only.
 *          - FRAM: Every byte is written right after it is received, there is no write cycle.
 *          - Both ignore WRITE without a preceding WREN.
 *          - A power loss during a write cycle leaves the whole page with undefined content.
 *
 *          Time passes with the transferred bytes, 12 MHz by default. millis() of the test environment follows
 *          this time, because the driver only polls the chip while it waits.
 *
 *          Everything a correct driver never does is counted in @ref violations.
 */
class SpiEepromEmulator final : public SpiEepromBus
{
public:
    /**
     * @param type        Properties of the chip
     * @param writeTimeUs Duration of a write cycle in microseconds, 0 emulates a FRAM
     */
    SpiEepromEmulator(const SpiEepromType& type, uint32_t writeTimeUs);

    static SpiEepromEmulator m95512r() { return {SPI_EEPROM_M95512R, 5000}; }
    static SpiEepromEmulator mb85rs256b() { return {SPI_FRAM_MB85RS256B, 0}; }

    void begin() override {}
    void select() override;
    void deselect() override;
    void transfer(const uint8_t* txData, uint8_t* rxData, uint32_t length) override;

    /** Cuts the power right before bus byte number @p byteCount + 1, counted from now on. */
    void powerFailAfterBytes(uint64_t byteCount);

    /** Cuts the power now. The memory stays, everything else is lost. */
    void powerOff();

    /** Powers on after a power loss, the content of the memory stays. */
    void powerOn();

    [[nodiscard]] bool powered() const { return isPowered; }

    std::vector<uint8_t> memory;   //!< Content of the chip
    uint8_t blockProtect = 0;      //!< BP1, BP0 in the status register
    bool stuckInWriteCycle = false; //!< A write cycle never ends
    bool ignoreWriteEnable = false; //!< WREN has no effect, like an active write protect

    uint64_t violations = 0;   //!< Commands a correct driver doesn't send
    uint64_t busBytes = 0;     //!< All bytes on the bus
    uint64_t transactions = 0; //!< Number of times the chip was selected
    uint64_t writeCycles = 0;  //!< Number of started write cycles or finished FRAM writes
    uint64_t timeNs = 0;       //!< Emulated time

private:
    enum class Phase
    {
        command,
        address,
        data,
        ignore
    };

    [[nodiscard]] bool isFram() const { return writeTimeUs == 0; }
    [[nodiscard]] bool inWriteCycle() const { return writeCycleRunning; }
    [[nodiscard]] uint8_t status() const;
    uint8_t exchange(uint8_t received);
    void startCommand(uint8_t instruction);
    void checkRollover();
    void updateTime();
    void corruptPage();

    const SpiEepromType type;
    const uint32_t writeTimeUs;

    bool isPowered = true;
    bool selected = false;
    bool writeEnabled = false;
    bool writeCycleRunning = false;
    uint64_t writeCycleEndNs = 0;
    uint32_t writeCyclePage = 0;
    bool powerFailArmed = false;
    uint64_t powerFailAtBusByte = 0;
    uint32_t corruption = 0x13579BDF;

    Phase phase = Phase::command;
    uint8_t instruction = 0;
    uint32_t address = 0;
    uint32_t addressBytes = 0;
    uint32_t latchedBytes = 0;
    uint32_t pendingEnable = 0; //!< Instruction which takes effect when chip select goes high
    std::vector<uint8_t> pageLatch;
    std::vector<bool> pageLatchUsed;
};

#endif /* TEST_SBLIB_SPI_EEPROM_EMULATOR_H_ */
