/*************************************************** 
  This is a library for our Adafruit 16-channel PWM & Servo driver

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/products/815

  These displays use I2C to communicate, 2 pins are required to  
  interface.

  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ****************************************************/

#include "PCA9685.h"

// Set to true to print some debug messages, or false to disable them.
//#define ENABLE_DEBUG_OUTPUT

/**************************************************************************/
/*! 
    @brief  Instantiates a new PCA9685 PWM driver chip with the I2C address on a TwoWire interface
    @param  i2c  A pointer to a 'Wire' compatible object that we'll use to communicate with
    @param  addr The 7-bit I2C address to locate this chip, default is 0x40
*/
/**************************************************************************/
PCA9685::PCA9685(uint8_t addr, TwoWire *i2cBus) :
    _i2caddr(addr),
    _i2cBus(i2cBus) {
}

/**************************************************************************/
/*! 
    @brief  Setups the I2C interface and hardware
*/
/**************************************************************************/
bool PCA9685::begin() {
  if (!reset()) {
    return false;
  }
  // set a default frequency
  return setPWMFreq(1000);
}


/**************************************************************************/
/*! 
    @brief  Sends a reset command to the PCA9685 chip over I2C
*/
/**************************************************************************/
bool PCA9685::reset() {
  if (!write8(PCA9685_MODE1, 0x80)) {
    return false;
  }
  delay(10);
  return true;
}

/**************************************************************************/
/*! 
    @brief  Sets the PWM frequency for the entire chip, up to ~1.6 KHz
    @param  freq Floating point frequency that we will attempt to match
*/
/**************************************************************************/
bool PCA9685::setPWMFreq(float freq) {
#ifdef ENABLE_DEBUG_OUTPUT
  Serial.print("Attempting to set freq ");
  Serial.println(freq);
#endif

  freq *= 0.9;  // Correct for overshoot in the frequency setting (see issue #11).
  float prescaleval = 25000000;
  prescaleval /= 4096;
  prescaleval /= freq;
  prescaleval -= 1;

#ifdef ENABLE_DEBUG_OUTPUT
  Serial.print("Estimated pre-scale: "); Serial.println(prescaleval);
#endif

  uint8_t prescale = floor(prescaleval + 0.5);
#ifdef ENABLE_DEBUG_OUTPUT
  Serial.print("Final pre-scale: "); Serial.println(prescale);
#endif
  
  uint8_t oldmode = 0;
  if (!read8(PCA9685_MODE1, &oldmode)) {
    return false;
  }
  uint8_t newmode = (oldmode&0x7F) | 0x10; // sleep
  if (!write8(PCA9685_MODE1, newmode)) { // go to sleep
    return false;
  }
  if (!write8(PCA9685_PRESCALE, prescale)) { // set the prescaler
    return false;
  }
  if (!write8(PCA9685_MODE1, oldmode)) {
    return false;
  }
  delay(5);
  if (!write8(PCA9685_MODE1, oldmode | 0xa0)) {  //  This sets the MODE1 register to turn on auto increment.
    return false;
  }

#ifdef ENABLE_DEBUG_OUTPUT
  Serial.print("Mode now 0x");
  uint8_t value = 0;
  read8(PCA9685_MODE1, &value);
  Serial.println(value, HEX);
#endif
  return true;
}

/**************************************************************************/
/*! 
    @brief  Sets the PWM output of one of the PCA9685 pins
    @param  num One of the PWM output pins, from 0 to 15
    @param  on At what point in the 4096-part cycle to turn the PWM output ON
    @param  off At what point in the 4096-part cycle to turn the PWM output OFF
*/
/**************************************************************************/
bool PCA9685::setPWM(uint8_t num, uint16_t on, uint16_t off) {
#ifdef ENABLE_DEBUG_OUTPUT
  Serial.print("Setting PWM "); Serial.print(num); Serial.print(": "); Serial.print(on); Serial.print("->"); Serial.println(off);
#endif

  _i2cBus->beginTransmission(_i2caddr);
  _i2cBus->write(LED0_ON_L+4*num);
  _i2cBus->write(on);
  _i2cBus->write(on>>8);
  _i2cBus->write(off);
  _i2cBus->write(off>>8);
  return _i2cBus->endTransmission() == 0;
}

/**************************************************************************/
/*! 
    @brief  Helper to set pin PWM output. Sets pin without having to deal with on/off tick placement and properly handles a zero value as completely off and 4095 as completely on.  Optional invert parameter supports inverting the pulse for sinking to ground.
    @param  num One of the PWM output pins, from 0 to 15
    @param  val The number of ticks out of 4096 to be active, should be a value from 0 to 4095 inclusive.
    @param  invert If true, inverts the output, defaults to 'false'
*/
/**************************************************************************/
bool PCA9685::setPin(uint8_t num, uint16_t val, bool invert)
{
  // Clamp value between 0 and 4095 inclusive.
  val = min(val, (uint16_t)4095);
  uint16_t on = 0;
  uint16_t off = 0;
  if (invert) {
    if (val == 0) {
      // Special value for signal fully on.
      on = 4096;
    } else if (val == 4095) {
      // Special value for signal fully off.
      off = 4096;
    } else {
      off = 4095 - val;
    }
  }
  else {
    if (val == 4095) {
      // Special value for signal fully on.
      on = 4096;
    } else if (val == 0) {
      // Special value for signal fully off.
      off = 4096;
    } else {
      off = val;
    }
  }
  return setPWM(num, on, off);
}

/*******************************************************************************************/

bool PCA9685::read8(uint8_t addr, uint8_t *returnedValue) {
  _i2cBus->beginTransmission(_i2caddr);
  _i2cBus->write(addr);
  if (_i2cBus->endTransmission(false) != 0) {
    return false;
  }

  _i2cBus->requestFrom((uint8_t)_i2caddr, (uint8_t)1);
  if (_i2cBus->available() == 0) {
    return false;
  }
  uint8_t readValue = _i2cBus->read();
  if (returnedValue) {
    *returnedValue = readValue;
  }
  return true;
}

bool PCA9685::write8(uint8_t addr, uint8_t d) {
  _i2cBus->beginTransmission(_i2caddr);
  _i2cBus->write(addr);
  _i2cBus->write(d);
  return _i2cBus->endTransmission() == 0;
}