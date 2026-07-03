#include <soc/gpio_struct.h>
#define PIN_TO_BASEREG(pin)             (0)
#define PIN_TO_BITMASK(pin)             (pin)
#define IO_REG_TYPE uint32_t
#define IO_REG_BASE_ATTR
#define IO_REG_MASK_ATTR

static inline __attribute__((always_inline))
IO_REG_TYPE directRead(IO_REG_TYPE pin)
{
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
    return (GPIO.in.val >> pin) & 0x1;
#else // plain ESP32
    if ( pin < 32 )
        return (GPIO.in >> pin) & 0x1;
    else if ( pin < 46 )
        return (GPIO.in1.val >> (pin - 32)) & 0x1;
#endif

    return 0;
}

static inline __attribute__((always_inline))
void directWriteLow(IO_REG_TYPE pin)
{
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
    GPIO.out_w1tc.val = ((uint32_t)1 << pin);
#else // plain ESP32
    if ( pin < 32 )
        GPIO.out_w1tc = ((uint32_t)1 << pin);
    else if ( pin < 46 )
        GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
#endif
}

static inline __attribute__((always_inline))
void directWriteHigh(IO_REG_TYPE pin)
{
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
    GPIO.out_w1ts.val = ((uint32_t)1 << pin);
#else // plain ESP32
    if ( pin < 32 )
        GPIO.out_w1ts = ((uint32_t)1 << pin);
    else if ( pin < 46 )
        GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
#endif
}

static inline __attribute__((always_inline))
void directModeInput(IO_REG_TYPE pin)
{
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
    GPIO.enable_w1tc.val = ((uint32_t)1 << (pin));
#else
    if ( digitalPinIsValid(pin) )
    {
#if ESP_IDF_VERSION_MAJOR < 4      // IDF 3.x ESP32/PICO-D4
        uint32_t rtc_reg(rtc_gpio_desc[pin].reg);

        if ( rtc_reg ) // RTC pins PULL settings
        {
            ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_gpio_desc[pin].mux);
            ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_gpio_desc[pin].pullup | rtc_gpio_desc[pin].pulldown);
        }
#endif
	// Input
        if ( pin < 32 )
            GPIO.enable_w1tc = ((uint32_t)1 << pin);
        else
            GPIO.enable1_w1tc.val = ((uint32_t)1 << (pin - 32));
    }
#endif
}

static inline __attribute__((always_inline))
void directModeOutput(IO_REG_TYPE pin)
{
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
    GPIO.enable_w1ts.val = ((uint32_t)1 << (pin));
#else
    if ( digitalPinIsValid(pin) && pin <= 33 ) // pins above 33 can be only inputs
    {
#if ESP_IDF_VERSION_MAJOR < 4      // IDF 3.x ESP32/PICO-D4
        uint32_t rtc_reg(rtc_gpio_desc[pin].reg);

        if ( rtc_reg ) // RTC pins PULL settings
        {
            ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_gpio_desc[pin].mux);
            ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_gpio_desc[pin].pullup | rtc_gpio_desc[pin].pulldown);
        }
#endif
        // Output
        if ( pin < 32 )
            GPIO.enable_w1ts = ((uint32_t)1 << pin);
        else // already validated to pins <= 33
            GPIO.enable1_w1ts.val = ((uint32_t)1 << (pin - 32));
    }
#endif
}

#define DIRECT_READ(base, pin)          directRead(pin)
#define DIRECT_WRITE_LOW(base, pin)     directWriteLow(pin)
#define DIRECT_WRITE_HIGH(base, pin)    directWriteHigh(pin)
#define DIRECT_MODE_INPUT(base, pin)    directModeInput(pin)
#define DIRECT_MODE_OUTPUT(base, pin)   directModeOutput(pin)
// https://github.com/PaulStoffregen/OneWire/pull/47
// https://github.com/stickbreaker/OneWire/commit/6eb7fc1c11a15b6ac8c60e5671cf36eb6829f82c
#ifdef  interrupts
#undef  interrupts
#endif
