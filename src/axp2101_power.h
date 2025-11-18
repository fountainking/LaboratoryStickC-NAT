#ifndef M5_POWER_H
#define M5_POWER_H

// M5StickC Plus2 has NO PMIC - uses GPIO 4 HOLD pin to stay powered on
#define M5_POWER_HOLD_PIN 4

void m5_power_init(void);
void m5_power_off(void);

#endif // M5_POWER_H
