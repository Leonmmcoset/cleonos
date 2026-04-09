#ifndef CLKS_SERIAL_H
#define CLKS_SERIAL_H

void clks_serial_init(void);
void clks_serial_write_char(char ch);
void clks_serial_write(const char *text);

#endif