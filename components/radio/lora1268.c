#include "defines.h"
 
#if defined(ARCTIC4_UHF)
 
#include <inttypes.h>
#include "esp_log.h"
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>
#include "system.h"
#include "config.h"
#include "radio.h"
#include "lora1268.h"



#define BUSY_TIMEOUT  5000


#define TAG "lora"

// SX126X physical layer properties
#define XTAL_FREQ     ( double )32000000
#define FREQ_DIV      ( double )pow( 2.0, 25.0 )
#define FREQ_STEP     ( double )( XTAL_FREQ / FREQ_DIV )


static int16_t loraBegin(uint32_t frequencyInHz, int8_t txPowerInDbm, float tcxoVoltage, bool useRegulatorLDO); 
static void    reset(void);
static void    setStandby(uint8_t mode);
static void    setRegulatorMode(uint8_t mode);
static void    setOvercurrentProtection(float currentLimit);
static void    setPaConfig(uint8_t paDutyCycle, uint8_t hpMax, uint8_t deviceSel, uint8_t paLut);
static void    calibrate(uint8_t calibParam);
static void    setDio2AsRfSwitchCtrl(uint8_t enable);
static void    setPowerConfig(int8_t power, uint8_t rampTime);
static void    setStopRxTimerOnPreambleDetect(bool enable);
static void    setLoRaSymbNumTimeout(uint8_t SymbNum);
static void    setPacketType(uint8_t packetType);
static void    fixInvertedIQ(uint8_t iqConfig);
static void    setDioIrqParams( uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask );
static void    setRx(uint32_t timeout);
static void    setTx(uint32_t timeoutInMs);
static void    setStandby(uint8_t mode);
static uint8_t getStatus(void);

static void    writeCommand(uint8_t cmd, uint8_t* data, uint8_t numBytes);
static uint8_t writeCommand2(uint8_t cmd, uint8_t* data, uint8_t numBytes);
static void    readCommand(uint8_t cmd, uint8_t* data, uint8_t numBytes);
static void    waitForIdleBegin(unsigned long timeout, char *text);
static bool    waitForIdle(unsigned long timeout, char *text, bool stop);
static void    readRegister(uint16_t reg, uint8_t* data, uint8_t numBytes);
static void    writeRegister(uint16_t reg, uint8_t* data, uint8_t numBytes);
static void    writeBuffer(uint8_t *txData, int16_t txDataLen);
static void    getRxBufferStatus(uint8_t *payloadLength, uint8_t *rxStartBufferPointer);
static void    chipSelect(bool select);

extern uint8_t spi_transfer(uint8_t address);
extern void  spi_init();

static bool txActive; 
static uint8_t PacketParams[6];

static int power[7] = {-5,-2,1,4,7,10,13};

/*
 * The module used: 
 * DIO3 = PA-ENABLE
 * Level, Reg-val,  Power (dBm, mW)
 * ---------------------------------
 *  0     -5        5.8 	   4 
 *  1     -2        13.2      21
 *  2      1        19.0 	  79
 *  3      4        24.0     251
 *  4      7        26.7     468
 *  5     10        28.4     692
 *  6     13        29.5     891 
 *  7     16        29.7     933
 *  8     19        29.8     955
 *  9     22        30.2    1047
 */ 


/****************************************************************************
 *  Init 
 ****************************************************************************/

void lora_init(void)
{
	txActive = false;

	gpio_reset_pin(LORA_PIN_CS);
	gpio_set_direction(LORA_PIN_CS, GPIO_MODE_OUTPUT);
	gpio_set_level(LORA_PIN_CS, 1);

	gpio_reset_pin(LORA_PIN_RST);
	gpio_set_direction(LORA_PIN_RST, GPIO_MODE_OUTPUT);
	
	gpio_reset_pin(LORA_PIN_BUSY);
	gpio_set_direction(LORA_PIN_BUSY, GPIO_MODE_INPUT);
	
	gpio_reset_pin(RADIO_PIN_PWRON);
	gpio_set_direction(RADIO_PIN_PWRON, GPIO_MODE_OUTPUT);
	gpio_set_level(LORA_PIN_CS, 0);		
	
	gpio_reset_pin(LORA_PIN_DIO1);
	gpio_set_direction(LORA_PIN_DIO1, GPIO_MODE_INPUT);
	gpio_set_pull_mode(LORA_PIN_DIO1, GPIO_PULLDOWN_ONLY);
	
	gpio_reset_pin(LORA_PIN_DIO3);
	gpio_set_direction(LORA_PIN_DIO3, GPIO_MODE_OUTPUT);
	gpio_set_level(LORA_PIN_DIO3, 0);	
    spi_init();  
}


