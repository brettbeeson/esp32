#include "I2CManager.h"

#include "driver/i2c.h"

#include "esp_log.h"
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
static const char *TAG = "I2CManager";

#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE 0 // I2C master  not need buffer
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE 0 // I2C master  not need buffer
#define I2C_EXAMPLE_MASTER_FREQ_HZ 100000   // I2C master clock frequency


#define I2C_EXAMPLE_MASTER_SCL_IO                                              \
  GPIO_NUM_22 /*!< gpio number for I2C master clock */ ////////////
#define I2C_EXAMPLE_MASTER_SDA_IO                                              \
  GPIO_NUM_21 /*!< gpio number for I2C master data  */ /////////////
#define I2C_EXAMPLE_MASTER_NUM I2C_NUM_1

I2CManagerClass I2CManager;

I2CManagerClass::I2CManagerClass(gpio_num_t sda, gpio_num_t scl)
    : _sclPin(scl), _sdaPin(sda) {
  _access = xSemaphoreCreateMutex();
  assert(_access);
}

I2CManagerClass::~I2CManagerClass() { end(); }

void I2CManagerClass::begin() {
  i2c_port_t i2c_master_port = I2C_NUM_1;
  i2c_config_t conf;
  conf.mode = I2C_MODE_MASTER;
  conf.sda_io_num = _sdaPin;
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
  conf.scl_io_num = _sclPin;
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
  conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;
  ESP_ERROR_CHECK(i2c_param_config(i2c_master_port, &conf));
  ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, conf.mode,
                     I2C_EXAMPLE_MASTER_RX_BUF_DISABLE,
                     I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0));
  _ready = true;
  ESP_LOGI(TAG, "begin sda:%d scl%d", (int)_sdaPin, (int)_sclPin);
  
}
void I2CManagerClass::end() {
  vSemaphoreDelete(_access);
  _access = NULL;
  _ready = false;
}

void I2CManagerClass::take() {
  assert(_ready);
  xSemaphoreTake(_access, portMAX_DELAY);
}

void I2CManagerClass::give() { xSemaphoreGive(_access); }

const char *I2CManagerClass::c_str() { return str().c_str(); }
String I2CManagerClass::str() {
  String r;
  r += "sda:";
  r += _sdaPin;
  r += " scl:";
  r += _sclPin;
  return r;
}

int I2CManagerClass::scan() {

  uint8_t error, address;
  int nDevices = 0;
  for (address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    assert(0); // convert to driver
    // Wire.beginTransmission(address);
    // error = Wire.endTransmission();

    if (error == 0) {
      ESP_LOGI(TAG, "I2C device found at address 0x (not implemented)");
      /*    if (address < 16)
            ESP_LOGI(TAG,"0");
          ESP_LOGI(TAG,address, HEX);
          ESP_LOGI(TAG,"  !");
      */
      nDevices++;
    } else if (error == 4) {
      ESP_LOGE(TAG, "Unknown error at address 0x");
      /*
        if (address < 16)
        ESP_LOGE(TAG,"0");
        ESP_LOGE(TAG,address, HEX);
      */
    }
  }
  if (nDevices == 0) {
    ESP_LOGI(TAG, "No I2C devices found");
  }
  return nDevices;
}
