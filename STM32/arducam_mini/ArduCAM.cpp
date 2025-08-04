#include "ArduCAM.h"
#include "mbed.h"
#include "ov2640_regs.h"
// Constructor
ArduCAM::ArduCAM(model cam_model, PinName cs_pin, SPI* spi)
    : m_model(cam_model), m_cs(cs_pin, 1), m_spi(spi) {
    m_burst_mode = false;
}

void ArduCAM::csHigh() {
    m_cs = 1;
}

void ArduCAM::csLow() {
    m_cs = 0;
}

void ArduCAM::write_reg(uint8_t addr, uint8_t data) {
    csLow();
    m_spi->write(addr | 0x80);
    m_spi->write(data);
    csHigh();
}

uint8_t ArduCAM::read_reg(uint8_t addr) {
    csLow();
    m_spi->write(addr & 0x7F);
    uint8_t val = m_spi->write(0x00);
    csHigh();
    return val;
}

void ArduCAM::set_bit(uint8_t addr, uint8_t bit) {
    uint8_t temp = read_reg(addr);
    write_reg(addr, temp | bit);
}

void ArduCAM::clear_bit(uint8_t addr, uint8_t bit) {
    uint8_t temp = read_reg(addr);
    write_reg(addr, temp & (~bit));
}

uint8_t ArduCAM::get_bit(uint8_t addr, uint8_t bit) {
    return read_reg(addr) & bit;
}

void ArduCAM::flush_fifo() {
    write_reg(ARDUCHIP_FIFO,
              FIFO_CLEAR_MASK | FIFO_RDPTR_RST_MASK | FIFO_WRPTR_RST_MASK);
}

void ArduCAM::start_capture() {
    write_reg(ARDUCHIP_FIFO, FIFO_START_MASK);
}

uint8_t ArduCAM::read_fifo() {
    csLow();
    m_spi->write(BURST_FIFO_READ);
    uint8_t val = m_spi->write(0x00);
    csHigh();
    return val;
}

void ArduCAM::burst_read_fifo(uint8_t* buf, uint32_t length) {
    csLow();
    m_spi->write(BURST_FIFO_READ);
    for (uint32_t i = 0; i < length; i++) {
        buf[i] = m_spi->write(0x00);
    }
    csHigh();
}

uint32_t ArduCAM::read_fifo_length() {
    uint32_t len = 0;
    len |= (uint32_t)read_reg(FIFO_SIZE1);
    len |= (uint32_t)read_reg(FIFO_SIZE2) << 8;
    len |= (uint32_t)read_reg(FIFO_SIZE3) << 16;
    return len & 0x007FFFFF;
}

// I2C register writing function
void ArduCAM::wrSensorReg8_8(I2C& i2c, uint8_t regID, uint8_t regDat) {
    char data[2] = { (char)regID, (char)regDat };
    i2c.write(OV2640_I2C_ADDR << 1, data, 2);
}

// I2C register reading function
void ArduCAM::rdSensorReg8_8(I2C& i2c, uint8_t regID, uint8_t* regDat) {
    char reg = regID;
    i2c.write(OV2640_I2C_ADDR << 1, &reg, 1, true);
    i2c.read(OV2640_I2C_ADDR << 1, (char*)regDat, 1);
}

// Write a list of registers
int ArduCAM::wrSensorRegs8_8(I2C& i2c, const struct sensor_reg reglist[]) {
    int i = 0;
    while ((reglist[i].reg != 0xFF) || (reglist[i].val != 0xFF)) {
        wrSensorReg8_8(i2c, reglist[i].reg, reglist[i].val);
        i++;
    }
    return 0;
}

void ArduCAM::set_format(uint8_t fmt, I2C& i2c) {
    if (fmt == JPEG) {
        wrSensorRegs8_8(i2c, OV2640_JPEG_INIT);
        wrSensorRegs8_8(i2c, OV2640_YUV422);
        wrSensorRegs8_8(i2c, OV2640_JPEG);
        wrSensorReg8_8(i2c, 0xFF, 0x01);
        wrSensorReg8_8(i2c, 0x15, 0x00);
    }
}

void ArduCAM::OV2640_set_JPEG_size(I2C& i2c, uint8_t size) {
    switch (size) {
      case OV2640_160x120:   wrSensorRegs8_8(i2c, OV2640_160x120_JPEG);   break;
      case OV2640_176x144:   wrSensorRegs8_8(i2c, OV2640_176x144_JPEG);   break;
      case OV2640_320x240:   wrSensorRegs8_8(i2c, OV2640_320x240_JPEG);   break;
      case OV2640_352x288:   wrSensorRegs8_8(i2c, OV2640_352x288_JPEG);   break;
      case OV2640_640x480:   wrSensorRegs8_8(i2c, OV2640_640x480_JPEG);   break;
      case OV2640_800x600:   wrSensorRegs8_8(i2c, OV2640_800x600_JPEG);   break;
      case OV2640_1024x768:  wrSensorRegs8_8(i2c, OV2640_1024x768_JPEG);  break;
      case OV2640_1280x1024: wrSensorRegs8_8(i2c, OV2640_1280x1024_JPEG); break;
      case OV2640_1600x1200: wrSensorRegs8_8(i2c, OV2640_1600x1200_JPEG); break;
      default:               wrSensorRegs8_8(i2c, OV2640_320x240_JPEG);   break;
    }
}

void ArduCAM::set_jpeg_quality(I2C& i2c, uint8_t q) {
    // Bank 0
    wrSensorReg8_8(i2c, 0xFF, 0x00);
    wrSensorReg8_8(i2c, 0x44, q);   // try 0x24 (high), 0x2A (med), 0x36 (low)
}


void ArduCAM::init_camera(I2C& i2c) {
    // Reset and initialize camera registers
    wrSensorReg8_8(i2c, 0xFF, 0x01);
    wrSensorReg8_8(i2c, 0x12, 0x80);
    ThisThread::sleep_for(100ms);
    wrSensorRegs8_8(i2c, OV2640_JPEG_INIT);
    wrSensorRegs8_8(i2c, OV2640_YUV422);
    wrSensorRegs8_8(i2c, OV2640_JPEG);
    wrSensorReg8_8(i2c, 0xFF, 0x01);
    wrSensorReg8_8(i2c, 0x15, 0x00);
}