static bool _on;
static int _count = 0;

void lora_on(bool on)
{
	gpio_set_level(RADIO_PIN_PWRON, (on ? 1 : 0));
	sleepMs(10);
	if (!_on && on) {
		uint8_t sf = get_byte_param("LORA_SF", DFL_LORA_SF);
		uint8_t cr = get_byte_param("LORA_CR", DFL_LORA_CR);
		int32_t freq = get_i32_param("FREQ", DFL_FREQ); 
		uint8_t txpo = get_byte_param("TXPOWER", DFL_TXPOWER);
	
		ESP_LOGI(TAG, "Lora on: sf=%x, cr=%x", sf, (cr-4));
		loraBegin((uint32_t) freq, power[txpo], 0, false );
		lora_config(sf, SX126X_LORA_BW_125_0, cr-4, 8, 0, true, false, (sf>=11 ? 1:0)); 
	     	// SF, BW, CR, PAlength, PLlen, CRCon, invertIRQ, optimize
	}
	_on = on;
}


bool lora_is_on() {
	return _on;
}



/******************************************************
 * Need radio - turn it on if not already on
 ******************************************************/
 
void lora_require(void)
{
    if (++_count == 1) {
        lora_on(true);   
        ESP_LOGI(TAG, "Radio is turned ON");
    }
}


 
/*******************************************************
 * Radio not needed any more - turn it off if no others
 * need it
 *******************************************************/
 
void lora_release(void)
{
    if (--_count <= 0) {
       sleepMs(60);
       lora_on(false);
       ESP_LOGI(TAG, "Radio is turned OFF");
    }
    if (_count < 0) _count = 0;
}


/****************************************************************************
 *  Set some parameters: 
 *  	spreadingfactor, bandwidth, coding-rate, preample-len, payload-len, 
 *      crc-on, invert-irq
 ****************************************************************************/

void lora_config(uint8_t spreadingFactor, uint8_t bandwidth, uint8_t codingRate, 
	uint16_t preambleLength, uint8_t payloadLen, bool crcOn, bool invertIrq, uint8_t ldro) 
{		 
	setStopRxTimerOnPreambleDetect(false);
	setLoRaSymbNumTimeout(0); 
	setPacketType(SX126X_PACKET_TYPE_LORA); // SX126x.ModulationParams.PacketType : MODEM_LORA
	lora_SetModulationParams(spreadingFactor, bandwidth, codingRate, ldro);
	
	PacketParams[0] = (preambleLength >> 8) & 0xFF;
	PacketParams[1] = preambleLength;
	if ( payloadLen ) {
		PacketParams[2] = 0x01; // Fixed length packet (implicit header)
		PacketParams[3] = payloadLen;
	}
	else {
		PacketParams[2] = 0x00; // Variable length packet (explicit header)
		PacketParams[3] = 0xFF;
	}
	
	if ( crcOn )
		PacketParams[4] = SX126X_LORA_IQ_INVERTED;
	else
		PacketParams[4] = SX126X_LORA_IQ_STANDARD;

	if ( invertIrq )
		PacketParams[5] = 0x01; // Inverted LoRa I and Q signals setup
	else
		PacketParams[5] = 0x00; // Standard LoRa I and Q signals setup
	
	// fixes IQ configuration for inverted IQ
	fixInvertedIQ(PacketParams[5]);
	writeCommand(SX126X_CMD_SET_PACKET_PARAMS, PacketParams, 6); // 0x8C
	
	/* 
	 * Set up Interrupts. On Arctic Tracker 4, DIO1 is available. 
	 * DIO3 is used for enabling the PA.
	 */
	setDioIrqParams(SX126X_IRQ_ALL,      //all interrupts enabled
					SX126X_IRQ_NONE,     //interrupts on DIO1
					SX126X_IRQ_NONE,     //interrupts on DIO2
					SX126X_IRQ_NONE);    //interrupts on DIO3
	
	// Receive state no receive timeoout
	setRx(0xFFFFFF);
}




void lora_setTxPower(uint8_t level) {
	if (level > 6)
		level = 6;
	setPowerConfig(power[level], SX126X_PA_RAMP_200U);
}


/****************************************************************************
 *  Set RF frequency
 ****************************************************************************/

