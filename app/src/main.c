#include "stm8s.h"

// 7segmenty
#define SEG1 (GPIO_PIN_1)
#define SEG2 (GPIO_PIN_2)
#define SEG3 (GPIO_PIN_3)
#define SEG4 (GPIO_PIN_4)

// Tlačítka
#define BUTTON1 (GPIO_PIN_1)
#define BUTTON2 (GPIO_PIN_2)
#define BUTTON3 (GPIO_PIN_3)
#define BUTTON4 (GPIO_PIN_4)
#define BUTTON5 (GPIO_PIN_6)
#define BUTTON6 (GPIO_PIN_7)

// Funkce pro vypínání a zapínání 7segmentů
#define SEGOFF(SEG) (GPIO_WriteHigh(GPIOG, SEG))
#define SEGON(SEG) (GPIO_WriteLow(GPIOG, SEG))

// Funkce pro kontrolu zmáčknutí tlačítka
#define buttonPressed(BUTTON) (GPIO_ReadInputPin(GPIOC, BUTTON)==RESET)

// Funkce pro výpočet délky arraye
#define len(arr) sizeof(arr)/sizeof(arr[0])

// Funkce
void delay(uint32_t iterations);
void adjustTime();
void changeState();
void reset();
void buzz(int timeUnit);

// Globální proměnné - potřebné pro interrupt
uint8_t state = 0; // Stav časovače (0 = neaktivní, 1 = odpočítává)
uint8_t segValues[4] = {0, 0, 0, 0}; // Číselné odnoty na 4dig 7segment
GPIO_Pin_TypeDef segButtons[4] = {BUTTON1, BUTTON2, BUTTON3, BUTTON4}; // Tlačítka pro nastavování času
int secs; // Počet sekund
int mins; // Počet minut
uint16_t buzzCount = 0; // Počítání bzučení

// Přerušení
INTERRUPT_HANDLER(EXTI_PORTC_IRQHandler, 5)
{
    if (state == 0) {adjustTime();} // Pokud je stav odpočtu neaktivní, je možné nastavit čas na odpočet

    if (buttonPressed(BUTTON5)) {changeState();} // Změna stavu odpočtu při zmáčknutí tlačítka 5
    
    if (buttonPressed(BUTTON6)) {reset();} // Resetování odpočtu při zmáčknutí tlačítka 6
}

