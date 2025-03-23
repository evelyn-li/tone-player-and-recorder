#include "tm4c123gh6pm.h" // register definitions

typedef enum
{
    PLAY,
    RECORD,
    PLAYBACK
} State;

State current_state = PLAY;
unsigned int EEPROM_mem_addr = 0; // current memory address on the EEPROM
unsigned char sequence[32] = {0}; // temp storage for sequence of notes to play back

const unsigned int note_freqs[8] = {262, 294, 330, 349, 392, 440, 494, 523}; // frequencies for each pitch in an octave

const char keypad[4][4] = { {'1', '2', '3', 'A'},
                            {'4', '5', '6', 'B'},
                            {'7', '8', '9', 'C'},
                            {'*', '0', '#', 'D'} };


// Main program helper functions
void change_state(State new_state);
void play_recording(unsigned char *sequence);

// I2C and EEPROM functions
void I2C0_Init(void);
void I2C0_CheckStatus(void);
void I2C0_EEPROM_BeginTransmission(void);
void I2C0_Write(unsigned char data);
void I2C0_EndTransmission(void);
void EEPROM_Write(unsigned int mem_addr, unsigned char data);
void EEPROM_ReadAll(unsigned char *arr);
void EEPROM_Clear(void);

// Speaker functions
void PWM0_Init(void);
void tone(unsigned int frequency);
void no_tone();

// Other functions
void delay_clock_cycles(unsigned long numCycles);
void LED_Init(void);
void LED_ChangeState(void);


int main(void)
{
    I2C0_Init();
    PWM0_Init();
    LED_Init();
    LED_ChangeState();


    /* Keypad setup for row pins */
    SYSCTL_RCGCGPIO_R |= (1 << 4); // Enable clock for GPIO Port E
    GPIO_PORTE_DEN_R = 0x0F; // Set PE0-PE3 as digital pins
    GPIO_PORTE_DIR_R = 0x0F; // Set PE0-PE3 as output pins

    /* Keypad setup for column pins */
    SYSCTL_RCGCGPIO_R |= (1 << 2); // Enable clock for GPIO Port C
    GPIO_PORTC_DEN_R = 0xF0; // Set PC4-PC7 as digital pins
    GPIO_PORTC_DIR_R = 0x00; // Set PC4-PC7 as input pins
    GPIO_PORTC_PDR_R = 0xF0; // Enable pull-downs (set floating value of input pins to default low)

    while (1)
    {
        // Write a value to each row, and scan all corresponding columns (row and column should have same value if button is pressed)
        int row, col;
        for (row = 0; row < 4; row++)
        {
            GPIO_PORTE_DATA_R = (1 << row); // Write to row

            for (col = 0; col < 4; col++)
            {
                if ((GPIO_PORTC_DATA_R & (1 << (col + 4))) != 0) // Check column bit (if key was pressed)
                {
                    char key = keypad[row][col];

                    /* Check for mode switches */
                    if (key == 'A')
                    {
                        change_state(PLAY);
                        while ((GPIO_PORTC_DATA_R & (1 << (col + 4))) != 0); // Wait for key release
                        delay_clock_cycles(160000); // Debounce
                    }
                    else if (key == 'B')
                    {
                        change_state(RECORD);
                        while ((GPIO_PORTC_DATA_R & (1 << (col + 4))) != 0); // Wait for key release
                        delay_clock_cycles(160000); // Debounce
                    }
                    else if (key == 'C')
                    {
                        change_state(PLAYBACK);
                        while ((GPIO_PORTC_DATA_R & (1 << (col + 4))) != 0); // Wait for key release
                        delay_clock_cycles(160000); // Debounce
                    }


                    /* Execute mode functionality */
                    if (current_state == PLAYBACK)
                    {
                        if (key == 'D')
                        {
                            play_recording(sequence);
                        }
                    }
                    else
                    {
                        if (key >= '1' && key <= '8') // Play note as long as button is pressed
                        {
                            tone(note_freqs[key - '0' - 1]);
                            while ((GPIO_PORTC_DATA_R & (1 << (col + 4))) != 0); // Wait for key release
                            no_tone();
                            delay_clock_cycles(160000); // Debounce

                            if (current_state == RECORD)
                            {
                                EEPROM_Write(EEPROM_mem_addr++, key - '0');
                            }
                        }
                    }
                }
            }
        }
    }

}


/* ================================================================================================================
   MAIN PROGRAM HELPER FUNCTIONS
   ================================================================================================================ */