void lora_SetRfFrequency(uint32_t frequency)
{
	uint8_t buf[4];
	uint32_t freq = 0;
	uint8_t calFreq[2];

	/* Image rejection for 433 MHz band */
	calFreq[0] = 0x6B;
	calFreq[1] = 0x6F;
	writeCommand(SX126X_CMD_CALIBRATE_IMAGE, calFreq, 2);
	
	/* Set frequency */
	freq = (uint32_t)((double)frequency / (double)FREQ_STEP);
	buf[0] = (uint8_t)((freq >> 24) & 0xFF);
	buf[1] = (uint8_t)((freq >> 16) & 0xFF);
	buf[2] = (uint8_t)((freq >> 8) & 0xFF);
	buf[3] = (uint8_t)(freq & 0xFF);
	writeCommand(SX126X_CMD_SET_RF_FREQUENCY, buf, 4); // 0x86
}


/****************************************************************************
 *  Set Modulation Parameters
 ****************************************************************************/

void lora_SetModulationParams(uint8_t spreadingFactor, uint8_t bandwidth, uint8_t codingRate, uint8_t ldro)
{
	if (codingRate>8) {
		ESP_LOGE(TAG, "Lora CR setting out of range: %d", codingRate); 
		codingRate = 1;
	}
	if (spreadingFactor>12 || spreadingFactor < 8) {
		ESP_LOGE(TAG, "Lora SF setting out of range: %d", codingRate);
		spreadingFactor = 12;
	}
	
	uint8_t data[4];
	//currently only LoRa supported
	data[0] = spreadingFactor;
	data[1] = bandwidth;
	data[2] = codingRate;
	data[3] = ldro;
	writeCommand(SX126X_CMD_SET_MODULATION_PARAMS, data, 4); // 0x8B
}


/****************************************************************************
 * Receive lora packet and clear IRQ status. Return number of bytes received
 * 0 if no packet is received. To be used in ISR. 
 ****************************************************************************/

uint8_t lora_ReceivePacket(uint8_t *pData, int16_t len) 
{
	uint8_t rxLen = 0;
	uint16_t irqRegs = lora_GetIrqStatus();

	if( irqRegs & SX126X_IRQ_RX_DONE ) {
		if (irqRegs & (SX126X_IRQ_CRC_ERR | SX126X_IRQ_HEADER_ERR)) {
			ESP_LOGI(TAG, "CRC error");
			return 0;
		}
		rxLen = lora_ReadBuffer(pData, len);
	}
	return rxLen;
}



/****************************************************************************
 * Send lora packet
 ****************************************************************************/

void lora_SendPacket(uint8_t *pData, int16_t len)
{
	PacketParams[2] = 0x00; //Variable length packet (explicit header)
	PacketParams[3] = len;
	writeCommand(SX126X_CMD_SET_PACKET_PARAMS, PacketParams, 6); // 0x8C
	lora_ClearIrqStatus(SX126X_IRQ_ALL);
	lora_WriteBuffer(pData, len);
	gpio_set_level(LORA_PIN_DIO3, 1);
	setTx(500);
}


/****************************************************************************
 * Turn transmitter off and go back to RX mode
 ****************************************************************************/

void lora_TxOff() {
	setRx(0xFFFFFF);
	gpio_set_level(LORA_PIN_DIO3, 0);
}

/****************************************************************************
 * Enable an interrupt and set ISR for DIO1 events
 * mask can be one of these or a combination: 
 * 
 *     SX126X_IRQ_TIMEOUT             Rx or Tx timeout
 *     SX126X_IRQ_CAD_DETECTED        channel activity detected
 *     SX126X_IRQ_CAD_DONE            channel activity detection finished
 *     SX126X_IRQ_CRC_ERR             wrong CRC received
 *     SX126X_IRQ_HEADER_ERR          LoRa header CRC error
 *     SX126X_IRQ_HEADER_VALID        valid LoRa header received
 *     SX126X_IRQ_SYNC_WORD_VALID     valid sync word detected
 *     SX126X_IRQ_PREAMBLE_DETECTED   preamble detected
 *     SX126X_IRQ_RX_DONE             packet received
 *     SX126X_IRQ_TX_DONE             packet transmission completed
 ****************************************************************************/

void lora_SetIrqHandler(gpio_isr_t handler, uint16_t  mask) 
{
	gpio_set_intr_type(LORA_PIN_DIO1, GPIO_INTR_POSEDGE);
	gpio_isr_handler_add(LORA_PIN_DIO1, handler, NULL);
	gpio_intr_enable(LORA_PIN_DIO1);
	
	setDioIrqParams(
		SX126X_IRQ_ALL, 
		mask,  
		SX126X_IRQ_NONE, SX126X_IRQ_NONE);  
}


