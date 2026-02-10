#include "defines.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "pmu.h"


#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"


extern "C" {

static const char *TAG = "pmu";

/* Koordiner med andre steder hvor i2c er brukt */
#define I2C_MASTER_NUM                  I2C_NUM_0
#define I2C_MASTER_SDA_IO               (gpio_num_t)CONFIG_PMU_I2C_SDA
#define I2C_MASTER_SCL_IO               (gpio_num_t)CONFIG_PMU_I2C_SCL
#define I2C_MASTER_FREQ_HZ              CONFIG_I2C_MASTER_FREQUENCY /*!< I2C master clock frequency */

#define PMU_LOWBAT_SHUTDOWN  5

// I2C handles - extern to allow sharing with display module
// Type i2c_master_bus_handle_t is defined in driver/i2c_master.h
extern i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t pmu_dev_handle = NULL;


static int pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len);
static int pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len);


static XPowersPMU PMU;


/************************************************************
 * Initialize the PMU. We assume that I2C bus is already 
 * initialized  
 ************************************************************/

esp_err_t pmu_init()
{
    // Add PMU device to the I2C bus (bus should already be created by display init)
    if (pmu_dev_handle == NULL && bus_handle != NULL) {
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = AXP2101_SLAVE_ADDRESS,
            .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        };
        esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_config, &pmu_dev_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add PMU device to I2C bus");
            return ESP_FAIL;
        }
    }

    /* Implemented using read and write callback methods, applicable to other platforms */
    if (PMU.begin(AXP2101_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte)) {
        ESP_LOGI(TAG, "Init PMU SUCCESS!");
    } else {
        ESP_LOGE(TAG, "Init PMU FAILED!");
        return ESP_FAIL;
    }
    /* Minimum common working voltage of the PMU VBUS input */
    PMU.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_3V88);

    /* Set the maximum current of the PMU VBUS input */
    PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_2000MA);
    
    /* Set VSYS off voltage as 2600mV , Adjustment range 2600mV ~ 3300mV */
    PMU.setSysPowerDownVoltage(2600);
    
    /* Don't change these VDDs */
    PMU.setDC3Voltage(3400);
    PMU.setALDO2Voltage(3300);
        // Optional output Vx-1 on ARCTIC4
    PMU.setALDO4Voltage(3300); 
        // GPS on T-TWR
    PMU.setBLDO1Voltage(1800);
        // Set to 1800 if ADC_ATTEN_DB_6
        // VREFADC on ARCTIC4
    
    
    
    /* The following supply voltages can be controlled by the user
     * 1400~3700mV,100mV/step, 24steps */
    PMU.setDC5Voltage(3300);
      // Optional output Vx-2 on ARCTIC4
  
    /* 500~3500mV, 100mV/step, 31steps */
    PMU.setALDO1Voltage(3300);
       // GPS on ARCTIC4
    
    PMU.setALDO3Voltage(3300);
       // Not used on ARCTIC4
    
    PMU.setBLDO2Voltage(3300);
       // Not used on ARCTIC4
    
    return ESP_OK;
}



/************************************************************
 * Initial setup of power-management on PMU
 ************************************************************/

void pmu_power_setup() 
{
    /* Turn on the power that needs to be used
     * DC1 ESP32S3 Core VDD , Don't change */
    PMU.enableDC1();

#if DEVICE == ARCTIC4
    PMU.disableDC5();  // Optional Vx-2
    PMU.enableALDO1(); // GPS
    PMU.disableALDO3();
    PMU.disableBLDO2();
    PMU.enableBLDO1();
    PMU.disableALDO2(); // Optional Vx-1
    PMU.enableDC3();
    
#else
    /* External pin power supply */
    PMU.enableDC5();
    PMU.enableALDO1();
    PMU.enableALDO3();
    PMU.enableBLDO2();

    /* ALDO2 MICRO TF Card VDD */
    PMU.enableALDO2();

    /* ALDO4 GNSS VDD */
    PMU.enableALDO4();

    /* BLDO1 MIC VDD */
    PMU.enableBLDO1();
    
#endif
        
    /* power off when not in use */
    PMU.disableDC2();
    PMU.disableDC3();
    PMU.disableDC4();
    PMU.disableCPUSLDO();
    PMU.disableDLDO1();
    PMU.disableDLDO2();
}



/************************************************************
 * Initial setup of battery/charging management on PMU
 ************************************************************/

void pmu_batt_setup() 
{    
  /* It is necessary to disable the detection function of the TS pin on the board
   * without the battery temperature detection function, otherwise it will cause abnormal charging 
   */
  PMU.disableTSPinMeasure();

  /* Enable internal ADC detection */
  PMU.enableBattDetection();
  PMU.enableVbusVoltageMeasure();
  PMU.enableBattVoltageMeasure();
  PMU.enableSystemVoltageMeasure();
  
  /* Set the precharge charging current */
  PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_150MA);

  /* Set constant current charge current limit
   * ! Please pay attention to add a suitable heat sink above the PMU when setting the charging current to 1A
   */
  PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_800MA);

  /* Set stop charging termination current */
  PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_150MA);

  /* Set charge cut-off voltage */
  PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
  
  /* Shut down if battery is lower than 5% */
  PMU.setLowBatShutdownThreshold(PMU_LOWBAT_SHUTDOWN);
  
  PMU.fuelGaugeControl(true, true);
