// RSLK Self Test via UART

/* This example accompanies the books
   "Embedded Systems: Introduction to the MSP432 Microcontroller",
       ISBN: 978-1512185676, Jonathan Valvano, copyright (c) 2017
   "Embedded Systems: Real-Time Interfacing to the MSP432 Microcontroller",
       ISBN: 978-1514676585, Jonathan Valvano, copyright (c) 2017
   "Embedded Systems: Real-Time Operating Systems for ARM Cortex-M Microcontrollers",
       ISBN: 978-1466468863, , Jonathan Valvano, copyright (c) 2017
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/

Simplified BSD License (FreeBSD License)
Copyright (c) 2017, Jonathan Valvano, All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are
those of the authors and should not be interpreted as representing official
policies, either expressed or implied, of the FreeBSD Project.
*/

#include "msp.h"
#include <stdint.h>
#include <string.h>
#include "..\inc\UART0.h"
#include "..\inc\EUSCIA0.h"
#include "..\inc\FIFO0.h"
#include "..\inc\Clock.h"
// #include "..\inc\SysTick.h"
#include "..\inc\SysTickInts.h"
#include "..\inc\CortexM.h"
#include "..\inc\TimerA1.h"
// #include "..\inc\Bump.h"
#include "..\inc\BumpInt.h"
#include "..\inc\LaunchPad.h"
#include "..\inc\Motor.h"
#include "../inc/IRDistance.h"
#include "../inc/ADC14.h"
#include "../inc/LPF.h"
#include "..\inc\Reflectance.h"
#include "../inc/TA3InputCapture.h"
#include "../inc/Tachometer.h"

////////// [2] IR Sensor //////////

volatile uint32_t ADCflag;
volatile uint32_t nr, nc, nl;

void SensorRead_ISR(void)
{ // runs at 2000 Hz
    uint32_t raw17, raw12, raw16;
    P1OUT ^= 0x01;                          // profile
    P1OUT ^= 0x01;                          // profile
    ADC_In17_12_16(&raw17, &raw12, &raw16); // sample
    nr = LPF_Calc(raw17);                   // right is channel 17 P9.0
    nc = LPF_Calc2(raw12);                  // center is channel 12, P4.1
    nl = LPF_Calc3(raw16);                  // left is channel 16, P9.1
    ADCflag = 1;                            // semaphore
    P1OUT ^= 0x01;                          // profile
}

void IRSensor_Init(void)
{
    uint32_t raw17, raw12, raw16;
    uint32_t s;
    ADCflag = 0;
    s = 256;                                // replace with your choice
    ADC0_InitSWTriggerCh17_12_16();         // initialize channels 17,12,16
    ADC_In17_12_16(&raw17, &raw12, &raw16); // sample
    LPF_Init(raw17, s);                     // P9.0/channel 17
    LPF_Init2(raw12, s);                    // P4.1/channel 12
    LPF_Init3(raw16, s);                    // P9.1/channel 16
    TimerA1_Init(&SensorRead_ISR, 250);     // 2000 Hz sampling
}

////////// [3] Bumper //////////

volatile uint8_t ContactFlag;

void Check_Contact()
{
    Motor_Stop();
    ContactFlag = 1;
    P4->IFG &= ~0xED; // clear interrupt flag
}

////////// [5] Tachometer //////////

#define P2_4 (*((volatile uint8_t *)(0x42098070)))
#define P2_3 (*((volatile uint8_t *)(0x4209806C)))
#define P2_2 (*((volatile uint8_t *)(0x42098068)))
#define P2_1 (*((volatile uint8_t *)(0x42098064)))
#define P2_0 (*((volatile uint8_t *)(0x42098060)))

uint16_t Period0;    // (1/SMCLK) units = 83.3 ns units
uint16_t First0 = 0; // Timer A3 first edge, P10.4
uint32_t Done0 = 0;  // set each rising

uint16_t Period2;    // (1/SMCLK) units = 83.3 ns units
uint16_t First2 = 0; // Timer A3 first edge, P8.2
uint32_t Done2 = 0;  // set each rising