/****************************************************************************
 * get interrupt status. Returns a combination of the bits in the mask
 * described above. 
 ****************************************************************************/

uint16_t lora_GetIrqStatus( void )
{
	uint8_t data[3];
	readCommand(SX126X_CMD_GET_IRQ_STATUS, data, 3); // 0x12
	return (data[1] << 8) | data[2];
}


/****************************************************************************
 * clear interrupt status. Clears all. 
 ****************************************************************************/

void lora_ClearIrqStatus()
{
	uint8_t buf[2];

	buf[0] = (uint8_t)(((uint16_t)SX126X_IRQ_ALL >> 8) & 0x00FF);
	buf[1] = (uint8_t)((uint16_t)SX126X_IRQ_ALL & 0x00FF);
	writeCommand(SX126X_CMD_CLEAR_IRQ_STATUS, buf, 2); // 0x02
}


/****************************************************************************
 *  Get theinstantaneous RSSI value during reception of the packet. The 
 *  command is valid for all protocols.
 *  Returns Signal power in dBm = –RssiInst/2 (dBm)
 ****************************************************************************/

uint8_t lora_GetRssiInst()
{
	uint8_t buf[2];
	readCommand( SX126X_CMD_GET_RSSI_INST, buf, 2 ); // 0x15
	return buf[1];
}


/****************************************************************************
 *  GetPacketStatus
 *  Returns RSSI and SNR of the last received packet (for LoRa) 
 ****************************************************************************/

void lora_GetPacketStatus(int8_t *rssiPacket, int8_t *snrPacket)
{
	uint8_t buf[4];
	readCommand( SX126X_CMD_GET_PACKET_STATUS, buf, 4 ); // 0x14
	*rssiPacket = (buf[3] >> 1) * -1;
	( buf[2] < 128 ) ? ( *snrPacket = buf[2] >> 2 ) : ( *snrPacket = ( ( buf[2] - 256 ) >> 2 ) );
}


/****************************************************************************
 * GetRxBufferStatus
 * Returns the length of the last received packet (PayloadLengthRx) and the 
 * address of the first byte received (RxStartBufferPointer). 
 ****************************************************************************/

void lora_GetRxBufferStatus(uint8_t *payloadLength, uint8_t *rxStartBufferPointer)
{
	uint8_t buf[3];
	readCommand( SX126X_CMD_GET_RX_BUFFER_STATUS, buf, 3 ); // 0x13
	*payloadLength = buf[1];
	*rxStartBufferPointer = buf[2];
}


/****************************************************************************
 *  SetBufferBaseAddresa
 *  Set RX and TX buffer base addr in 256 bytes internal buffer
 *  Default 0 and 0
 ****************************************************************************/

void lora_SetBufferAddr(uint8_t txBaseAddress, uint8_t rxBaseAddress)
{
	uint8_t buf[2];

	buf[0] = txBaseAddress;
	buf[1] = rxBaseAddress;
	writeCommand(SX126X_CMD_SET_BUFFER_BASE_ADDRESS, buf, 2); // 0x8F
}



/****************************************************************************
 *  Read a buffer
 ****************************************************************************/

uint8_t lora_ReadBuffer(uint8_t *rxData, int16_t rxDataLen)
{
	uint8_t offset = 0;
	uint8_t payloadLength = 0;
	lora_GetRxBufferStatus(&payloadLength, &offset);
	if( payloadLength > rxDataLen )
	{
		ESP_LOGW(TAG, "ReadBuffer rxDataLen too small. payloadLength=%d rxDataLen=%d", payloadLength, rxDataLen);
		return 0;
	}

	// ensure BUSY is low (state meachine ready)
	waitForIdle(BUSY_TIMEOUT, "start ReadBuffer", true);

	// start transfer
	chipSelect(true); 
	spi_transfer(SX126X_CMD_READ_BUFFER); // 0x1E
	spi_transfer(offset);
	spi_transfer(SX126X_CMD_NOP);
	for( int i = 0; i < payloadLength; i++ )
		rxData[i] = spi_transfer(SX126X_CMD_NOP);  

	// stop transfer
	chipSelect(false);

	// wait for BUSY to go low
	waitForIdle(BUSY_TIMEOUT, "end ReadBuffer", false);
	return payloadLength;
}



/****************************************************************************
 *  Write a buffer
 ****************************************************************************/

