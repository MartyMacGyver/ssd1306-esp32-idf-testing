#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <esp_task.h>
#include <esp_system.h>
#include <driver/spi_master.h>

#include "ssd1306.h"
#include "iface_esp32_spi.h"

#define TraceHere( ) printf( "%s: line %d\n", __FUNCTION__, __LINE__ )

static const int MOSIPin = 23;
static const int SCKPin = 18;
static const int DCPin = 15;

int ESP32_WriteCommand_SPI( struct SSD1306_Device* DeviceHandle, SSDCmd SSDCommand ) {
    static uint8_t TempCommandByte = 0;
    spi_device_handle_t SPIDevice = NULL;
    spi_transaction_t SPITransaction;
    esp_err_t Result = ESP_OK;

    NullCheck( DeviceHandle, return 0 );
        SPIDevice = ( spi_device_handle_t ) DeviceHandle->User0;
    NullCheck( SPIDevice, return 0 );

    memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );

    TempCommandByte = ( uint8_t ) SSDCommand;

    SPITransaction.length = sizeof( uint8_t ) * 8;
    SPITransaction.tx_buffer = ( const void* ) &TempCommandByte;

    gpio_set_level( DCPin, 0 ); /* Command mode is DC low */
    Result = spi_device_transmit( SPIDevice, &SPITransaction );

    return ( Result == ESP_OK ) ? 1 : 0;
}

int ESP32_WriteData_SPI( struct SSD1306_Device* DeviceHandle, uint8_t* Data, size_t DataLength ) {
    spi_device_handle_t SPIDevice = NULL;
    spi_transaction_t SPITransaction;
    esp_err_t Result = ESP_OK;

    NullCheck( DeviceHandle, return 0 );
    NullCheck( Data, return 0 );
        SPIDevice = ( spi_device_handle_t ) DeviceHandle->User0;
    NullCheck( SPIDevice, return 0 );

    memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );

    SPITransaction.length = DataLength * 8;
    SPITransaction.tx_buffer = Data;

    gpio_set_level( DCPin, 1 ); /* Data mode is DC high */
        Result = spi_device_transmit( SPIDevice, &SPITransaction );
    gpio_set_level( DCPin, 0 );

    return ( Result == ESP_OK ) ? 1 : 0;
}

int ESP32_Reset_SPI( struct SSD1306_Device* DeviceHandle ) {
    NullCheck( DeviceHandle, return 0 );

    if ( GPIO_IS_VALID_OUTPUT_GPIO( DeviceHandle->RSTPin ) ) {
        gpio_set_level( DeviceHandle->RSTPin, 0 );
            vTaskDelay( pdMS_TO_TICKS( 100 ) );
        gpio_set_level( DeviceHandle->RSTPin, 1 );

        return 0;
    }

    return 0;
}

int ESP32_InitSPIMaster( void ) {
    spi_bus_config_t BusConfig;

    gpio_set_direction( DCPin, GPIO_MODE_OUTPUT );
    gpio_set_level( DCPin, 0 );

    memset( &BusConfig, 0, sizeof( spi_bus_config_t ) );

    BusConfig.mosi_io_num = MOSIPin;
    BusConfig.sclk_io_num = SCKPin;
    BusConfig.quadhd_io_num = -1;
    BusConfig.quadwp_io_num = -1;
    BusConfig.miso_io_num = -1;

    return spi_bus_initialize( HSPI_HOST, &BusConfig, 1 ) == ESP_OK ? 1 : 0;
}

int ESP32_AddDevice_SPI( struct SSD1306_Device* DeviceHandle, int Width, int Height, int CSPin, int RSTPin ) {
    spi_device_interface_config_t SPIDevice;
    spi_device_handle_t SPIHandle;

    NullCheck( DeviceHandle, return 0 );
    memset( &SPIDevice, 0, sizeof( spi_device_interface_config_t ) );

    SPIDevice.clock_speed_hz = 100000; /* 100KHz */
    SPIDevice.mode = 0;
    SPIDevice.spics_io_num = CSPin;
    SPIDevice.queue_size = 100;

    gpio_set_direction( RSTPin, GPIO_MODE_OUTPUT );
    gpio_set_level( RSTPin, 0 );   /* Reset happens later and this will remain high */

    if ( spi_bus_add_device( HSPI_HOST, &SPIDevice, &SPIHandle ) == ESP_OK ) {
        printf( "Here: Device handle = 0x%08X\n", ( uint32_t ) SPIHandle );
        return SSD1306_Init_SPI( DeviceHandle, Width, Height, RSTPin, CSPin, ( uint32_t ) SPIHandle, ESP32_WriteCommand_SPI, ESP32_WriteData_SPI, ESP32_Reset_SPI );
    }

    return 0;
}
