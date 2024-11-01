#include "mbed.h"
#include "stdint.h" // This allows the use of integers of a known width
#include <cstdio>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>

#define LM75_REG_TEMP (0x00)  // Temperature Register
#define LM75_REG_CONF (0x01)  // Configuration Register
#define LM75_ADDR     (0x90)  // LM75 address
#define LM75_REG_TOS  (0x03)  // TOS Register
#define LM75_REG_THYST (0x02) // THYST Register

I2C i2c(I2C_SDA, I2C_SCL);
DigitalOut myled(LED1);
DigitalOut blue(LED2);
DigitalOut red(LED3);
InterruptIn lm75_int(D7); // Ensure the OS line is connected to D7
Serial pc(SERIAL_TX, SERIAL_RX);

bool interrupted = false;
bool sent_data = false;

int16_t i16; // This variable needs to be 16 bits wide for the TOS and THYST conversion to work

void interrupt()
{
    interrupted = true;
}

int main()
{
    fflush(stdout);
    char data_write[3];
    char data_read[3];
    std::vector<float> temp_values = {};

    // Configure the Temperature sensor device STLM75:
    // Thermostat mode Interrupt, Fault tolerance: 0
    data_write[0] = LM75_REG_CONF;
    data_write[1] = 0x02;
    int status = i2c.write(LM75_ADDR, data_write, 2, 0);
    if (status != 0)
    { // Error
        while (1)
        {
            myled = !myled;
            wait(0.2);
        }
    }

    float tos = 28;  // TOS temperature
    float thyst = 26; // THYST temperature

    // Set the TOS register
    data_write[0] = LM75_REG_TOS;
    i16 = (int16_t)(tos * 256) & 0xFF80;
    data_write[1] = (i16 >> 8) & 0xff;
    data_write[2] = i16 & 0xff;
    i2c.write(LM75_ADDR, data_write, 3, 0);

    // Set the THYST register
    data_write[0] = LM75_REG_THYST;
    i16 = (int16_t)(thyst * 256) & 0xFF80;
    data_write[1] = (i16 >> 8) & 0xff;
    data_write[2] = i16 & 0xff;
    i2c.write(LM75_ADDR, data_write, 3, 0);

    // Attach the interrupt to trigger on a falling edge
    lm75_int.fall(&interrupt);

    while (1)
    {
        if (!interrupted)
        {
            data_write[0] = LM75_REG_TEMP;
            i2c.write(LM75_ADDR, data_write, 1, 1); // no stop
            i2c.read(LM75_ADDR, data_read, 2, 0);

            // Calculate temperature value in Celsius
            int16_t i16 = (data_read[0] << 8) | data_read[1];
            float temp = i16 / 256.0;

            // Display result
            pc.printf("Temperature = %.3f\r\n", temp);
            myled = !myled;
            temp_values.push_back(temp);
            if (temp_values.size() > 60)
                temp_values.erase(temp_values.begin());

            wait(1);
        }
        else
        {
            if (sent_data)
            {
                red = !red;
                wait(1);
            }
            else
            {
                for (size_t i = 0; i < temp_values.size(); i++)
                {
                    pc.printf("Temperature reading #%d: %f\r\n", i, temp_values[i]);
                }
                sent_data = true;
            }
        }
    }
}