void lora_WriteBuffer(uint8_t *txData, int16_t txDataLen)
{
	// ensure BUSY is low (state meachine ready)
	waitForIdle(BUSY_TIMEOUT, "start WriteBuffer", true);

	// start transfer
	chipSelect(true);

	spi_transfer(SX126X_CMD_WRITE_BUFFER); // 0x0E
	spi_transfer(0); //offset in tx fifo
	for( int i = 0; i < txDataLen; i++ )
		 spi_transfer( txData[i]);	
	
	// stop transfer
	chipSelect(false);

	// wait for BUSY to go low
	waitForIdle(BUSY_TIMEOUT, "end WriteBuffer", false);
}
 


/****************************************************************************
 *  Start chip and do basic setup
 ****************************************************************************/

static int16_t loraBegin(uint32_t frequencyInHz, int8_t txPowerInDbm, float tcxoVoltage, bool useRegulatorLDO) 
{
	if ( txPowerInDbm > 22 )
		txPowerInDbm = 22;
	if ( txPowerInDbm < -3 )
		txPowerInDbm = -3;
	
	reset();
	ESP_LOGI(TAG, "Reset");
	
	uint8_t wk[2];
	readRegister(SX126X_REG_LORA_SYNC_WORD_MSB, wk, 2); // 0x0740
	uint16_t syncWord = (wk[0] << 8) + wk[1];
	ESP_LOGI(TAG, "syncWord = 0x%x", syncWord);
	if (syncWord != SX126X_SYNC_WORD_PUBLIC && syncWord != SX126X_SYNC_WORD_PRIVATE) {
		ESP_LOGE(TAG, "SX126x error, maybe no SPI connection");
		return ERR_INVALID_MODE;
	}

	ESP_LOGI(TAG, "SX126x installed");
	setStandby(SX126X_STANDBY_RC);
	setDio2AsRfSwitchCtrl(true);  // Is this correct?
	
	calibrate(	SX126X_CALIBRATE_IMAGE_ON
				| SX126X_CALIBRATE_ADC_BULK_P_ON
				| SX126X_CALIBRATE_ADC_BULK_N_ON
				| SX126X_CALIBRATE_ADC_PULSE_ON
				| SX126X_CALIBRATE_PLL_ON
				| SX126X_CALIBRATE_RC13M_ON
				| SX126X_CALIBRATE_RC64K_ON
				);
	
	/* Oppsett av regulator LDO or DCDC. Er dette støttet av modulen??  */
	ESP_LOGI(TAG, "useRegulatorLDO = %d", useRegulatorLDO);
	if (useRegulatorLDO) {
		setRegulatorMode(SX126X_REGULATOR_LDO);
	} else {
		setRegulatorMode(SX126X_REGULATOR_DC_DC);
	}
	
//	setPaConfig(0x04, 0x07, 0x00, 0x01); // PA Optimal Settings +22 dBm
	setPaConfig(0x04, 0x06, 0x00, 0x01); // PA Optimal Settings +14 dBm
	setOvercurrentProtection(60.0); 
	setPowerConfig(txPowerInDbm, SX126X_PA_RAMP_200U);
	lora_SetRfFrequency(frequencyInHz);
	return ERR_NONE;
}




/****************************************************************************
 *  Set it to RX mode
 ****************************************************************************/

static void setRx(uint32_t timeout)
{
	setStandby(SX126X_STANDBY_RC);
	uint8_t buf[3];
	buf[0] = (uint8_t)((timeout >> 16) & 0xFF);
	buf[1] = (uint8_t)((timeout >> 8) & 0xFF);
	buf[2] = (uint8_t)(timeout & 0xFF);
	writeCommand(SX126X_CMD_SET_RX, buf, 3); // 0x82

	for(int retry=0; retry<10; retry++) {
		if ((getStatus() & 0x70) == 0x50) break;
		sleepMs(1);
	}
	if ((getStatus() & 0x70) != 0x50) {
		ESP_LOGE(TAG, "SetRx Illegal Status");
//		LoRaError(ERR_INVALID_SETRX_STATE);
	}
}


/****************************************************************************
 *  Set it to TX mode
 ****************************************************************************/

static void setTx(uint32_t timeoutInMs)
{
	setStandby(SX126X_STANDBY_RC);
	uint8_t buf[3];
	uint32_t tout = timeoutInMs;
	if (timeoutInMs != 0) {
		uint32_t timeoutInUs = timeoutInMs * 1000;
		tout = (uint32_t)(timeoutInUs / 0.015625);
	}

	buf[0] = (uint8_t)((tout >> 16) & 0xFF);
	buf[1] = (uint8_t)((tout >> 8) & 0xFF);
	buf[2] = (uint8_t )(tout & 0xFF);
	writeCommand(SX126X_CMD_SET_TX, buf, 3); // 0x83
	
	for(int retry=0; retry<10; retry++) {
		if ((getStatus() & 0x70) == 0x60) break;
		sleepMs(1);
	}
	if ((getStatus() & 0x70) != 0x60) {
		ESP_LOGE(TAG, "SetTx Illegal Status");
//		LoRaError(ERR_INVALID_SETTX_STATE);
	}
}