//  PMU.resetGaugeBesides();
}


/*****************************************************
 * Turn on the power to the GPS
 *****************************************************/

void pmu_gps_on(bool on) 
{
#if DEVICE == ARCTIC4
    if (on)
        PMU.enableALDO4();
    else
        PMU.disableALDO4();
#else
        if (on)
        PMU.enableALDO1();
    else
        PMU.disableALDO1();
#endif
}


void pmu_dc3_on(bool on) {
    if (on)
        PMU.enableDC3(); 
    else
        PMU.disableDC3();
}


bool pmu_dc3_isOn() {
    return PMU.isEnableDC3();
}


bool pmu_dc5_isOn() {
    return PMU.isEnableDC5();
}


/*****************************************************
 * Return the battery voltage in millivolts
 *****************************************************/

uint16_t pmu_getBattVoltage() {
    return PMU.getBattVoltage();
}


/*****************************************************
 * Return the charging-level of the battery in percent
 *****************************************************/

uint16_t pmu_getBattPercent() {
    return PMU.getBatteryPercent();
}


/*****************************************************
 * Return the temperature (of the PMU?) 
 *****************************************************/
float pmu_getTemperature() {
    return PMU.getTemperature();
}


/*****************************************************
 * Return true if the battery is charging
 *****************************************************/

bool pmu_isCharging() {
    return PMU.isCharging();
}
    
    
/*****************************************************
 * Return true if connected on VBUS
 *****************************************************/

bool pmu_isVbusIn() {
    return PMU.isVbusIn();
}


/*****************************************************
 * Shut it down
 *****************************************************/

void pmu_shutdown() {
    PMU.shutdown(); 
}


/*****************************************************
 * Temporarily disable automatic shutdown
 *****************************************************/

void pmu_disableShutdown(bool on) {
   PMU.setLowBatShutdownThreshold(on ? 0 : PMU_LOWBAT_SHUTDOWN);
}


/*****************************************************
 * Print status information on console
 *****************************************************/
void pmu_printInfo()
{
    printf("Charging:        %s\n", (PMU.isCharging() ? "YES" : "NO"));
    printf("Discharge:       %s\n", (PMU.isDischarge() ? "YES" : "NO"));
    printf("Standby:         %s\n", (PMU.isStandby() ? "YES" : "NO"));
    printf("Vbus In:         %s\n", (PMU.isVbusIn() ? "YES" : "NO"));
    printf("Vbus Good:       %s\n", (PMU.isVbusGood() ? "YES" : "NO"));
    printf("Charger Status:  ");
    uint8_t charge_status = PMU.getChargerStatus();
    if (charge_status == XPOWERS_AXP2101_CHG_TRI_STATE) {
        printf("tri charge");
    } else if (charge_status == XPOWERS_AXP2101_CHG_PRE_STATE) {
        printf("pre charge");
    } else if (charge_status == XPOWERS_AXP2101_CHG_CC_STATE) {
        printf("constant charge");
    } else if (charge_status == XPOWERS_AXP2101_CHG_CV_STATE) {
        printf("constant voltage");
    } else if (charge_status == XPOWERS_AXP2101_CHG_DONE_STATE) {
        printf("charge done");
    } else if (charge_status == XPOWERS_AXP2101_CHG_STOP_STATE) {
        printf("not charging");
    }
    printf("\n");

    printf("Batt Voltage:    %d mV\n", PMU.getBattVoltage()); 
    printf("Vbus Voltage:    %d mV\n", PMU.getVbusVoltage());
    printf("System Voltage:  %d mV\n", PMU.getSystemVoltage());

    // The battery percentage may be inaccurate at first use, the PMU will automatically
    // learn the battery curve and will automatically calibrate the battery percentage
    // after a charge and discharge cycle
    if (PMU.isBatteryConnect()) {
        printf("Battery Percent: %d %%\n", PMU.getBatteryPercent());
    }
    printf("Shutdown thresh: %d %%\n", PMU.getLowBatShutdownThreshold());
    
}



/****************************************************************************************
 * Read a sequence of bytes from a pmu registers
 ****************************************************************************************/

static int pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
{
    esp_err_t ret;
    if (len == 0) {
        return ESP_OK;
    }
    if (data == NULL) {
        return ESP_FAIL;
    }

    // Use i2c_master_transmit_receive for combined write-read transaction
    ret = i2c_master_transmit_receive(pmu_dev_handle, &regAddr, 1, data, len, pdMS_TO_TICKS(1000));
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PMU READ FAILED! > ");
    }
    return ret == ESP_OK ? 0 : -1;
}



/*****************************************************************************************
 * Write a byte to a pmu register
 *****************************************************************************************/

static int pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
{
    esp_err_t ret;
    if (data == NULL) {
        return ESP_FAIL;
    }

    // Prepare write buffer: register address followed by data
    uint8_t *write_buf = (uint8_t *)malloc(len + 1);
    if (write_buf == NULL) {
        ESP_LOGE(TAG, "PMU WRITE - malloc failed");
        return ESP_FAIL;
    }
    
    write_buf[0] = regAddr;
    memcpy(write_buf + 1, data, len);
    
    ret = i2c_master_transmit(pmu_dev_handle, write_buf, len + 1, pdMS_TO_TICKS(1000));
    free(write_buf);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PMU WRITE FAILED! < ");
    }
    return ret == ESP_OK ? 0 : -1;
}


}
