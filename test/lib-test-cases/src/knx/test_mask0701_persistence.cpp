/*
 * Characterization tests of the load state machine and the persistence of a MASK0701
 *
 * These tests document the current behavior, even where it looks wrong.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include "mask0701_fixture.h"

#include <sblib/eib/knx_lpdu.h>
#include <sblib/timer.h>
#include <system_time_simulator.h>

#include <cstring>

using F = Mask0701Fixture;
using Data = std::vector<uint8_t>;

namespace
{
/** Payload of a DMP_LoadStateMachineWrite_RCo_IO */
Data loadControlIo(const uint8_t control)
{
    Data data(DMP_LOADSTATE_MACHINE_WRITE_RCO_IO_LENGTH, 0);
    data[0] = control;
    return data;
}

void flush(const F& f)
{
    REQUIRE(f.bcu->flushUserMemory(UsrCallbackType::flash));
}
}

TEST_CASE("MASK0701 load state machine", "[poc][loadstate]")
{
    F f;
    auto loadState = [&f](const uint8_t objectIdx) { return f.bcu->userEeprom->loadState(objectIdx); };

    SECTION("empty device is unloaded and the application doesn't run")
    {
        for (int i = 0; i < INTERFACE_OBJECT_COUNT; i++)
        {
            REQUIRE(loadState(i) == LS_UNLOADED);
        }
        REQUIRE_FALSE(f.bcu->applicationRunning());
    }

    SECTION("download with memory writes")
    {
        f.loadControlMem(OT_ADDR_TABLE, LC_START_LOADING);
        REQUIRE(loadState(OT_ADDR_TABLE) == LS_LOADING);
        f.allocTaskSegmentMem(OT_ADDR_TABLE, F::ADDR_TABLE);
        REQUIRE(loadState(OT_ADDR_TABLE) == LS_LOADING);
        REQUIRE(f.bcu->userEeprom->addrTabAddr() == F::ADDR_TABLE);
        f.loadControlMem(OT_ADDR_TABLE, LC_LOAD_COMPLETED);
        REQUIRE(loadState(OT_ADDR_TABLE) == LS_LOADED);
        f.loadControlMem(OT_ADDR_TABLE, LC_UNLOAD);
        REQUIRE(loadState(OT_ADDR_TABLE) == LS_UNLOADED);
    }

    SECTION("after a complete download the application runs")
    {
        f.downloadTables();
        REQUIRE(loadState(OT_ADDR_TABLE) == LS_LOADED);
        REQUIRE(loadState(OT_ASSOC_TABLE) == LS_LOADED);
        REQUIRE(loadState(OT_APPLICATION) == LS_LOADED);
        REQUIRE(f.bcu->applicationRunning());
    }

    SECTION("property write and memory write set the same byte")
    {
        const auto result = f.propertyWrite(OT_APPLICATION, PID_LOAD_STATE_CONTROL, loadControlIo(LC_START_LOADING));
        REQUIRE(result.data == Data {LS_LOADING});
        REQUIRE(loadState(OT_APPLICATION) == LS_LOADING);
        REQUIRE(f.memoryRead(F::LOAD_STATE + OT_APPLICATION, 1).data == Data {LS_LOADING});
        REQUIRE(f.propertyRead(OT_APPLICATION, PID_LOAD_STATE_CONTROL).data == Data {LS_LOADING});
    }

    SECTION("load controls with a wrong length end in the error state")
    {
        f.memoryWrite(0x104, {static_cast<uint8_t>((OT_ADDR_TABLE << 4) | LC_START_LOADING)});
        REQUIRE(loadState(OT_ADDR_TABLE) == LS_ERROR);
    }

    SECTION("memory write and property write of a load control both set modified")
    {
        f.bcu->userEeprom->modified(false);
        f.loadControlMem(OT_ADDR_TABLE, LC_LOAD_COMPLETED);
        REQUIRE(f.bcu->userEeprom->isModified());

        f.bcu->userEeprom->modified(false);
        f.propertyWrite(OT_ASSOC_TABLE, PID_LOAD_STATE_CONTROL, loadControlIo(LC_LOAD_COMPLETED));
        REQUIRE(loadState(OT_ASSOC_TABLE) == LS_LOADED);
        REQUIRE(f.bcu->userEeprom->isModified());
    }
}