/** Switches the mode to one of three options: play, record, or playback */
void change_state(State new_state)
{
    if (new_state == RECORD)
    {
        EEPROM_mem_addr = 0;
        EEPROM_Clear();
    }
    if (new_state == PLAYBACK)
    {
        int i;
        for (i = 0; i < 32; i++)
        {
            sequence[i] = 0;
        }
        EEPROM_ReadAll(sequence);
    }
    current_state = new_state;
    LED_ChangeState();
}


/** Plays back a previously recorded sequence of notes */
void play_recording(unsigned char *sequence)
{
    int i;
    for (i = 0; i < 32; i++)
    {
        if (sequence[i] == 0) return;
        int note_idx = sequence[i] - 1;

        tone(note_freqs[note_idx]);
        delay_clock_cycles(1000000); // roughly 1 second
        no_tone();

        delay_clock_cycles(50000);
    }
}


/* ================================================================================================================
   I2C AND EEPROM FUNCTIONS
   ================================================================================================================ */

/** Sets up I2C communication for sending and receiving as master on I2C module 0 */
void I2C0_Init(void)
{
    /* GPIO setup for SDA and SCL pins */
    SYSCTL_RCGCGPIO_R |= (1 << 1); // Enable clock for GPIO Port B
    GPIO_PORTB_DEN_R |= (1 << 3) | (1 << 2); // Configure PB2 and PB3 as digital pins
    GPIO_PORTB_AFSEL_R |= (1 << 3) | (1 << 2); // Set alternate function mode for PB2 and PB3
    GPIO_PORTB_PCTL_R |= (0x3 << 12) | (0x3 << 8); // Select I2C0SCL function for PB2 and I2C0SDA function for PB3
    GPIO_PORTB_ODR_R |= (1 << 3); // Configure PB3 as open drain
    GPIO_PORTB_PUR_R |= (1 << 3) | (1 << 2); // Enable pull-ups (to set floating values to default high)

    /* I2C master setup */
    SYSCTL_RCGCI2C_R |= (1 << 0); // Enable clock to I2C0
    I2C0_MCR_R = (1 << 4); // Enable I2C master mode
    I2C0_MTPR_R = 0x07; // Set SCL clock speed to standard 100 kbps
}


/** Waits for an I2C transaction to finish and performs error checking */
void I2C0_CheckStatus(void)
{
    while (I2C0_MCS_R & (1 << 0)); // Wait until I2C controller is not busy
    if (I2C0_MCS_R & (1 << 1)) // Check for errors
    {
        I2C0_MCS_R = (1 << 2); // End communication
        while (1);
    }
}


/** Initiates I2C multi-byte transmission to the EEPROM */
void I2C0_EEPROM_BeginTransmission(void)
{
    I2C0_MSA_R = 0xA0; // Set slave address and direction byte
    I2C0_MDR_R = 0x0; // Set data byte to EEPROM address high
    I2C0_MCS_R = (1 << 1) | (1 << 0); // Generate start condition and transmit first byte
    I2C0_CheckStatus();

    I2C0_MDR_R = 0x0; // Set data byte to EEPROM address low
    I2C0_MCS_R = (1 << 0); // Transmit next byte
    I2C0_CheckStatus();
}


/** Transmits a single byte via I2C, assuming that multi-byte transmission has been started */
void I2C0_Write(unsigned char data)
{
    I2C0_MDR_R = data; // Set data byte
    I2C0_MCS_R = (1 << 0); // Transmit byte
    I2C0_CheckStatus();
}


/** Stops a previously started I2C transmission */
void I2C0_EndTransmission(void)
{
    I2C0_MCS_R = (1 << 2); // Generate stop condition
    I2C0_CheckStatus();
}


/** Transmits a single byte of data via I2C to the EEPROM at the given memory address */
void EEPROM_Write(unsigned int mem_addr, unsigned char data)
{
    unsigned char addr_high = mem_addr >> 8;
    unsigned char addr_low = mem_addr & 0xFF;

    I2C0_MSA_R = 0xA0; // Set slave address and direction byte
    I2C0_MDR_R = addr_high; // Set data byte to EEPROM address high
    I2C0_MCS_R = (1 << 1) | (1 << 0); // Generate start condition and transmit first byte
    I2C0_CheckStatus();

    I2C0_MDR_R = addr_low; // Set data byte to EEPROM address low
    I2C0_MCS_R = (1 << 0); // Transmit next byte
    I2C0_CheckStatus();

    I2C0_MDR_R = data; // Set data byte to data to be written
    I2C0_MCS_R = (1 << 2) | (1 << 0); // Transmit last byte and generate stop condition
    I2C0_CheckStatus();
    delay_clock_cycles(80000); // roughly 5 ms for write cycle to complete
}


