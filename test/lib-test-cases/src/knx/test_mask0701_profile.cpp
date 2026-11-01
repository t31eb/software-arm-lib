/*
 * Access profile of a MASK0701 with a MultiSensorAktor like configuration
 *
 * Only built with the CMake option SBLIB_TEST_ACCESS_PROFILE. Prints a table, the numbers are
 * explained in the workspace document docs/zugriffsprofil.md.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include "mask0701_fixture.h"

#include <access_profile.h>
#include <sblib/timer.h>
#include <system_time_simulator.h>

#include <cstdio>
#include <functional>

using F = Mask0701Fixture;
using Data = std::vector<uint8_t>;

namespace
{
constexpr uint16_t ADDR_TABLE = 0x4000;
constexpr uint32_t SYSTEM_AREA = 0x4270;      //!< load states and table pointers, tables have to stay out of it
constexpr uint16_t ASSOC_TABLE = 0x4300;
constexpr uint16_t FLAGS_TABLE = F::FLAGS_TABLE;
constexpr uint16_t OBJECT_RAM = FLAGS_TABLE + 128; //!< same as HelperFunctions::setComObjPtr() of the MultiSensorAktor
constexpr uint8_t COM_CONFIG = COMCONF_COMM | COMCONF_READ | COMCONF_WRITE | COMCONF_TRANS | COMCONF_UPDATE;

constexpr double SPI_BYTE_US = 8.0 / 12.0;  //!< SPI 12 MHz
constexpr double SPI_TRANSACTION_BYTES = 3; //!< command and 16 bit address
constexpr double I2C_BYTE_US = 22.5;        //!< 9 bits at 400 kHz
constexpr double I2C_TRANSACTION_BYTES = 4; //!< device address, 16 bit address, device address after the restart
constexpr double I2C_CONDITIONS_US = 7.5;   //!< start, restart and stop condition

uint16_t groupAddress(const uint16_t index)
{
    return static_cast<uint16_t>(0x0800 + index);
}

/** The tables of @p count com objects, every com object has its own group address */
struct Tables
{
    uint16_t comTable;
    Data addr;
    Data assoc;
    Data com;

    explicit Tables(const uint16_t count)
    {
        addr = {static_cast<uint8_t>(count + 1), HIGH_BYTE(F::OWN_ADDRESS), lowByte(F::OWN_ADDRESS)};
        assoc = {static_cast<uint8_t>(count)};
        com = {static_cast<uint8_t>(count), HIGH_BYTE(FLAGS_TABLE), lowByte(FLAGS_TABLE)};
        for (uint16_t i = 0; i < count; i++)
        {
            const auto value = static_cast<uint16_t>(OBJECT_RAM + i);
            addr.push_back(HIGH_BYTE(groupAddress(i + 1)));
            addr.push_back(lowByte(groupAddress(i + 1)));
            assoc.push_back(static_cast<uint8_t>(i + 1));
            assoc.push_back(static_cast<uint8_t>(i));
            com.insert(com.end(), {HIGH_BYTE(value), lowByte(value), COM_CONFIG, BYTE_1});
        }
        comTable = static_cast<uint16_t>(ASSOC_TABLE + assoc.size());
    }
};

void download(F& f, const Tables& tables)
{
    const struct
    {
        uint8_t objectIdx;
        uint16_t address;
        const Data& table;
    } objects[] = {
        {OT_ADDR_TABLE, ADDR_TABLE, tables.addr},
        {OT_ASSOC_TABLE, ASSOC_TABLE, tables.assoc},
        {OT_APPLICATION, tables.comTable, tables.com},
    };

    for (const auto& [objectIdx, address, table] : objects)
    {
        f.loadControlMem(objectIdx, LC_START_LOADING);
        f.allocTaskSegmentMem(objectIdx, address);
        f.memoryWriteChunked(address, table);
        f.loadControlMem(objectIdx, LC_LOAD_COMPLETED);
    }
}

/** Watches the mirror split into its parts, the emulated flash and the user RAM */
struct Watch
{
    size_t system, addrTable, loadAndPointers, assocTable, comTable, flash, ram;

    Watch(const F& f, const uint16_t comTableAddress)
    {
        uint8_t* mirror = f.bcu->userEeprom->userEepromData;
        const auto part = [mirror](const uint32_t first, const uint32_t end) {
            return accessProfile::addRegion(mirror + first - F::USER_EEPROM_FIRST, end - first, true);
        };

        accessProfile::clearRegions();
        system = part(F::USER_EEPROM_FIRST, ADDR_TABLE);
        addrTable = part(ADDR_TABLE, SYSTEM_AREA);
        loadAndPointers = part(SYSTEM_AREA, ASSOC_TABLE);
        assocTable = part(ASSOC_TABLE, comTableAddress);
        comTable = part(comTableAddress, F::USER_EEPROM_LAST + 1);
        flash = accessProfile::addRegion(FLASH, FLASH_SIZE, true);
        ram = accessProfile::addRegion(f.bcu->userRam->userRamData, F::RAM_END - F::RAM_START + 1, false);
    }
};

void printHeader()
{
    printf("%-34s %4s %9s %9s %9s %7s | %7s %7s %7s | %9s %9s %10s %10s | %8s %8s %8s %8s %8s %8s | %8s\n", "case", "KOs",
           "reads", "rdBytes", "rdBursts", "unique", "writes", "wrBytes", "wrBurst", "spiAcc_us", "spiBur_us", "i2cAcc_us",
           "i2cBur_us", "system", "addr", "load", "assoc", "com", "flash", "ramRd");
}

