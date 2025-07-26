/*******************************************************************************
Copyright � 2015, STMicroelectronics International N.V.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of STMicroelectronics nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED.
IN NO EVENT SHALL STMICROELECTRONICS INTERNATIONAL N.V. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************************/

/**
 * @file VL53L0X_i2c.c
 *
 * Copyright (C) 2014 ST MicroElectronics
 *
 * provide variable word size byte/Word/dword VL6180x register access via i2c
 *
 */
#include "mbed.h"
#include "vl53l0x_platform.h"
#include "vl53l0x_api.h"
#include "stdint.h"

/**
 * @def I2C_BUFFER_CONFIG
 *
 * @brief Configure Device register I2C access
 *
 * @li 0 : one GLOBAL buffer \n
 *   Use one global buffer of MAX_I2C_XFER_SIZE byte in data space \n
 *   This solution is not multi-Device compliant nor multi-thread cpu safe \n
 *   It can be the best option for small 8/16 bit MCU without stack and limited ram  (STM8s, 80C51 ...)
 *
 * @li 1 : ON_STACK/local \n
 *   Use local variable (on stack) buffer \n
 *   This solution is multi-thread with use of i2c resource lock or mutex see VL6180x_GetI2CAccess() \n
 *
 * @li 2 : User defined \n
 *    Per Device potentially dynamic allocated. Requires VL6180x_GetI2cBuffer() to be implemented.
 * @ingroup Configuration
 */
#define I2C_BUFFER_CONFIG 1
/** Maximum buffer size to be used in i2c */
#define VL53L0X_MAX_I2C_XFER_SIZE 64 /* Maximum buffer size to be used in i2c */

#if I2C_BUFFER_CONFIG == 0
/* GLOBAL config buffer */
  uint8_t i2c_global_buffer[VL53L0X_MAX_I2C_XFER_SIZE];

  #define DECL_I2C_BUFFER
  #define VL53L0X_GetLocalBuffer(Dev, n_byte) i2c_global_buffer

#elif I2C_BUFFER_CONFIG == 1
/* ON STACK */
  #define DECL_I2C_BUFFER uint8_t LocBuffer[VL53L0X_MAX_I2C_XFER_SIZE];
  #define VL53L0X_GetLocalBuffer(Dev, n_byte) LocBuffer
#elif I2C_BUFFER_CONFIG == 2
/* user define buffer type declare DECL_I2C_BUFFER  as access  via VL53L0X_GetLocalBuffer */
  #define DECL_I2C_BUFFER
#else
  #error "invalid I2C_BUFFER_CONFIG "
#endif

VL53L0X_Error VL53L0X_LockSequenceAccess(VL53L0X_DEV Dev)
{
  VL53L0X_Error Status = VL53L0X_ERROR_NONE;

  return Status;
}

VL53L0X_Error VL53L0X_UnlockSequenceAccess(VL53L0X_DEV Dev)
{
  VL53L0X_Error Status = VL53L0X_ERROR_NONE;

  return Status;
}

VL53L0X_Error VL53L0X_WriteMulti(VL53L0X_DEV Dev, uint8_t index, uint8_t *pdata, uint32_t count)
{
  VL53L0X_Error Status = VL53L0X_ERROR_NONE;
  int32_t status_int = 0;

  char data[count+1];
  data[0]=index;
  for (int i = 1; i< count+1; i++)
  {
      data[i]=pdata[i-1];
  }
  status_int = Dev->i2c_device->write((Dev->i2c_address << 1), data, static_cast<int>(count));

  if (status_int != 0)
  {
    Status = VL53L0X_ERROR_CONTROL_INTERFACE;
  }

  return Status;
}

// the ranging_sensor_comms.dll will take care of the page selection
VL53L0X_Error VL53L0X_ReadMulti(VL53L0X_DEV Dev, uint8_t index, uint8_t *pdata, uint32_t count)
{
  VL53L0X_Error Status = VL53L0X_ERROR_NONE;
  int32_t status_int;
  char i2c_package_cmd[1];
  char i2c_package_data[count];
  

  if (count >= VL53L0X_MAX_I2C_XFER_SIZE)
  {
    
    Status = VL53L0X_ERROR_INVALID_PARAMS;
  }
  else  
  {
    i2c_package_cmd[0]=index;
    status_int = Dev->i2c_device->write((Dev->i2c_address << 1), i2c_package_cmd, 1, true);
    status_int = Dev->i2c_device->read( (Dev->i2c_address << 1), i2c_package_data, count);
  }

  for(int x = 0 ; x < count ; x++)
  {
    pdata[x] = i2c_package_data[x];
  }

  if (status_int != 0)
  {
    Status = VL53L0X_ERROR_CONTROL_INTERFACE;
  }

  return Status;
}

VL53L0X_Error VL53L0X_WrByte(VL53L0X_DEV Dev, uint8_t index, uint8_t data)
{
  VL53L0X_Error Status = VL53L0X_ERROR_NONE;
  int32_t status_int;
  char i2c_package[2];
  i2c_package[0] = index;
  i2c_package[1] = data;  

  status_int = Dev->i2c_device->write((Dev->i2c_address << 1), i2c_package, 2);

  if (status_int != 0)
  {
    Status = VL53L0X_ERROR_CONTROL_INTERFACE;
  }

  return Status;
}