/** Performs a sequential read of the EEPROM starting at the first address, storing retrieved contents in the given array */
void EEPROM_ReadAll(unsigned char *arr)
{
    int i = 0;
    I2C0_EEPROM_BeginTransmission();

    I2C0_MSA_R = 0xA1; // Set slave address and direction byte (1010.0001)
    I2C0_MCS_R = (1 << 3) | (1 << 1) | (1 << 0); // Generated repeated start and receive first byte
    I2C0_CheckStatus();

    while (I2C0_MDR_R != 0xFF)
    {
        arr[i++] = I2C0_MDR_R;
        I2C0_MCS_R = (1 << 3) | (1 << 0); // Receive next byte
        I2C0_CheckStatus();
    }
    I2C0_EndTransmission();
}


/** Erases the first 32 bytes via a page write (assuming no more than a single page of memory was used) */
void EEPROM_Clear(void)
{
    I2C0_EEPROM_BeginTransmission();

    int i;
    for (i = 0; i < 32; i++)
    {
        I2C0_Write(0xFF);
    }
    I2C0_EndTransmission();
    delay_clock_cycles(80000); // roughly 5 ms for write cycle to complete
}


/* ================================================================================================================
   SPEAKER FUNCTIONS
   ================================================================================================================ */

/** Sets up pulse width modulation on PWM Module 0, Generator 0 */
void PWM0_Init(void)
{
    SYSCTL_RCGCPWM_R |= (1 << 0); // Enable clock to PWM0
    SYSCTL_RCC_R &= ~(1 << 20); // Directly feed system clock to PWM0 clock (without pre-divider)

    /* GPIO setup for PWM output pin */
    SYSCTL_RCGCGPIO_R |= (1 << 1); // Enable clock to GPIO Port B
    GPIO_PORTB_DEN_R |= (1 << 6); // Configure PB6 as digital pin
    GPIO_PORTB_AFSEL_R |= (1 << 6); // Set alternate function mode for PB6
    GPIO_PORTB_PCTL_R |= (0x4 << 24); // Select M0PWM0 function for PB6

    /* Generator 0 setup */
    PWM0_0_CTL_R = 0x0; // Set count direction to count-down mode
    PWM0_0_GENA_R = 0x8C; // Set PWM high when counter is reset to load value and set PWM low when counter matches comparator A
    PWM0_0_LOAD_R = 16000; // Set frequency via load value (determines the pitch of the tone)
    PWM0_0_CMPA_R = 16000 - 1; // Set duty cycle via comparator A value (determines the volume of the tone)
    PWM0_0_CTL_R |= (1 << 0); // Enable generator 0 timer
//    PWM0_ENABLE_R |= (1 << 0); // Enable PWM output on M0PWM0
}


/** Plays a tone by generating a square wave of the specified frequency */
void tone(unsigned int frequency)
{
    unsigned int load_value = 16000000 / frequency; // assuming a 16MHz clock
    PWM0_0_LOAD_R = load_value; // Set frequency to control pitch of the tone
    PWM0_0_CMPA_R = load_value - (load_value * 0.1); // Set duty cycle to control volume of the tone (should be kept constant)
    PWM0_ENABLE_R |= (1 << 0); // Enable PWM output on M0PWM0
}


/** Stops a previously playing tone */
void no_tone()
{
    PWM0_ENABLE_R &= ~(1 << 0); // Disable PWM output on M0PWM0
}


/* ================================================================================================================
   MISCELLANEOUS FUNCTIONS
   ================================================================================================================ */

void LED_Init(void)
{
    SYSCTL_RCGCGPIO_R |= (1 << 3); // Enable clock to GPIO Port D
    GPIO_PORTD_DEN_R |= (1 << 3) | (1 << 2) | (1 << 1); // Configure PD1-PD3 as digital pins
    GPIO_PORTD_DIR_R |= (1 << 3) | (1 << 2) | (1 << 1); // Configure PD1-PD3 as output pins
}


void LED_ChangeState(void)
{
    GPIO_PORTD_DATA_R = (1 << (current_state + 1)); // Light correct LED according to current mode
}


/** Pauses the program for a given number of clock cycles (each cycle is 62.5 ns long) */
void delay_clock_cycles(unsigned long numCycles)
{
    volatile unsigned long i;
    for (i = 0; i < numCycles; i++);
}