void measure(const char* name, const uint16_t objects, const Watch& watch, const int repeat, const std::function<void()>& action)
{
    accessProfile::start();
    for (int i = 0; i < repeat; i++)
    {
        action();
    }
    accessProfile::stop();

    const accessProfile::Counters total = accessProfile::persistentTotal();
    const double n = repeat;
    const double reads = total.reads / n;
    const double bursts = total.readBursts / n;
    const double bytes = total.readBytes / n;

    const auto spi = [bytes](const double transactions) {
        return (transactions * SPI_TRANSACTION_BYTES + bytes) * SPI_BYTE_US;
    };
    const auto i2c = [bytes](const double transactions) {
        return transactions * (I2C_TRANSACTION_BYTES * I2C_BYTE_US + I2C_CONDITIONS_US) + bytes * I2C_BYTE_US;
    };
    const auto regionBytes = [n](const size_t index) { return accessProfile::region(index).readBytes / n; };

    printf("%-34s %4u %9.1f %9.1f %9.1f %7llu | %7.1f %7.1f %7.1f | %9.1f %9.1f %10.1f %10.1f | %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f | %8.1f\n",
           name, objects, reads, bytes, bursts, static_cast<unsigned long long>(total.uniqueBytes), total.writes / n,
           total.writeBytes / n, total.writeBursts / n, spi(reads), spi(bursts), i2c(reads), i2c(bursts),
           regionBytes(watch.system), regionBytes(watch.addrTable), regionBytes(watch.loadAndPointers),
           regionBytes(watch.assocTable), regionBytes(watch.comTable), regionBytes(watch.flash), regionBytes(watch.ram));
}

void profile(const uint16_t objects)
{
    F f;
    const Tables tables(objects);
    download(f, tables);
    REQUIRE(f.bcu->flushUserMemory(UsrCallbackType::flash));
    f.bcu->userEeprom->modified(false);
    REQUIRE(f.bcu->applicationRunning());

    // move the bus out of its INIT state, so telegrams can be sent
    _LPC_TMR16B1.IR = 4;
    f.bcu->bus->timerInterruptHandler();
    _LPC_TMR16B1.IR = 4;
    f.bcu->bus->timerInterruptHandler();

    const Watch watch(f, tables.comTable);
    ComObjects* com = f.bcu->comObjects;
    BcuDefault* bcu = f.bcu;
    const auto last = static_cast<uint16_t>(objects - 1);

    setMillis(1000);
    bcu->groupTelSent = millis();
    measure("loop, no send slot", objects, watch, 100, [bcu] { bcu->loop(); });

    com->sendNextObjIndex = 0;
    bcu->groupTelSent = millis() - 40;
    measure("loop, send slot, scan from KO 0", objects, watch, 1, [bcu] { bcu->loop(); });

    com->sendNextObjIndex = 0;
    measure("loop, send slot, average", objects, watch, objects, [bcu] {
        bcu->groupTelSent = millis() - 40;
        bcu->loop();
    });

    measure("nextUpdatedObject, nothing", objects, watch, 100, [com] { com->nextUpdatedObject(); });
    measure("objectRead", objects, watch, 100, [com, last] { com->objectRead(last); });

    uint8_t telegram[] = {0xBC, 0x11, 0x01, 0x08, 0x02, 0xE2, 0x00, 0x80, 0x42};
    measure("group write, first GA", objects, watch, 1, [bcu, &telegram] {
        bcu->processGroupAddressTelegram(APCI_GROUP_VALUE_WRITE_PDU, groupAddress(1), telegram, sizeof(telegram));
    });
    measure("group write, last GA", objects, watch, 1, [bcu, &telegram, objects] {
        bcu->processGroupAddressTelegram(APCI_GROUP_VALUE_WRITE_PDU, groupAddress(objects), telegram, sizeof(telegram));
    });
    measure("group write, unknown GA", objects, watch, 1, [bcu, &telegram] {
        bcu->processGroupAddressTelegram(APCI_GROUP_VALUE_WRITE_PDU, 0x7FFF, telegram, sizeof(telegram));
    });
    while (com->nextUpdatedObject() != INVALID_OBJECT_NUMBER)
    {
    }

    measure("property read", objects, watch, 1, [&f] { f.propertyRead(OT_ADDR_TABLE, PID_TABLE_REFERENCE); });
    measure("memory read 12 B EEPROM", objects, watch, 1, [&f] { f.memoryRead(ADDR_TABLE, 12); });
    measure("memory read 12 B MemMapper", objects, watch, 1, [&f] { f.memoryRead(F::MAPPER_START, 12); });
    measure("memory write 12 B EEPROM", objects, watch, 1, [&f] { f.memoryWrite(ADDR_TABLE + 0x100, Data(12, 0)); });
    measure("download of all tables", objects, watch, 1, [&f, &tables] { download(f, tables); });

    // sending last, it leaves a telegram in the send buffer
    f.bcu->userEeprom->modified(false);
    bcu->groupTelSent = millis();
    com->objectWrite(last, 1);
    com->sendNextObjIndex = 0;
    measure("send last KO", objects, watch, 1, [com] { com->sendNextGroupTelegram(); });

    accessProfile::clearRegions();
}
}

TEST_CASE("Access profile of a MultiSensorAktor like configuration", "[poc][profile]")
{
    setMillis(0);
    printHeader();
    profile(60);
    profile(240);
}