TEST_CASE("MASK0701 persistence", "[poc][loadstate]")
{
    F f;

    SECTION("first start on empty flash: default address and marked as modified, once")
    {
        REQUIRE(f.bcu->ownAddress() == PHY_ADDR_DEFAULT);
        REQUIRE(f.bcu->userEeprom->isModified());

        flush(f);
        f.restart();
        REQUIRE(f.bcu->ownAddress() == PHY_ADDR_DEFAULT);
        REQUIRE_FALSE(f.bcu->userEeprom->isModified());
    }

    SECTION("a download survives a restart")
    {
        f.downloadTables();
        f.memoryWrite(F::MAPPER_START, {0x11, 0x22});
        flush(f);
        f.restart();

        REQUIRE(f.bcu->applicationRunning());
        REQUIRE(f.bcu->addrTables->indexOfAddr(F::GROUP_ADDRESS_2) == 2);
        const Data addressTable = F::addressTableBytes();
        REQUIRE(f.memoryRead(F::ADDR_TABLE, 7).data == Data(addressTable.begin(), addressTable.begin() + 7));
        REQUIRE(f.memoryRead(F::MAPPER_START, 2).data == Data {0x11, 0x22});
    }

    SECTION("no flush, no persistence")
    {
        f.downloadTables();
        f.restart();
        if (F::spiBackend())
        {
            // Every write reaches the chip right away, the download is complete
            REQUIRE(f.bcu->applicationRunning());
            return;
        }
        REQUIRE_FALSE(f.bcu->applicationRunning());
        if (F::flashBackend())
        {
            // Pages written back on a change of the page stay, only the cached page with the load states is lost
            REQUIRE(f.memoryRead(F::ADDR_TABLE, 1).data == Data {F::addressTableBytes()[0]});
        }
        else
        {
            REQUIRE(f.memoryRead(F::ADDR_TABLE, 1).data == Data {0});
        }
    }

    SECTION("a load state written over a property survives a restart")
    {
        flush(f);
        f.propertyWrite(OT_ADDR_TABLE, PID_LOAD_STATE_CONTROL, loadControlIo(LC_LOAD_COMPLETED));
        flush(f);
        f.restart();
        REQUIRE(f.bcu->userEeprom->loadState(OT_ADDR_TABLE) == LS_LOADED);
    }

    SECTION("begin() changes the mirror without marking it as modified")
    {
        f.memoryWrite(F::USER_EEPROM_FIRST + 0x0D, {0x00}); // runError
        f.memoryWrite(F::USER_EEPROM_FIRST + 0x0C, {0x55}); // portADDR
        f.memoryWrite(F::USER_EEPROM_FIRST + 0x03, {0x12, 0x34, 0x56, 0x78, 0x9A}); // manufacturer, device type
        flush(f);
        REQUIRE(F::flashByte(F::USER_EEPROM_FIRST + 0x0D) == 0x00);

        f.restart();

        REQUIRE(f.mirrorByte(F::USER_EEPROM_FIRST + 0x0D) == 0xFF);
        REQUIRE(f.mirrorByte(F::USER_EEPROM_FIRST + 0x0C) == 0x00);
        REQUIRE(f.mirrorByte(F::USER_EEPROM_FIRST + 0x03) == HIGH_BYTE(F::MANUFACTURER));
        REQUIRE(f.mirrorByte(F::USER_EEPROM_FIRST + 0x04) == lowByte(F::MANUFACTURER));
        REQUIRE(f.mirrorByte(F::USER_EEPROM_FIRST + 0x07) == F::VERSION);
        REQUIRE_FALSE(f.bcu->userEeprom->isModified());
        if (F::flashBackend() || F::spiBackend())
        {
            // flash: BCU2::begin() writes the serial number into another page afterwards, that writes this page back;
            // SPI: every write reaches the chip right away
            REQUIRE(F::flashByte(F::USER_EEPROM_FIRST + 0x0D) == 0xFF);
        }
        else
        {
            REQUIRE(F::flashByte(F::USER_EEPROM_FIRST + 0x0D) == 0x00);
        }
    }

    SECTION("the load states of the user EEPROM are written after the parameters of the MemMapper")
    {
        constexpr unsigned int USER_EEPROM_FLASH = 0xF000;

        f.downloadTables();
        f.memoryWrite(F::MAPPER_START, {0x11, 0x22});
        iap_program_log_count = 0;
        flush(f);

        if (F::spiBackend())
        {
            // Nothing in the flash: user EEPROM and the former MemMapper range are in the chip, written in the order
            // of the telegrams
            REQUIRE(iap_program_log_count == 0);
            const uint32_t data = F::SPI_EXTENSION + UserEepromSpi::HEADER_SIZE;
            REQUIRE(F::spiChip().memory[data] == 0x11);
            REQUIRE(F::spiChip().memory[data + 1] == 0x22);
            return;
        }
        // the mirror writes 3 chunks of 1024 bytes, the flash backend the page with the load states
        const unsigned int userEepromWrites = F::flashBackend() ? 1 : 3;
        REQUIRE(iap_program_log_count > userEepromWrites);
        REQUIRE(iap_program_log[0] < USER_EEPROM_FLASH);
        REQUIRE(iap_program_log[iap_program_log_count - 1] >= USER_EEPROM_FLASH);
        for (unsigned int i = 0; i < iap_program_log_count - userEepromWrites; i++)
        {
            REQUIRE(iap_program_log[i] < USER_EEPROM_FLASH); // the user EEPROM takes the last writes
        }
    }

    SECTION("the last byte is the page marker, it is always cleared")
    {
        f.memoryWrite(F::USER_EEPROM_LAST, {0x5A});
        REQUIRE(f.mirrorByte(F::USER_EEPROM_LAST) == 0x5A);
        flush(f);
        const uint8_t expected = F::spiBackend() ? 0x5A : 0x00; // the SPI chip has a header instead of a marker
        REQUIRE(f.mirrorByte(F::USER_EEPROM_LAST) == expected);
        REQUIRE(F::flashByte(F::USER_EEPROM_LAST) == expected);
    }

    SECTION("nothing is written without modifications, every flush with modifications erases the sector")
    {
        flush(f);
        const int erases = iap_calls[I_ERASE];
        const int programs = iap_calls[I_RAM2FLASH];

        flush(f);
        REQUIRE(iap_calls[I_ERASE] == erases);
        REQUIRE(iap_calls[I_RAM2FLASH] == programs);

        const auto cycles = F::spiChip().writeCycles;
        f.memoryWrite(F::USER_EEPROM_FIRST + 0x20, {0x01});
        flush(f);
        if (F::spiBackend())
        {
            REQUIRE(iap_calls[I_RAM2FLASH] == programs);
            REQUIRE(F::spiChip().writeCycles == cycles + 1);
        }
        else if (F::flashBackend())
        {
            REQUIRE(iap_calls[I_ERASE] == erases); // erases only the page, the emulation doesn't count that
            REQUIRE(iap_calls[I_RAM2FLASH] == programs + 1);
        }
        else
        {
            REQUIRE(iap_calls[I_ERASE] == erases + 1);
            REQUIRE(iap_calls[I_RAM2FLASH] == programs + 3); // 3072 bytes in chunks of 1024 bytes
        }
    }

    SECTION("loop() flushes only without a transport layer connection")
    {
        flush(f);
        const int programs = iap_calls[I_RAM2FLASH];
        f.memoryWrite(F::USER_EEPROM_FIRST + 0x20, {0x01});

        f.bcu->state = TLayer4::OPEN_IDLE;
        f.bcu->loop();
        REQUIRE(iap_calls[I_RAM2FLASH] == programs);
        REQUIRE(f.bcu->userEeprom->isModified());

        f.bcu->state = TLayer4::CLOSED;
        setMillis(millis() + 51); // writeDelayElapsed()
        if (F::spiBackend())
        {
            F::spiChip().timeNs += 51'000'000ull; // the emulated chip sets millis() to its own clock on every access
        }
        f.bcu->loop();
        if (!F::spiBackend()) // the SPI chip has the byte already, flush only waits for the write cycle
        {
            REQUIRE(iap_calls[I_RAM2FLASH] > programs);
        }
        REQUIRE_FALSE(f.bcu->userEeprom->isModified());
    }
}