// max period is (2^16-1)*83.3 ns = 5.4612 ms
// min period determined by time to run ISR, which is about 1 us
void PeriodMeasure0(uint16_t time)
{
    Period0 = (time - First0) & 0xFFFF; // 16 bits, 83.3 ns resolution
    First0 = time;                      // setup for next
    Done0++;
}

// max period is (2^16-1)*83.3 ns = 5.4612 ms
// min period determined by time to run ISR, which is about 1 us
void PeriodMeasure2(uint16_t time)
{
    Period2 = (time - First2) & 0xFFFF; // 16 bits, 83.3 ns resolution
    First2 = time;                      // setup for next
    Done2++;
}

//////////////////////////////

void RSLK_Reset(void)
{
    DisableInterrupts();

    LaunchPad_Init();
    // Initialise modules used e.g. Reflectance Sensor, Bump Switch, Motor, Tachometer etc
    //  ... ...

    // Motor
    Motor_Init();
    Motor_Stop();

    // IR Sensor
    IRSensor_Init();

    // Bumper
    BumpInt_Init(&Check_Contact);

    // Reflectance Sensor
    Reflectance_Init();

    EnableInterrupts();
}

// RSLK Self-Test
// Sample program of how the text based menu can be designed.
// Only one entry (RSLK_Reset) is coded in the switch case. Fill up with other menu entries required for Lab5 assessment.
// Init function to various peripherals are commented off.  For reference only. Not the complete list.

