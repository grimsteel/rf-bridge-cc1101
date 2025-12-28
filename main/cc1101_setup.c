#include "cc1101_setup.h"
#include "cc1101.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

#define TAG "CC1101 Setup"

// Register settings for CC1101 (315 MHz, AM650 modulation)
// Exported from TI Smart RF
uint8_t registers[] = {
  0x0D,  // IOCFG2        GDO2 Output Pin Configuration
  0x2E,  // IOCFG1        GDO1 Output Pin Configuration
  0x0D,  // IOCFG0        GDO0 Output Pin Configuration
  0x07,  // FIFOTHR       RX FIFO and TX FIFO Thresholds
  0xD3,  // SYNC1         Sync Word, High Byte
  0x91,  // SYNC0         Sync Word, Low Byte
  0xFF,  // PKTLEN        Packet Length
  0x04,  // PKTCTRL1      Packet Automation Control
  0x32,  // PKTCTRL0      Packet Automation Control
  0x00,  // ADDR          Device Address
  0x00,  // CHANNR        Channel Number
  0x06,  // FSCTRL1       Frequency Synthesizer Control
  0x00,  // FSCTRL0       Frequency Synthesizer Control
  0x0C,  // FREQ2         Frequency Control Word, High Byte
  0x1D,  // FREQ1         Frequency Control Word, Middle Byte
  0x89,  // FREQ0         Frequency Control Word, Low Byte
  0x17,  // MDMCFG4       Modem Configuration
  0x32,  // MDMCFG3       Modem Configuration
  0x30,  // MDMCFG2       Modem Configuration
  0x00,  // MDMCFG1       Modem Configuration
  0x00,  // MDMCFG0       Modem Configuration
  0x15,  // DEVIATN       Modem Deviation Setting
  0x07,  // MCSM2         Main Radio Control State Machine Configuration
  0x30,  // MCSM1         Main Radio Control State Machine Configuration
  0x18,  // MCSM0         Main Radio Control State Machine Configuration
  0x18,  // FOCCFG        Frequency Offset Compensation Configuration
  0x6C,  // BSCFG         Bit Synchronization Configuration
  0x07,  // AGCCTRL2      AGC Control
  0x00,  // AGCCTRL1      AGC Control
  0x91,  // AGCCTRL0      AGC Control
  0x87,  // WOREVT1       High Byte Event0 Timeout
  0x6B,  // WOREVT0       Low Byte Event0 Timeout
  0xFB,  // WORCTRL       Wake On Radio Control
  0x56,  // FREND1        Front End RX Configuration
  0x11,  // FREND0        Front End TX Configuration
  0xE9,  // FSCAL3        Frequency Synthesizer Calibration
  0x2A,  // FSCAL2        Frequency Synthesizer Calibration
  0x00,  // FSCAL1        Frequency Synthesizer Calibration
  0x1F,  // FSCAL0        Frequency Synthesizer Calibration
  0x41,  // RCCTRL1       RC Oscillator Configuration
  0x00,  // RCCTRL0       RC Oscillator Configuration
  0x59,  // FSTEST        Frequency Synthesizer Calibration Control
  0x7F,  // PTEST         Production Test
  0x3F,  // AGCTEST       AGC Test
  0x88,  // TEST2         Various Test Settings
  0x31,  // TEST1         Various Test Settings
  0x0B,  // TEST0         Various Test Settings
};

// -15 dBm on 315 MHz configuration
uint8_t patable[8] = {0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

esp_err_t init_cc1101(cc1101_device_t** cc1101_handle) {
  spi_bus_config_t spi_bus_cfg = {
    .miso_io_num = GPIO_NUM_13,
    .mosi_io_num = GPIO_NUM_11,
    .sclk_io_num = GPIO_NUM_12,
  };

  cc1101_device_cfg_t cfg = {
    .spi_host = SPI2_HOST,
    .gdo0_io_num = GPIO_NUM_33,
    .gdo2_io_num = GPIO_NUM_21,
    .cs_io_num = GPIO_NUM_34,
    .miso_io_num = GPIO_NUM_13,
    // Check your hardware for setting the correct value!
    .crystal_freq = CC1101_CRYSTAL_26MHZ
  };

  cc1101_device_t* cc;

  // Initialize SPI, CC1101, and reset CC1101
  ESP_LOGI(TAG, "Initialize CC1101");
  ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &spi_bus_cfg, SPI_DMA_CH_AUTO), TAG, "Failed to initialize CC1101 SPI bus");
  ESP_RETURN_ON_ERROR(cc1101_init(&cfg, &cc), TAG, "Failed to initialize CC1101");
  ESP_RETURN_ON_ERROR(cc1101_hard_reset(cc), TAG, "Failed to reset CC1101");

  // Configure registers and PA Table
  ESP_LOGI(TAG, "Configure CC1101");
  ESP_RETURN_ON_ERROR(cc1101_write_burst(cc, CC1101_FIRST_CFG_REG, registers, sizeof(registers)), TAG, "Failed to write register configuration to CC1101");
  ESP_RETURN_ON_ERROR(cc1101_write_patable(cc, patable), TAG, "Failed to write PA Table to CC1101");

  *cc1101_handle = cc;

  return ESP_OK;
}
esp_err_t cc1101_start_rx(cc1101_device_t* cc) {
    // set idle
    ESP_LOGI(TAG, "Set idle");
    ESP_RETURN_ON_ERROR(cc1101_set_idle(cc), TAG, "Failed to set CC1101 idle");

    vTaskDelay(1); // Wait
  // Start RX mode
  ESP_LOGI(TAG, "Start RX mode");
  ESP_RETURN_ON_ERROR(cc1101_enable_rx(cc, CC1101_TRANS_MODE_ASYNCHRONOUS), TAG, "Failed to enable CC1101 RX mode");

  vTaskDelay(1); // Wait for auto calibration to finish.

  return ESP_OK;
}
esp_err_t cc1101_start_tx(cc1101_device_t* cc) {
    // set idle
    ESP_LOGI(TAG, "Set idle");
    ESP_RETURN_ON_ERROR(cc1101_set_idle(cc), TAG, "Failed to set CC1101 idle");

    vTaskDelay(1); // Wait

  // Start TX mode
  ESP_LOGI(TAG, "Start TX mode");
  ESP_RETURN_ON_ERROR(cc1101_enable_tx(cc, CC1101_TRANS_MODE_ASYNCHRONOUS), TAG, "Failed to enable CC1101 TX mode");

  vTaskDelay(1); // Wait for auto calibration to finish.

  return ESP_OK;
}