/****************************************************************************
 *  Get status
 ****************************************************************************/

static uint8_t getStatus(void)
{
	uint8_t rv;
	readCommand(SX126X_CMD_GET_STATUS, &rv, 1); // 0xC0
	return rv;
}


/****************************************************************************
 *  Configure DIO interrupts
 ****************************************************************************/

static void setDioIrqParams
( uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask )
{
	uint8_t buf[8];

	buf[0] = (uint8_t)((irqMask >> 8) & 0x00FF);
	buf[1] = (uint8_t)(irqMask & 0x00FF);
	buf[2] = (uint8_t)((dio1Mask >> 8) & 0x00FF);
	buf[3] = (uint8_t)(dio1Mask & 0x00FF);
	buf[4] = (uint8_t)((dio2Mask >> 8) & 0x00FF);
	buf[5] = (uint8_t)(dio2Mask & 0x00FF);
	buf[6] = (uint8_t)((dio3Mask >> 8) & 0x00FF);
	buf[7] = (uint8_t)(dio3Mask & 0x00FF);
	writeCommand(SX126X_CMD_SET_DIO_IRQ_PARAMS, buf, 8); // 0x08
}


/****************************************************************************
 *  fixes IQ configuration for inverted IQ
 ****************************************************************************/

static void fixInvertedIQ(uint8_t iqConfig)
{
	/* 
	 * see SX1262/SX1268 datasheet, chapter 15 Known Limitations, section 15.4 for details
	 * When exchanging LoRa packets with inverted IQ polarity, some packet losses may be observed for longer packets.
	 * Workaround: Bit 2 at address 0x0736 must be set to:
	 * “0” when using inverted IQ polarity (see the SetPacketParam(...) command)
	 * “1” when using standard IQ polarity
	 */
	
	uint8_t iqConfigCurrent = 0;
	readRegister(SX126X_REG_IQ_POLARITY_SETUP, &iqConfigCurrent, 1); // 0x0736

	// set correct IQ configuration
	//if(iqConfig == SX126X_LORA_IQ_STANDARD) {
	if(iqConfig == SX126X_LORA_IQ_INVERTED) {
		iqConfigCurrent &= 0xFB; // using inverted IQ polarity
	} else {
		iqConfigCurrent |= 0x04; // using standard IQ polarity
	}

	// update with the new value
	writeRegister(SX126X_REG_IQ_POLARITY_SETUP, &iqConfigCurrent, 1); // 0x0736
}


/****************************************************************************
 *  Set Packet Type
 ****************************************************************************/

static void setPacketType(uint8_t packetType)
{
	uint8_t data = packetType;
	writeCommand(SX126X_CMD_SET_PACKET_TYPE, &data, 1); // 0x01
}


/****************************************************************************
 *  SetLoRaSymbNumTimeout
 ****************************************************************************/

static void setLoRaSymbNumTimeout(uint8_t SymbNum)
{
	uint8_t data = SymbNum;
	writeCommand(SX126X_CMD_SET_LORA_SYMB_NUM_TIMEOUT, &data, 1); // 0xA0
}


/****************************************************************************
 *  SetStopRxTimerOnPreambleDetect
 ****************************************************************************/

static void setStopRxTimerOnPreambleDetect(bool enable)
{
	ESP_LOGI(TAG, "SetStopRxTimerOnPreambleDetect enable = %d", enable);
	//uint8_t data = (uint8_t)enable;
	uint8_t data = 0;
	if (enable) data = 1;
	writeCommand(SX126X_CMD_STOP_TIMER_ON_PREAMBLE, &data, 1); // 0x9F
}



/****************************************************************************
 *  Set TX power configuration
 ****************************************************************************/

static void setPowerConfig(int8_t power, uint8_t rampTime)
{
	uint8_t buf[2];

	if( power > 22 )
		power = 22;
	else if( power < -5 )
		power = -5;
	
	buf[0] = power;
	buf[1] = ( uint8_t )rampTime;
	writeCommand(SX126X_CMD_SET_TX_PARAMS, buf, 2); // 0x8E
}


/****************************************************************************
 *  Calibrate
 ****************************************************************************/