int main(void)
{
    uint32_t cmd = 0xDEAD, menu = 0;

    DisableInterrupts();
    Clock_Init48MHz(); // makes SMCLK=12 MHz
    // SysTick_Init(48000,2);  // set up SysTick for 1000 Hz interrupts
    // Motor_Init();
    // Motor_Stop();
    LaunchPad_Init();
    // Bump_Init();
    // Bumper_Init();
    // IRSensor_Init();
    // Tachometer_Init();
    EUSCIA0_Init(); // initialize UART
    EnableInterrupts();

    while (1)
    { // Loop forever
        // write this as part of Lab 5
        EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
        EUSCIA0_OutString("RSLK Testing"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
        EUSCIA0_OutString("[0] RSLK Reset"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
        EUSCIA0_OutString("[1] Motor Test"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
        EUSCIA0_OutString("[2] IR Sensor Test"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
        EUSCIA0_OutString("[3] Bumper Test"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
        EUSCIA0_OutString("[4] Reflectance Sensor Test"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);
        EUSCIA0_OutString("[5] Tachometer Test"); EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);

        EUSCIA0_OutString("CMD: ");
        cmd = EUSCIA0_InUDec();
        EUSCIA0_OutChar(CR); EUSCIA0_OutChar(LF);

        switch (cmd)
        {
        case 0:
            RSLK_Reset();
            menu = 1;
            cmd = 0xDEAD;
            break;

        //////////////////////////////

        // Motor Test

        case 1:
        {
            EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
            EUSCIA0_OutString("[1] Motor Test");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
            Motor_Init();

            uint32_t choice;

            do
            {
                EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                EUSCIA0_OutString("SELECT A DIRECTION");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                EUSCIA0_OutString("[1] Forward");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                EUSCIA0_OutString("[2] Backward");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                EUSCIA0_OutString("[3] Left");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                EUSCIA0_OutString("[4] Right");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                EUSCIA0_OutString("[5] EXIT");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                EUSCIA0_OutString("Enter your choice: ");
                choice = EUSCIA0_InUDec();
                EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);

                switch (choice)
                {
                case 1:
                    EUSCIA0_OutString("Forward selected");
                    EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                    Motor_Forward(3000, 3000);
                    Clock_Delay1ms(3000);
                    Motor_Stop();
                    break;
                case 2:
                    EUSCIA0_OutString("Backward selected");
                    EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                    Motor_Backward(3000, 3000);
                    Clock_Delay1ms(3000);
                    Motor_Stop();
                    break;
                case 3:
                    EUSCIA0_OutString("Left selected");
                    EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                    Motor_Left(3000, 3000);
                    Clock_Delay1ms(3000);
                    Motor_Stop();
                    break;
                case 4:
                    EUSCIA0_OutString("Right selected");
                    EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                    Motor_Right(3000, 3000);
                    Clock_Delay1ms(3000);
                    Motor_Stop();
                    break;
                default:
                    break;
                }                  // end switch for motor selection
            } while (choice != 5); // end do-while loop

            menu = 1;
            cmd = 0xDEAD;
            break;
        } // end for case 1

        //////////////////////////////

        // IR Sensor Test

        case 2:
        {
            IRSensor_Init();

            EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
            EUSCIA0_OutString("[2] IR Sensor Test");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
            EUSCIA0_OutString("Place an object in front of the IR Sensors");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);

            EUSCIA0_OutString("Left, Center, Right:");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);

            while (1)
            {
                for (int n = 0; n < 2000; n++)
                {
                    while (ADCflag == 0)
                    {
                    };
                    ADCflag = 0; // show every 2000th point
                }
                EUSCIA0_OutUDec5(LeftConvert(nl));EUSCIA0_OutString(" mm,");
                EUSCIA0_OutUDec5(CenterConvert(nc));EUSCIA0_OutString(" mm,");
                EUSCIA0_OutUDec5(RightConvert(nr));EUSCIA0_OutString(" mm\r\n");
            }

            menu = 1;
            cmd = 0xDEAD;
            break;
        } // end case 2

        //////////////////////////////

        // Bumper Test (with simultaneous bump switch detection)

        case 3:
        {
            uint8_t bumpdata;
            uint8_t bumparr[6];

            EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
            EUSCIA0_OutString("[3] Bumper Test");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
            EUSCIA0_OutString("Press any bump switch(es)");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);

            BumpInt_Init(&Check_Contact);

            while (1)
            {
                WaitForInterrupt();

                if (ContactFlag == 1)
                {
                    bumpdata = Bump_Read();

                    for (int i = 0; i < 6; i++)
                    {
                        bumparr[i] = bumpdata % 2;
                        bumpdata /= 2;
                    }

                    EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                    EUSCIA0_OutString("Bump Switch Readings: ");

                    for (int i = 5; i >= 0; i--)
                    {
                        EUSCIA0_OutUDec(bumparr[i]);
                        EUSCIA0_OutString(" ");
                    }

                    EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                    EUSCIA0_OutString("Bump Switch Closed: ");

                    for (int i = 0; i < 6; i++)
                    {
                        if (bumparr[i] == 0)
                        { // bump switch asserted
                            EUSCIA0_OutUDec(i + 1);EUSCIA0_OutString(" ");
                        }
                    } // end for loop
                    EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                } // end if for ContactFlag condition checking
                ContactFlag = 0;
            } // end while

            menu = 1;
            cmd = 0xDEAD;
            break;
        } // end case 3

        //////////////////////////////

        // Reflectance Sensor Test

        case 4:
        {
            Reflectance_Init();

            EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
            EUSCIA0_OutString("[4] Reflectance Sensor Test");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
            EUSCIA0_OutString("Place the robot on a line");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);

            uint8_t refdata;
            uint8_t refarr[8];
            //int32_t position;

            EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
            EUSCIA0_OutString("Left[b7] to Right[b0], Position");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);

            while (1)
            {
                // read data and calc position
                refdata = Reflectance_Read(1000);
                //position = Reflectance_Position(refdata);

                // store indiv bits in an array
                for (int i = 7; i >= 0; i--)
                {
                    refarr[i] = refdata % 2;
                    refdata /= 2;
                }

                // traverse the array and print from LtoR
                for (int i = 0; i < 8; i++)
                {
                    EUSCIA0_OutUDec(refarr[i]);
                    EUSCIA0_OutString(" ");
                }

                // print position in mm
                //UART0_OutString(", ");UART0_OutUDec5(position * 0.1);UART0_OutString("mm");
                EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);

                Clock_Delay1ms(1000);
            }

            menu = 1;
            cmd = 0xDEAD;
            break;
        } // end case 4

        //////////////////////////////

        // Tachometer Test (with RPM calculations)

        case 5:
        {
            EUSCIA0_OutString("[5] Tachometer Test");EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);

            uint32_t main_count = 0;

            // Period0 and Period2 is the time taken for 1 pulse (in units of 83.3ns)
            // For every 1 rotation of the wheel, it takes 360 pulses
            // RPM = 1 minute / time taken for 1 rotation

            long rpmLeft;
            long rpmRight;

            UART0_Init(); // initialize UART0 115,200 baud rate
            Motor_Init(); // configure motor
            TimerA3Capture_Init(&PeriodMeasure0, &PeriodMeasure2);
            Clock_Delay1ms(500);
            Motor_Forward(3000,3000);
            EnableInterrupts();
            while (1)
            {
                WaitForInterrupt();
                main_count++;
                if (main_count % 1000)
                {
                    EUSCIA0_OutString("Left Period = ");EUSCIA0_OutUDec5(Period2);EUSCIA0_OutString(" | Right Period = ");EUSCIA0_OutUDec5(Period0);
                    EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);

                    rpmLeft = (60 * 1000 * 1000) / (360 * 83.3 * Period2 / 1000);
                    rpmRight = (60 * 1000 * 1000) / (360 * 83.3 * Period0 / 1000);

                    EUSCIA0_OutString("Left RPM = ");EUSCIA0_OutUDec5(rpmLeft);EUSCIA0_OutString("    | Right RPM = ");EUSCIA0_OutUDec5(rpmRight);
                    EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);EUSCIA0_OutChar(CR);EUSCIA0_OutChar(LF);
                }
            }

            menu = 1;
            cmd = 0xDEAD;
            break;
        } // end case 5

        default:
            menu = 1;
            break;
        } // end switch for main

        if (!menu)
            Clock_Delay1ms(3000);
        else
        {
            menu = 0;
        }

        // ....
        // ....
    } // end main while loop
} // end of main function