void main(void)
{
    // Deinicializace potřebných modulů
    GPIO_DeInit;
    TIM4_DeInit;
    EXTI_DeInit;

    CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1); // FREQ MCU 16MHz

    // Inicializace potřebných portů na STM8
    GPIO_Init(GPIOC, GPIO_PIN_ALL, GPIO_MODE_IN_PU_IT);
    GPIO_Init(GPIOG, GPIO_PIN_ALL, GPIO_MODE_OUT_PP_LOW_SLOW);
    GPIO_Init(GPIOD, GPIO_PIN_6, GPIO_MODE_OUT_PP_LOW_SLOW);
    GPIO_Init(GPIOB, GPIO_PIN_ALL, GPIO_MODE_OUT_PP_LOW_SLOW);

    // Nastavení přerušení
    EXTI_SetExtIntSensitivity(EXTI_PORT_GPIOC, EXTI_SENSITIVITY_FALL_ONLY);
    ITC_SetSoftwarePriority(ITC_IRQ_PORTC, ITC_PRIORITYLEVEL_0);
    enableInterrupts();

    // Nastavení časování
    TIM4_TimeBaseInit(TIM4_PRESCALER_128, 250);
    TIM4_Cmd(ENABLE);

    // Lokální proměnné - nepotřebné pro interrupt
    GPIO_Pin_TypeDef SEGS[8] = {SEG1, SEG2, SEG3, SEG4}; // 7segmenty
    uint8_t nums[10] = {0b00111111, 0b00000110, 0b01011011, 
                        0b01001111, 0b01100110, 0b01101101, 
                        0b01111101, 0b00000111, 0b01111111, 
                        0b01100111}; // Čísla zobrazované na 4dig 7segmentu
    uint32_t timeUnit = 0; // Jednotka pro počítání času (500 = 1 sekunda)

    // Výpočet sekund a minut z čísel na 4dig 7segmentu
    secs = segValues[2] * 10 + segValues[3];
    mins = segValues[0] * 10 + segValues[1];

    while (1)
    {
        // Odpočítávání zadaného času
        if (state == 1) {

            // Kontrola odpočtu
            uint8_t segSum = 0; // Součet hodnot na 4dig 7segmentu (0 -> odpočet ukončen)
            for(uint8_t i = 0; i < len(segValues); i++) {
                segSum += segValues[i];
            }

            if(TIM4_GetFlagStatus(TIM4_FLAG_UPDATE)==SET) {
                TIM4_ClearFlag(TIM4_FLAG_UPDATE);
                timeUnit++;

                // Pokud časovač dokončil odpočet, 10 sekund bzuč, pak nastav stav odpočtu na neaktivní
                if (segSum == 0) {
                    if (timeUnit == 500) {
                        timeUnit = 0;
                        buzzCount++;
                    }
                    buzz(timeUnit);
                }
            }
            
            if (segSum != 0) {
                // Pokud uběhla sekunda, odečti jednu sekundu z odpočtu
                if (timeUnit == 500) {
                    timeUnit = 0;
                    secs--;
                }

                // Pokud uběhla minuta, odečti jednu minut z odpočtu
                if (secs<0) {
                    secs = 59;
                    if (mins != 0) {
                        mins--;
                    }
                }

                // Převeď sekundy a minuty zpět na číselné hodnoty na 4dig 7segmentu
                segValues[3] = secs % 10;
                segValues[2] = secs / 10;
                segValues[1] = mins % 10;
                segValues[0] = mins / 10;
            }
        } 
        
        // Zobrazení číselných hodnot na 4dig 7segmentu
        for (uint8_t i = 0; i < len(SEGS); i++) {
            SEGON(SEGS[i]);
            GPIO_Write(GPIOB, nums[segValues[i]]);
            if (i == 1) {GPIO_WriteHigh(GPIOB, GPIO_PIN_7);} // Zobrazení tečky mezi minutami a sekundami
            delay(150);
            SEGOFF(SEGS[i]);
        }
    }
}

// Pozastavení
void delay(uint32_t iter)
{
    for(uint32_t i = 0; i < iter; i++);
}

// Změna číselných hodnot, které se mají odpočítávat
void adjustTime() {
    for (uint8_t i = 0; i < len(segButtons); i++) {
        if (buttonPressed(segButtons[i])) {
            // Pokud bylo tlačítko zmáčknuto, přičti 1 k číselné hodnotě na daném 7segmentu
            segValues[i]++;
            if (i == 2) { // Třetí 7segment je nastavitelný pouze do pěti
                if (segValues[i] > 5) {
                    segValues[i] = 0;
                }
            } else { // Ostatní 7segmenty jsou nastavitelné do devíti
                if (segValues[i] > 9) {
                    segValues[i] = 0;
                }
            }
        }
        // Přepočtení upravených číselných hodnot 4dig 7segmentu na minuty a sekundy
        mins = segValues[0] * 10 + segValues[1];
        secs = segValues[2] * 10 + segValues[3];
    }
}

// Změna stavu odpočtu - je možné odpočet spustit nebo pozastavit
void changeState() {
    if (state == 0) {
        state = 1;
        buzzCount = 0;
    } else {
        state = 0;
    }
}

// Resetování odpočtu
void reset() {
    state = 0;
    GPIO_WriteLow(GPIOD, GPIO_PIN_6); // Zastavení bzučení, pokud je v provozu

    // Vynulování času na 4dig 7segmentu
    for (uint8_t i = 0; i < len(segValues); i++) {
        segValues[i] = 0;
    }
    mins = segValues[0] * 10 + segValues[1];
    secs = segValues[2] * 10 + segValues[3];
}

// 10ti-sekundové bzučení
void buzz(int timeUnit) {
    if (buzzCount < 10) {
        // Chvíli ticho, chvíli bzučí
        if (timeUnit == 100) {
            GPIO_WriteReverse(GPIOD, GPIO_PIN_6);
        }
    // Po 10tisekundách se se zastaví bzučení a stav odpočtu se nastaví na neaktivní
    } else if (buzzCount == 10) {
        GPIO_WriteLow(GPIOD, GPIO_PIN_6);
        state = 0;
    }
}
