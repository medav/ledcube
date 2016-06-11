#include "TLC59116.h"
#include "LedCubeMCUDriver.h"

void InterruptHandler() iv 0x0008 ics ICS_AUTO {

    if(RC1IF) {
        auto_idle_flag = 1;
        auto_idle_counter = 0;
        UARTRecieve();
    }
}

void main() {
    // Set OSC to 64 MHz
    IRCF2_bit = 1;
    IRCF1_bit = 1;
    IRCF0_bit = 1;

    INTSRC_bit = 1;
    PLLEN_bit = 1;

    // Setup output pins
    ANSELC = 0;
    TRISB = 0x00;
    TRISD = 0x00;
    LATB = 0x00;

    // Setup default variable values
    // and communications
    SetDefaults();
    InitDriverI2C();
    InitUART();

    // Delay 1 second in case the power cable is
    // still being plugged in
    Delay_1sec();

    // Reset the Drivers
    TLC59116_ResetAll();
    TLC59116_Setup();

    // Run Idle Pattern before allowing input
    IdlePattern();

    // Enable all interrupts
    INTCON.GIE = 1;
    INTCON.PEIE = 1;


    while(1) {
        Refresh();
        auto_idle_counter++;

        if((auto_idle_counter > auto_idle_timeout) && auto_idle_enable && auto_idle_flag) {
            IdlePattern();
            auto_idle_counter = 0;
            auto_idle_flag = 0;
        }
    }
}

void DispatchCmd() {
    switch(curcmd) {
    case READFRAME:
        SwapBuffers();
        break;
    case SETCONTROL:
        SetVar(arg_buffer[0], *((int *) (arg_buffer + 1)));
        break;
    default:
        break;
    }
}

void InitUART() {
    // 230.4 kBaud
    // Multiplier on
    TXSTA1.BRGH = 1;
    
    TXSTA1.BRG16 = 0;

    // SPBRGH1:SPBRG1 = 0x0010
    SPBRGH1 = 0;
    SPBRG1 = 16;

    // Want Async operation
    TXSTA1.SYNC = 0;
    TXSTA1.TXEN = 1;

    // Enable Interrupts
    PIE1.RC1IE = 1;
    IPR1.RC1IP = 1;

    // Turn on the UART
    RCSTA1.SPEN = 1;
    RCSTA1.CREN = 1;
}

void UARTRecieve() {
    byte recv = RCREG1;
    
    if(recv == ENDCMD) {
        if(curcmd != NOOP) {
            DispatchCmd();
            curcmd = NOOP;
        }
        return;
    }

    switch(curcmd) {
    case NOOP:
        curcmd = recv;
        buffer_counter = 0;
        arg_counter = 0;
        break;
    case READFRAME:
        if(buffer_counter < 127)
            buffer[buffer_counter++] = recv;
        break;
    case SETCONTROL:
        if(arg_counter < 63)
            arg_buffer[arg_counter++] = recv;
        break;
    default:
        curcmd = NOOP;
        break;
    }
}

void InitDriverI2C() {
    I2C1_Init(800000);
}

void Alive() {
    int i;

    for(i = 0; i < 3; i++) {
        Delay_100ms();
        LATD = 0x03;
        Delay_100ms();
        LATD = 0x00;
        
        TXREG1 = 65;
    }

    Delay_100ms();
    LATD = 0x03;
}

void SetDefaults() {
    curcmd = 0;
    auto_idle_timeout = 4000;
    auto_idle_counter = 0;
    auto_idle_enable = 1;
    auto_idle_flag = 0;
    led_power_duration = 1000;
    buffer = buffer1;
    backbuffer = buffer2;
    memset(buffer, 0x00, BUFFERSIZE);
}

void Refresh() {
    int l, i;
    for(l = 0; l < SIZE; l++) {
        TLC59116_WriteLEDs(0, &buffer[l * 16 + 0]);
        TLC59116_WriteLEDs(1, &buffer[l * 16 + 4]);
        TLC59116_WriteLEDs(2, &buffer[l * 16 + 8]);
        TLC59116_WriteLEDs(3, &buffer[l * 16 + 12]);

        LATB = 0x80 >> l;
          TLC59116_On();
          for(i = 0; i < led_power_duration; i++);

        LATB = 0x00;

    }
}

void IdlePattern() {
    int i, d;
    memset(buffer, 0x00, BUFFERSIZE);
    
    for(i = 0; i < SIZE; i++) {
        LedOn(0, 0, i);
        LedOn(7, 0, i);
        LedOn(0, 7, i);
        LedOn(7, 7, i);
        
        LedOn(0, i, 0);
        LedOn(7, i, 0);
        LedOn(0, i, 7);
        LedOn(7, i, 7);
        
        LedOn(i, 0, 0);
        LedOn(i, 7, 0);
        LedOn(i, 0, 7);
        LedOn(i, 7, 7);
        
        for(d = 0; d < 2; d++) {
            Refresh();
        }
    }
}

void SwapBuffers() {
    byte * temp = buffer;
    buffer = backbuffer;
    backbuffer = temp;
}

void SetVar(CONTROLDATA var, int val) {
    switch(var) {
    case V_LED_POWER_DURATION:
        led_power_duration = val;
        break;
    case V_AUTO_IDLE_ENABLE:
        auto_idle_enable = val;
        break;
    case V_AUTO_IDLE_TIMEOUT:
        auto_idle_timeout = val;
        break;
    default:
        break;
    }
}

void LedSet(byte x, byte y, byte z, byte val) {
    int i = 16 * z + 2 * y + (x > 3 ? 1 : 0);
    int o = (x % 4) * 2;
    buffer[i] = ((buffer[i] & ~(0x03 << o)) | val << o);
}

void LedOn(byte x, byte y, byte z) {
    LedSet(x, y, z, 1);
}

void LedOff(byte x, byte y, byte z) {
    LedSet(x, y, z, 0);
}