#if 0
//Sample program for using the UART related functions.
int Program5_4(void){
//int main(void){
    // demonstrates features of the EUSCIA0 driver
  char ch;
  char string[20];
  uint32_t n;
  DisableInterrupts();
  Clock_Init48MHz();  // makes SMCLK=12 MHz
  EUSCIA0_Init();     // initialize UART
  EnableInterrupts();
  EUSCIA0_OutString("\nLab 5 Test program for EUSCIA0 driver\n\rEUSCIA0_OutChar examples\n");
  for(ch='A'; ch<='Z'; ch=ch+1){// print the uppercase alphabet
     EUSCIA0_OutChar(ch);
  }
  EUSCIA0_OutChar(LF);
  for(ch='a'; ch<='z'; ch=ch+1){// print the lowercase alphabet
    EUSCIA0_OutChar(ch);
  }
  while(1){
    EUSCIA0_OutString("\n\rInString: ");
    EUSCIA0_InString(string,19); // user enters a string
    EUSCIA0_OutString(" OutString="); EUSCIA0_OutString(string); EUSCIA0_OutChar(LF);

    EUSCIA0_OutString("InUDec: ");   n=EUSCIA0_InUDec();
    EUSCIA0_OutString(" OutUDec=");  EUSCIA0_OutUDec(n); EUSCIA0_OutChar(LF);
    EUSCIA0_OutString(" OutUFix1="); EUSCIA0_OutUFix1(n); EUSCIA0_OutChar(LF);
    EUSCIA0_OutString(" OutUFix2="); EUSCIA0_OutUFix2(n); EUSCIA0_OutChar(LF);

    EUSCIA0_OutString("InUHex: ");   n=EUSCIA0_InUHex();
    EUSCIA0_OutString(" OutUHex=");  EUSCIA0_OutUHex(n); EUSCIA0_OutChar(LF);
  }
}
#endif