static void calibrate(uint8_t calibParam)
{
	uint8_t data = calibParam;
	writeCommand(SX126X_CMD_CALIBRATE, &data, 1); // 0x89
}


/****************************************************************************
 *  Is this used for this module?
 ****************************************************************************/

static void setDio2AsRfSwitchCtrl(uint8_t enable)
{
	uint8_t data = enable;
	writeCommand(SX126X_CMD_SET_DIO2_AS_RF_SWITCH_CTRL, &data, 1); // 0x9D
}


/****************************************************************************
 *  Set PA config
 ****************************************************************************/

static void setPaConfig(uint8_t paDutyCycle, uint8_t hpMax, uint8_t deviceSel, uint8_t paLut)
{
	uint8_t buf[4];

	buf[0] = paDutyCycle;
	buf[1] = hpMax;
	buf[2] = deviceSel;
	buf[3] = paLut;
	writeCommand(SX126X_CMD_SET_PA_CONFIG, buf, 4); // 0x95
}


/****************************************************************************
 *  Set Over current protecction
 ****************************************************************************/

static void setOvercurrentProtection(float currentLimit)
{
	if((currentLimit >= 0.0) && (currentLimit <= 140.0)) {
		uint8_t buf[1];
		buf[0] = (uint8_t)(currentLimit / 2.5);
		writeRegister(SX126X_REG_OCP_CONFIGURATION, buf, 1); // 0x08E7
	}
}


/****************************************************************************
 *  Reset the chip
 ****************************************************************************/

static void reset(void)
{
	sleepMs(10);
	gpio_set_level(LORA_PIN_RST,0);
	sleepMs(20);
	gpio_set_level(LORA_PIN_RST,1);
	sleepMs(10);
	// ensure BUSY is low (state meachine ready)
	waitForIdle(BUSY_TIMEOUT, "Reset", true);
}



/****************************************************************************
 *  Standby mode
 ****************************************************************************/

static void setStandby(uint8_t mode)
{
	uint8_t data = mode;
	writeCommand(SX126X_CMD_SET_STANDBY, &data, 1); // 0x80
}


/****************************************************************************
 *  Regulator mode: LDO or DCDC
 ****************************************************************************/

static void setRegulatorMode(uint8_t mode)
{
	uint8_t data = mode;
	writeCommand(SX126X_CMD_SET_REGULATOR_MODE, &data, 1); // 0x96
}



/****************************************************************************
 *  Select pin 
 ****************************************************************************/

static void chipSelect(bool select) {
	gpio_set_level(LORA_PIN_CS, (select? LOW : HIGH));
}



/****************************************************************************
 *  Wait until chip is ready
 ****************************************************************************/

static void waitForIdleBegin(unsigned long timeout, char *text) {
	// ensure BUSY is low (state meachine ready)
	bool stop = false;
	for (int retry=0;retry<10;retry++) {
		if (retry == 9) stop = true;
		bool ret = waitForIdle(BUSY_TIMEOUT, text, stop);
		if (ret == true) break;
		ESP_LOGW(TAG, "WaitForIdle retry=%d", retry);
		sleepMs(1);
	}
}

static bool waitForIdle(unsigned long timeout, char *text, bool stop)
{
	bool ret = true;
	TickType_t start = xTaskGetTickCount();
	sleepUs(1);
	while(xTaskGetTickCount() - start < (timeout/portTICK_PERIOD_MS)) {
		if (gpio_get_level(LORA_PIN_BUSY) == 0) break;
		sleepUs(1);
	}
	if (gpio_get_level(LORA_PIN_BUSY)) {
        ESP_LOGE(TAG, "WaitForIdle Timeout text=%s timeout=%lu start=%"PRIu32, text, timeout, start);
		if (stop) 
  		   ; // LoRaError(ERR_IDLE_TIMEOUT);
		else 
			ret = false;
		
	}
	return ret;
}



/****************************************************************************
 *  Write a register
 ****************************************************************************/

static void writeRegister(uint16_t reg, uint8_t* data, uint8_t numBytes) {
	// ensure BUSY is low (state meachine ready)
	waitForIdle(BUSY_TIMEOUT, "start WriteRegister", true);
    ESP_LOGD(TAG, "WriteRegister: REG=0x%02x", reg);

    // start transfer
	chipSelect(true);

	// send command byte
	spi_transfer(SX126X_CMD_WRITE_REGISTER); // 0x0D
	spi_transfer((reg & 0xFF00) >> 8);
	spi_transfer(reg & 0xff);
	
	for(uint8_t n = 0; n < numBytes; n++) {
		uint8_t in = spi_transfer(data[n]);
		(void)in;
        ESP_LOGD(TAG, "%02x --> %02x", data[n], in);
	}

	// stop transfer
	chipSelect(false);

	// wait for BUSY to go low
	waitForIdle(BUSY_TIMEOUT, "end WriteRegister", false);
}


