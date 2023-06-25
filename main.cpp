#define F_CPU 16000000UL
#include "funsape/globalDefines.hpp"
#include "funsape/device/keypad.hpp"
#include "funsape/device/hd44780.hpp"
#include <avr/io.h>
#include <string.h>
#include <math.h>

#define firstLine       PB2
#define firstCol       PC1
#define BAUD_RATE 9600
#define ANALOG_CHANNEL 0 // Canal ADC usado para leitura

// Funções
void USARTInit(unsigned int ubrr);
void USARTTransmit(unsigned char data);
void USARTTransmitString(const char *str);
void sendATCommand(const char *command);
void ADCInit();
uint16_t ADCRead(uint8_t channel);
void SendSMS(const char *SMSnumber);

// Variaveis globais
uint8_t mode = 0;
uint16_t luminosidade = 0;
uint16_t porcentagem = 0;

int main()
{
    // Configs
    Hd44780 display;
    display.controlPortSet(&DDRD, PD2, PD3);
    display.dataPortSet(&DDRD, PD4);
    display.init(Hd44780::Size::LCD_16X2, Hd44780::Font::FONT_5X8, true, false);
    display.stdio();

    Keypad keypad;
    keypad.setPorts(&DDRC, firstCol, &DDRB, firstLine);
    keypad.setKeyValues(Keypad::Type::KEYPAD_4X4,
            '1', '2', '3', '-',
            '4', '5', '6', '*',
            '7', '8', '9', '/',
            'E', '0', '=', '+'
    );
    keypad.init(10);

    // Variaveis locais
    char cellNumber[20] = "";
    uint8_t cursorPosition = 0;
    uint16_t tamanho;

    // Inicializa o ADC
    ADCInit();
    delayMs(200);

    // Inicializa a comunicação USART
    USARTInit(BAUD_RATE);
    // Espera o SIM900 inicializar
    delayMs(2000);

    while(1) {
        // Modo 0: Digite o numero de telefone
        if(mode == 0) {
            // Le o dígito pressionado
            uint8_t keyPressed;
            keypad.readKeyPressed(&keyPressed);

            switch(keyPressed) {
            case 0xFF:
                break;
            case 69:
                // Usa o numero default (numero do lucas)
                delayMs(20);
                display.cursorHome();
                display.clearScreen();
                strcpy(cellNumber, "+5548999309880");
                printf("%s", cellNumber);
                delayMs(2000);
                mode = 1;
                break;
            case 61:
                // Mantem o numero atual
                mode = 1;
                break;
            case 45:
                // Remove o último caractere
                delayMs(20);
                tamanho = strlen(cellNumber);
                if(tamanho > 0) {
                    cellNumber[tamanho - 1] = '\0';
                    cursorPosition--;
                    display.cursorGoTo(0, cursorPosition);
                    printf("%c", ' ');
                }
                break;
            default:
                // Adiciona o digito pressionado ao número de telefone
                delayMs(20);
                char keyPressedAsString[3];
                sprintf(keyPressedAsString, "%c", (char)keyPressed);
                strncat(cellNumber, keyPressedAsString, sizeof(cellNumber) - strlen(cellNumber) - 1);
                display.cursorGoTo(0, cursorPosition);
                printf("%c", (char)keyPressed);
                cursorPosition++;
                break;
            }
        }
        // Modo 1: Le a luminosidade e envia um SMS
        if(mode == 1) {
            // Limpa o display
            display.clearScreen();
            display.cursorHome();
            // Le a luminosidade pelo ADC
            luminosidade = ADCRead(ANALOG_CHANNEL);
            // Calcula a porcentagem de luminosidade
            uint16_t porcentagem = ((1023 - luminosidade) / 1023.0) * 100.0;
            // Mostra a luminosidade na tela
            char buffer[32];
            sprintf(buffer, "Luminosidade=%d%%", porcentagem);
            printf("%s", buffer);
            SendSMS(cellNumber);
            delayMs(3000);
            cursorPosition = 0;
            display.cursorHome();
            display.clearScreen();
            mode = 0;
        }
    }
}

void USARTInit(unsigned int ubrr)
{
    // Calcula o valor do registrador UBRR0 (registrador que define a taxa de transmissão)
    uint16_t ubrr_value = F_CPU / (16UL * ubrr) - 1;

    // Configura os registradores
    UBRR0H = (unsigned char)(ubrr_value >> 8);
    UBRR0L = (unsigned char)ubrr_value;

    // Habilita o receptor e transmissor
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);

    // Seta o formato do frame: 8 data bits, 1 stop bit
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// Transmite um caractere pela USART
void USARTTransmit(unsigned char data)
{
    // Verifica se o buffer de transmissão está vazio
    while(!(UCSR0A & (1 << UDRE0)))
        ;

    // Coloca a informação no buffer e envia
    UDR0 = data;
}

void USARTTransmitString(const char *str)
{
    // Transmite uma string pela USART
    for(int i = 0; str[i] != '\0'; i++) {
        USARTTransmit(str[i]);
    }
}

void sendATCommand(const char *command)
{
    // Limpa o buffer de recepção
    while(UCSR0A & (1 << RXC0)) {
        (void)UDR0;
    }

    // Transmite o comando
    USARTTransmitString(command);

    // Espera um tempo de resposta
    delayMs(500);
}

void ADCInit()
{
    // Configuração do ADC
    ADMUX |= (1 << REFS0); // Tensão de referência em AVCC
    ADCSRA |= (1 << ADEN); // Habilita o ADC
}

uint16_t ADCRead(uint8_t channel)
{
    // Seleciona o canal de leitura
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);

    // Inicia a conversão
    ADCSRA |= (1 << ADSC);

    // Aguarda a conversão ser concluída
    while(ADCSRA & (1 << ADSC))
        ;

    // Retorna o valor lido
    return ADC;
}

void SendSMS(const char *SMSnumber)
{
    // Seta o modo de texto para SMS
    sendATCommand("AT+CMGF=1\r\n");
    delayMs(500);

    char buffer1[64];
    sprintf(buffer1, "AT+CMGS=\"%s\"\r\n", SMSnumber);
    // Seta o número para quem vai ser enviado o SMS
    sendATCommand(buffer1);
    delayMs(500);

    // Edita o buffer para montar a mensagem
    char buffer2[32];
    sprintf(buffer2, "Luminosidade = %d%%", porcentagem);

    // Envia a mensagem do buffer em si
    sendATCommand(buffer2);
    sendATCommand("\r\n");
    delayMs(500);

    // Manda Ctrl+Z para indicar o fim da mensagem
    USARTTransmit(0x1A);
}