TEST_CASE("MASK0701 writes the MemMapper before a load state changes", "[poc][loadstate]")
{
    // A backend may persist the load state right away (SPI, a page of the flash backend); a power loss after it
    // must not leave a loaded state with parameters that are still in the RAM buffer of the MemMapper.
    if (F::spiBackend())
    {
        return; // no MemMapper, the extension in the chip is written right away
    }
    F f;
    flush(f);
    f.loadControlMem(OT_APPLICATION, LC_START_LOADING);
    f.memoryWrite(F::MAPPER_START, {0x11, 0x22});
    REQUIRE(f.memMapper->isModified());

    iap_program_log_count = 0;
    f.loadControlMem(OT_APPLICATION, LC_LOAD_COMPLETED);
    REQUIRE_FALSE(f.memMapper->isModified());
    REQUIRE(iap_program_log_count >= 1);
    REQUIRE(f.bcu->userEeprom->loadState(OT_APPLICATION) == LS_LOADED);

    // Without changes in the MemMapper nothing is written
    iap_program_log_count = 0;
    f.loadControlMem(OT_APPLICATION, LC_LOAD_COMPLETED);
    REQUIRE(iap_program_log_count == 0);
}

TEST_CASE("UserEeprom::writeDelayElapsed() waits 50ms after a modification", "[poc][userEeprom]")
{
    F f;
    UserEeprom* eeprom = f.bcu->userEeprom;
    setMillis(1000);

    SECTION("modified: elapsed after the delay")
    {
        eeprom->modified(true);
        REQUIRE(eeprom->writeUserEepromTime == 1050);
        REQUIRE_FALSE(eeprom->writeDelayElapsed());
        setMillis(1049);
        REQUIRE_FALSE(eeprom->writeDelayElapsed());
        setMillis(1051);
        REQUIRE(eeprom->writeDelayElapsed());
    }

    SECTION("not modified: never elapsed")
    {
        eeprom->modified(false);
        REQUIRE_FALSE(eeprom->writeDelayElapsed());
    }
}
