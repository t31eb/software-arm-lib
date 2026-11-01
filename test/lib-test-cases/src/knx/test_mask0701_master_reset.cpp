/*
 * Characterization tests of A_Restart with master reset on a MASK0701
 *
 * These tests document the current behavior, even where it looks wrong.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include "mask0701_fixture.h"

#include <sblib/eib/apci.h>
#include <sblib/internal/bootloader_commands.h>

using F = Mask0701Fixture;
using Data = std::vector<uint8_t>;

namespace
{
struct ResetResponse
{
    uint8_t errorCode;
    uint8_t seconds;
};

ResetResponse masterReset(const F& f, const uint8_t eraseCode, const uint8_t channel)
{
    uint8_t telegram[16] = {};
    uint8_t sendBuffer[16] = {};
    telegram[8] = eraseCode;
    telegram[9] = channel;
    REQUIRE(f.bcu->processApci(APCI_MASTER_RESET_PDU, telegram, 10, sendBuffer));
    REQUIRE(sendBuffer[9] == 0);
    return {sendBuffer[8], sendBuffer[10]};
}
}

TEST_CASE("MASK0701 master reset", "[poc][masterreset]")
{
    F f;
    f.downloadTables();
    f.memoryWrite(F::MAPPER_START, {0x11, 0x22});
    REQUIRE(f.bcu->flushUserMemory(UsrCallbackType::flash));
    const auto tableBefore = f.memoryRead(F::ADDR_TABLE, 8).data;

    SECTION("invalid erase codes are refused without a restart")
    {
        for (const uint8_t eraseCode : {uint8_t {0}, uint8_t {8}, uint8_t {0xFF}})
        {
            const auto response = masterReset(f, eraseCode, 0);
            REQUIRE(response.errorCode == T_RESTART_UNSUPPORTED_ERASE_CODE);
            REQUIRE(response.seconds == 0);
            REQUIRE(f.bcu->restartType == RestartType::None);
        }
    }

    SECTION("confirmed restart")
    {
        const auto response = masterReset(f, T_MASTERRESET_CONFIRMED_RESTART, 0);
        REQUIRE(response.errorCode == T_RESTART_NO_ERROR);
        REQUIRE(response.seconds == 1);
        REQUIRE(f.bcu->restartType == RestartType::Master);
    }

    SECTION("all erasing codes are answered as unsupported, but the device restarts anyway")
    {
        for (uint8_t eraseCode = T_MASTERRESET_FACTORY_RESET; eraseCode <= T_MASTERRESET_FACTORY_WO_IA; eraseCode++)
        {
            f.bcu->restartType = RestartType::None;
            const auto response = masterReset(f, eraseCode, 1); // channel 1 keeps us away from the magic word
            REQUIRE(response.errorCode == T_RESTART_UNSUPPORTED_ERASE_CODE);
            REQUIRE(response.seconds == 0);
            REQUIRE(f.bcu->restartType == RestartType::Master);
        }
    }

    SECTION("nothing is erased: load states, tables, parameters and the physical address stay")
    {
        const uint16_t ownAddress = f.bcu->ownAddress();
        for (uint8_t eraseCode = T_MASTERRESET_FACTORY_RESET; eraseCode <= T_MASTERRESET_FACTORY_WO_IA; eraseCode++)
        {
            masterReset(f, eraseCode, 1);
        }
        REQUIRE_FALSE(f.bcu->userEeprom->isModified());

        f.restart();
        REQUIRE(f.bcu->applicationRunning());
        REQUIRE(f.bcu->ownAddress() == ownAddress);
        REQUIRE(f.memoryRead(F::ADDR_TABLE, 8).data == tableBefore);
        REQUIRE(f.memoryRead(F::MAPPER_START, 2).data == Data {0x11, 0x22});
    }

    SECTION("magic word restarts into the bootloader")
    {
        int found = -1;
        for (int channel = 0; channel <= 0xFF; channel++)
        {
            if (checkApciForMagicWord(T_MASTERRESET_FACTORY_WO_IA, static_cast<uint8_t>(channel)))
            {
                found = channel;
                break;
            }
        }
        REQUIRE(found >= 0);

        const auto response = masterReset(f, T_MASTERRESET_FACTORY_WO_IA, static_cast<uint8_t>(found));
        REQUIRE(response.errorCode == T_RESTART_NO_ERROR);
        REQUIRE(f.bcu->restartType == RestartType::MasterIntoBootloader);
    }
}
