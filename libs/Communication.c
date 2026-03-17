/**
*   @file     Communication.c
*   @brief    Communication file
*   @author   Analog Devices Inc.
*
********************************************************************************
* Copyright 2016(c) Analog Devices, Inc.
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*  - Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*  - Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in
*    the documentation and/or other materials provided with the
*    distribution.
*  - Neither the name of Analog Devices, Inc. nor the names of its
*    contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*  - The use of this software may or may not infringe the patent rights
*    of one or more patent holders.  This license does not release you
*    from the requirement that you obtain separate licenses from these
*    patent holders to use this software.
*  - Use of the software either in source or binary form, must be run
*    on or directly connected to an Analog Devices Inc. component.
*
* THIS SOFTWARE IS PROVIDED BY ANALOG DEVICES "AS IS" AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, NON-INFRINGEMENT,
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL ANALOG DEVICES BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, INTELLECTUAL PROPERTY RIGHTS, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
********************************************************************************/

#include "Communication.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"

uint8_t convFlag = 0, readConv;


void SPI_Write(spi_device_handle_t* device, uint8_t* data, uint16_t bytesNumber)
{
   esp_err_t err;
   spi_transaction_t t;

   memset(&t, 0, sizeof(spi_transaction_t));

   t.length = 8 * bytesNumber;
   t.tx_buffer = data;
   t.user = (void*)0;
   err = spi_device_polling_transmit(*device,&t);
   assert(err == ESP_OK);
}

/***************************************************************************//**
 * @brief Reads data from SPI.
 *
 * @param data - As an input parameter, data represents the write buffer:
 *               - first byte is the chip select number;
 *               - from the second byte onwards are located data bytes to write.
 *               As an output parameter, data represents the read buffer:
 *               - from the first byte onwards are located the read data bytes.
 * @param bytesNumber - Number of bytes to write.
 *
 * @return Number of written bytes.
*******************************************************************************/
void SPI_Read(spi_device_handle_t* device, uint8_t* data, uint16_t bytesNumber)
{
   spi_transaction_t t;
   memset(&t, 0 ,sizeof(spi_transaction_t));
   t.length = 8 * bytesNumber;
   t.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;

   unsigned char writeData[4]  = {0, 0, 0, 0};
   unsigned char count          = 0;

    for(count = 0;count < bytesNumber;count++)
    {
        if(count == 0)
           t.tx_data[count] = data[count];
        else
           t.tx_data[count] = 0xAA;    /* dummy value */
    }

   spi_device_polling_transmit(*device, &t);

    for(count = 0; count < bytesNumber;count++)
    {

       data[count] = t.rx_data[count];
    }

}