/****************************************************************************
 *  Read a register
 ****************************************************************************/

static void readRegister(uint16_t reg, uint8_t* data, uint8_t numBytes) {
	// ensure BUSY is low (state meachine ready)
	waitForIdle(BUSY_TIMEOUT, "start ReadRegister", true);

    ESP_LOGD(TAG, "ReadRegister: REG=0x%02x", reg);

	// start transfer
	chipSelect(true);

	// send command byte
	spi_transfer(SX126X_CMD_READ_REGISTER); // 0x1D
	spi_transfer((reg & 0xFF00) >> 8);
	spi_transfer(reg & 0xff);
	spi_transfer(SX126X_CMD_NOP);

	for(uint8_t n = 0; n < numBytes; n++) {
		data[n] = spi_transfer(SX126X_CMD_NOP);
        ESP_LOGD(TAG, "DataIn:%02x ", data[n]);
	}

	// stop transfer
	chipSelect(false);

	// wait for BUSY to go low
	waitForIdle(BUSY_TIMEOUT, "end ReadRegister", false);
}


/****************************************************************************
 *  Write Command with retry
 ****************************************************************************/

static void writeCommand(uint8_t cmd, uint8_t* data, uint8_t numBytes) {
	uint8_t status;
	for (int retry=1; retry<10; retry++) {
		status = writeCommand2(cmd, data, numBytes);
		ESP_LOGD(TAG, "status=%02x", status);
		if (status == 0) break;
		ESP_LOGW(TAG, "writeCommand2 status=%02x retry=%d", status, retry);
	}
	if (status != 0) {
		ESP_LOGE(TAG, "SPI Transaction error:0x%02x", status);
//		LoRaError(ERR_SPI_TRANSACTION);
	}
}


/****************************************************************************
 *  Write Command 
 ****************************************************************************/

static uint8_t writeCommand2(uint8_t cmd, uint8_t* data, uint8_t numBytes) {
	// ensure BUSY is low (state meachine ready)
	waitForIdle(BUSY_TIMEOUT, "start WriteCommand2", true);

	// start transfer
	chipSelect(true);

	// send command byte
    ESP_LOGD(TAG, "WriteCommand: CMD=0x%02x", cmd);
	spi_transfer(cmd);

	// variable to save error during SPI transfer
	uint8_t status = 0;

	// send/receive all bytes
	for(uint8_t n = 0; n < numBytes; n++) {
		uint8_t in = spi_transfer(data[n]);
        ESP_LOGD(TAG, "%02x --> %02x", data[n], in);

		// check status
		if(((in & 0b00001110) == SX126X_STATUS_CMD_TIMEOUT) ||
 		  (( in & 0b00001110) == SX126X_STATUS_CMD_INVALID) ||
		  (( in & 0b00001110) == SX126X_STATUS_CMD_FAILED)) {
			status = in & 0b00001110;
			break;
		} else if(in == 0x00 || in == 0xFF) {
			status = SX126X_STATUS_SPI_FAILED;
			break;
		}
	} 

	// stop transfer
	chipSelect(false);
	
	// wait for BUSY to go low
	waitForIdle(BUSY_TIMEOUT, "end WriteCommand2", false);
	return status;
}



/****************************************************************************
 *  read command
 ****************************************************************************/

static void readCommand(uint8_t cmd, uint8_t* data, uint8_t numBytes) {
	// ensure BUSY is low (state meachine ready)
	//WaitForIdle(BUSY_WAIT, "start ReadCommand", true);
	waitForIdleBegin(BUSY_TIMEOUT, "start ReadCommand");

	// start transfer
	chipSelect(true);

	// send command byte
	ESP_LOGD(TAG, "ReadCommand: CMD=0x%02x", cmd);
	spi_transfer(cmd);

	// send/receive all bytes
	for(uint8_t n = 0; n < numBytes; n++) {
		data[n] = spi_transfer(SX126X_CMD_NOP);
        ESP_LOGD(TAG, "DataIn: %02x", data[n]);
	}

	// stop transfer
	chipSelect(false);

	// wait for BUSY to go low
	sleepMs(1);
	waitForIdle(BUSY_TIMEOUT, "end ReadCommand", false);
}



#endif