VL53L0X_Error VL53L0X_WrWord(VL53L0X_DEV Dev, uint8_t index, uint16_t data)
{
  VL53L0X_Error Status = VL53L0X_ERROR_NONE;
  int32_t status_int;
  char i2c_package[3];
  i2c_package[0] = index;
  i2c_package[1] = (data >> 8) & 0xFF;  
  i2c_package[2] = (data >> 0) & 0xFF;  

  status_int = Dev->i2c_device->write((Dev->i2c_address << 1), i2c_package, 3);

  if (status_int != 0)
  {
    Status = VL53L0X_ERROR_CONTROL_INTERFACE;
  }

  return Status;
}

VL53L0X_Error VL53L0X_WrDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t data)
{
  VL53L0X_Error Status = VL53L0X_ERROR_NONE;
  int32_t status_int;

  char i2c_package[5];
  i2c_package[0] = index;
  i2c_package[1] = (data >> 24) & 0xFF;  
  i2c_package[2] = (data >> 16) & 0xFF;  
  i2c_package[3] = (data >>  8) & 0xFF;  
  i2c_package[4] = (data >>  0) & 0xFF;  

  status_int = Dev->i2c_device->write((Dev->i2c_address << 1), i2c_package, 5);

  if (status_int != 0)
  {
    Status = VL53L0X_ERROR_CONTROL_INTERFACE;
  }
  return Status;
}

VL53L0X_Error VL53L0X_UpdateByte(VL53L0X_DEV Dev, uint8_t index, uint8_t AndData, uint8_t OrData)
{
  VL53L0X_Error Status = VL53L0X_ERROR_NONE;
  int32_t status_int;
  char data;
  char ind = index;
  status_int = Dev->i2c_device->write((Dev->i2c_address << 1), &ind, 1, true);
  status_int = Dev->i2c_device->read((Dev->i2c_address << 1), &data, 1);

  if (status_int != 0)
  {
    Status = VL53L0X_ERROR_CONTROL_INTERFACE;
  }

  if (Status == VL53L0X_ERROR_NONE)
  {
    data = (data & AndData) | OrData;

    status_int = Dev->i2c_device->write((Dev->i2c_address << 1), &data, 1);

    if (status_int != 0)
    {
      Status = VL53L0X_ERROR_CONTROL_INTERFACE;
    }
  }

  return Status;
}

VL53L0X_Error VL53L0X_RdByte(VL53L0X_DEV Dev, uint8_t index, uint8_t *data)
{
  VL53L0X_Error Status = VL53L0X_ERROR_NONE;
  int32_t status_int;
  char  pdata;
  char  ind = index;

  status_int = Dev->i2c_device->write((Dev->i2c_address << 1), &ind, 1, true);
  status_int = Dev->i2c_device->read((Dev->i2c_address << 1), &pdata, 1);

  *data = pdata;

  if (status_int != 0)
  {
    Status = VL53L0X_ERROR_CONTROL_INTERFACE;
  }

  return Status;
}

VL53L0X_Error VL53L0X_RdWord(VL53L0X_DEV Dev, uint8_t index, uint16_t *data)
{
  VL53L0X_Error Status = VL53L0X_ERROR_NONE;
  int32_t   status_int;
  char      pdata[2];
  char      ind = index;
  uint16_t  tmp = 0;

  status_int = Dev->i2c_device->write((Dev->i2c_address << 1), &ind, 1, true);
  status_int = Dev->i2c_device->read((Dev->i2c_address << 1), pdata, 2);
  tmp |= pdata[1]<<0;
  tmp |= pdata[0]<<8;
  *data = tmp;

  if (status_int != 0)
  {
    Status = VL53L0X_ERROR_CONTROL_INTERFACE;
  }


  return Status;
}

VL53L0X_Error VL53L0X_RdDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t *data)
{
  VL53L0X_Error Status = VL53L0X_ERROR_NONE;
  int32_t   status_int;
  char      pdata[4];
  char      ind = index;
  uint32_t  tmp = 0;

  status_int = Dev->i2c_device->write((Dev->i2c_address << 1), &ind, 1, true);
  status_int = Dev->i2c_device->read((Dev->i2c_address << 1), pdata, 4);
  tmp |= pdata[3]<<0;
  tmp |= pdata[2]<<8;
  tmp |= pdata[1]<<16;
  tmp |= pdata[0]<<24;
  *data = tmp;

  if (status_int != 0)
  {
    Status = VL53L0X_ERROR_CONTROL_INTERFACE;
  }

  return Status;
}

#define VL53L0X_POLLINGDELAY_LOOPNB 250

VL53L0X_Error VL53L0X_PollingDelay(VL53L0X_DEV Dev)
{
  VL53L0X_Error status = VL53L0X_ERROR_NONE;
  volatile uint32_t i;
  for(i=0;i<VL53L0X_POLLINGDELAY_LOOPNB;i++){
    //Do nothing
    asm("nop");
  }
  return status;
}
