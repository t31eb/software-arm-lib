/*
 * Test class for the userEeprom.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#ifndef TEST_SBLIB_KNX_USEREEPROM_H_
#define TEST_SBLIB_KNX_USEREEPROM_H_

#include <sblib/eib/userEeprom.h>


class TestUserEeprom final : public UserEeprom
{
public:
    TestUserEeprom() = delete;
    using UserEeprom::UserEeprom;
    //TestUserEeprom(unsigned int start, unsigned int size, unsigned int flashSize);
    using UserEeprom::findValidPage;

    [[nodiscard]] uint8_t optionReg() const override { return field8(0); }
    void setOptionReg(const uint8_t value) override { setField8(0, value); }
    [[nodiscard]] uint8_t manuDataH() const override { return field8(1); }
    void setManuDataH(const uint8_t value) override { setField8(1, value); }
    [[nodiscard]] uint8_t manuDataL() const override { return field8(2); }
    void setManuDataL(const uint8_t value) override { setField8(2, value); }
    [[nodiscard]] uint8_t manufacturerH() const override { return field8(3); }
    void setManufacturerH(const uint8_t value) override { setField8(3, value); }
    [[nodiscard]] uint8_t manufacturerL() const override { return field8(4); }
    void setManufacturerL(const uint8_t value) override { setField8(4, value); }
    [[nodiscard]] uint8_t deviceTypeH() const override { return field8(5); }
    void setDeviceTypeH(const uint8_t value) override { setField8(5, value); }
    [[nodiscard]] uint8_t deviceTypeL() const override { return field8(6); }
    void setDeviceTypeL(const uint8_t value) override { setField8(6, value); }
    [[nodiscard]] uint8_t version() const override { return field8(7); }
    void setVersion(const uint8_t value) override { setField8(7, value); }
    [[nodiscard]] uint8_t checkLimit() const override { return field8(8); }
    void setCheckLimit(const uint8_t value) override { setField8(8, value); }
    [[nodiscard]] uint8_t appPeiType() const override { return field8(9); }
    void setAppPeiType(const uint8_t value) override { setField8(9, value); }
    [[nodiscard]] uint8_t syncRate() const override { return field8(10); }
    void setSyncRate(const uint8_t value) override { setField8(10, value); }
    [[nodiscard]] uint8_t portCDDR() const override { return field8(11); }
    void setPortCDDR(const uint8_t value) override { setField8(11, value); }
    [[nodiscard]] uint8_t portADDR() const override { return field8(12); }
    void setPortADDR(const uint8_t value) override { setField8(12, value); }
    [[nodiscard]] uint8_t runError() const override { return field8(13); }
    void setRunError(const uint8_t value) override { setField8(13, value); }
    [[nodiscard]] uint8_t routeCnt() const override { return field8(14); }
    void setRouteCnt(const uint8_t value) override { setField8(14, value); }
    [[nodiscard]] uint8_t maxRetransmit() const override { return field8(15); }
    void setMaxRetransmit(const uint8_t value) override { setField8(15, value); }
    [[nodiscard]] uint8_t confDesc() const override { return field8(16); }
    void setConfDesc(const uint8_t value) override { setField8(16, value); }
    [[nodiscard]] uint8_t assocTabPtr() const override { return field8(17); }
    void setAssocTabPtr(const uint8_t value) override { setField8(17, value); }
    [[nodiscard]] uint8_t commsTabPtr() const override { return field8(18); }
    void setCommsTabPtr(const uint8_t value) override { setField8(18, value); }
    [[nodiscard]] uint8_t usrInitPtr() const override { return field8(19); }
    void setUsrInitPtr(const uint8_t value) override { setField8(19, value); }
    [[nodiscard]] uint8_t usrProgPtr() const override { return field8(20); }
    void setUsrProgPtr(const uint8_t value) override { setField8(20, value); }

    [[nodiscard]] uint8_t addrTabSize() const override { return field8(21); }
    void setAddrTabSize(const uint8_t value) override { setField8(21, value); }
    [[nodiscard]] uint8_t* addrTab() const override { return &userEepromData[22]; }
};



#endif /* TEST_SBLIB_KNX_USEREEPROM_H_ */
