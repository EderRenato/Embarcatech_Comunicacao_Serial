#include <stdio.h>
#include <string.h>
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "ws2818b.pio.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/uart.h"
#include "hardware/irq.h"

#define UART_ID uart0               // Usando UART0
#define BAUD_RATE 115200            // Velocidade da comunicação
#define UART_TX_PIN 0               // Pino TX do Pico (GP0)
#define UART_RX_PIN 1               // Pino RX do Pico (GP1)
#define I2C_PORT i2c1               // Porta i2c1
#define DISPLAY_SDA 14              // Pino do SDA
#define DISPLAY_SCL 15              // Pino do SCL
#define ENDERECO 0x3C               // Endereço do display
#define MATRIX_PIN 7                // Pino da matriz de LEDs
#define LED_COUNT 25                // Numero de leds
const uint BLUE_PIN = 12;           // Pino azul do LED RGB
const uint GREEN_PIN = 11;          // Pino verde do LED RGB
const uint BOTAO_A = 5;             // Pino do botão A
const uint BOTAO_B = 6;             // Pino do botão B
const uint DEBOUNCE_DELAY = 100;    // Tempo de debounce para os botões
volatile char ch;                   // Variavel que pega o caractere da UART
volatile bool GREEN_STATE = false;  // Variavel do estado do led verde
volatile bool BLUE_STATE = false;   // variavel do estado do led azul
volatile uint32_t last_interrupt_time_a = 0; // tempo de interrupção do botao a
volatile uint32_t last_interrupt_time_b = 0; // tempo de interrupção do botao b

// Estrutura para representar um pixel (LED)
struct pixel_t {
    uint8_t G, R, B;        // Componentes de cor: Verde, Vermelho e Azul
};

typedef struct pixel_t pixel_t; // Alias para a estrutura pixel_t
typedef pixel_t npLED_t;        // Alias para facilitar o uso no contexto de LEDs

npLED_t leds[LED_COUNT];        // Array para armazenar o estado de cada LED
PIO np_pio;                     // Variável para referenciar a instância PIO usada
uint sm;                        // Variável para armazenar o número do state machine usado

// Função para calcular o índice do LED na matriz
int getIndex(int x, int y);
// Função para inicializar o PIO para controle dos LEDs
void npInit(uint pin);
// Função para definir a cor de um LED específico
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b);
// Função para limpar (apagar) todos os LEDs
void npClear();
// Função para atualizar os LEDs no hardware
void npWrite();

void display_numerico(int frame); // Função que organiza o display numerico

void core1_entry(); // Função da uart que vai ser utilizada no nucleo 2 do raspberry

void button_irq_handler(uint gpio, uint32_t events); // função de interrupção e debounce dos botões

int main()
{
    i2c_init(I2C_PORT, 400e3); // inicia o i2c
    stdio_init_all(); //Inicialização do stdio
    // Inicialização dos pinos gpio
    gpio_init(BLUE_PIN);
    gpio_init(GREEN_PIN);
    gpio_init(BOTAO_A);
    gpio_init(BOTAO_B);
    // Inicializa a UART com a taxa de transmissão definida
    uart_init(UART_ID, BAUD_RATE);
    // Definição se os pinos são de entrada ou saida
    gpio_set_dir(BLUE_PIN, GPIO_OUT);
    gpio_set_dir(GREEN_PIN, GPIO_OUT);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    // Definição de função dos pinos SDA e SCL
    gpio_set_function(DISPLAY_SDA, GPIO_FUNC_I2C);
    gpio_set_function(DISPLAY_SCL, GPIO_FUNC_I2C);
    // Configura os pinos GPIO para UART
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    // Configuração pull-up dos botões e display
    gpio_pull_up(BOTAO_A);
    gpio_pull_up(BOTAO_B);
    gpio_pull_up(DISPLAY_SDA);
    gpio_pull_up(DISPLAY_SCL);

    ssd1306_t ssd; // Inicialização a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT); // Inicialização do display
    ssd1306_config(&ssd); // Configura display
    ssd1306_send_data(&ssd); // envia dados para o display

    ssd1306_fill(&ssd, false); // limpa o display
    ssd1306_send_data(&ssd);

    bool cor = true;    
    npInit(MATRIX_PIN); // Inicializar o PIO para controle dos LEDs
    multicore_launch_core1(core1_entry); // ativa o nucleo 2

    display_numerico(10); // inicia o display da matriz desligado
    //inicia os leds azul e verde desligados
    gpio_put(GREEN_PIN, false);
    gpio_put(BLUE_PIN, false);
    // aplica a função de interrupção dos botões
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &button_irq_handler);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &button_irq_handler);
    // variaveis para declarar o estado do led no display
    char estadoVerde[9];
    char estadoAzul[9];

    while (true) {
        strcpy(estadoVerde, GREEN_STATE ? "On" : "Off");
        char buffer1[100];  // Tamanho suficiente para armazenar a string
        snprintf(buffer1, sizeof(buffer1), "Led Verde: %s", estadoVerde); // concatena strings

        strcpy(estadoAzul, BLUE_STATE ? "On" : "Off");
        char buffer2[100];  // Tamanho suficiente para armazenar a string
        snprintf(buffer2, sizeof(buffer2), "Led Azul: %s", estadoAzul);

        tight_loop_contents();

        cor = !cor;
        // Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor); // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
        ssd1306_draw_string(&ssd, buffer1, 10, 50);
        ssd1306_draw_string(&ssd, buffer2, 10, 10);
        ssd1306_draw_char(&ssd, ch, 10, 30); // Desenha uma string   
        ssd1306_send_data(&ssd); // Atualiza o display

        if(ch <= '9' && ch >= '0'){
            display_numerico(ch - '0');
        } else{
            display_numerico(10);
        }
        sleep_ms(1000);
    }
}


int getIndex(int x, int y) {
    x = 4 - x; // Inverte as colunas (0 -> 4, 1 -> 3, etc.)
    y = 4 - y; // Inverte as linhas (0 -> 4, 1 -> 3, etc.)
    if (y % 2 == 0) {
        return y * 5 + x;       // Linha par (esquerda para direita)
    } else {
        return y * 5 + (4 - x); // Linha ímpar (direita para esquerda)
    }
}

void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program); // Carregar o programa PIO
    np_pio = pio0;                                         // Usar o primeiro bloco PIO

    sm = pio_claim_unused_sm(np_pio, false);              // Tentar usar uma state machine do pio0
    if (sm < 0) {                                         // Se não houver disponível no pio0
        np_pio = pio1;                                    // Mudar para o pio1
        sm = pio_claim_unused_sm(np_pio, true);           // Usar uma state machine do pio1
    }

    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f); // Inicializar state machine para LEDs

    for (uint i = 0; i < LED_COUNT; ++i) {                // Inicializar todos os LEDs como apagados
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;                                    // Definir componente vermelho
    leds[index].G = g;                                    // Definir componente verde
    leds[index].B = b;                                    // Definir componente azul
}

void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i) {                // Iterar sobre todos os LEDs
        npSetLED(i, 0, 0, 0);                             // Definir cor como preta (apagado)
    }
    npWrite();                                            // Atualizar LEDs no hardware
}

void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {                // Iterar sobre todos os LEDs
        pio_sm_put_blocking(np_pio, sm, leds[i].G);       // Enviar componente verde
        pio_sm_put_blocking(np_pio, sm, leds[i].R);       // Enviar componente vermelho
        pio_sm_put_blocking(np_pio, sm, leds[i].B);       // Enviar componente azul
    }
}

void core1_entry(){
    sleep_ms(1000);
    printf("Digite um caractere: ");
    while(true){
        ch = getchar();// Aguarda a entrada do usuário
            if (ch != EOF) {  // Verifica se um caractere foi recebido
                printf("\nVocê digitou: %c\n", ch);
                printf("Digite outro caractere: ");
            }
        
    }
}

void button_irq_handler(uint gpio, uint32_t events) {
    uint32_t now = to_ms_since_boot(get_absolute_time()); // Obtém tempo atual em ms
    
    if (gpio == BOTAO_A) {
        if (now - last_interrupt_time_a > DEBOUNCE_DELAY) {  // Confere o tempo de debounce
            GREEN_STATE = !GREEN_STATE;
            gpio_put(GREEN_PIN, GREEN_STATE);
            last_interrupt_time_a = now;
            printf("Led Verde: %s\n", GREEN_STATE ? "Ligado" : "Desligado");
        }
    } else if (gpio == BOTAO_B) {
        if (now - last_interrupt_time_b > DEBOUNCE_DELAY) {
            BLUE_STATE = !BLUE_STATE;
            gpio_put(BLUE_PIN, BLUE_STATE);
            last_interrupt_time_b = now;
            printf("Led Azul: %s\n", BLUE_STATE ? "Ligado" : "Desligado");
        }
    }
}

void display_numerico(int frame) {
    int matriz[11][5][5][3] = {
                {
                    {{0, 0, 0}, {193, 192, 191}, {193, 192, 191}, {193, 192, 191}, {0, 0, 0}},
                    {{0, 0, 0}, {193, 192, 191}, {0, 0, 0}, {193, 192, 191}, {0, 0, 0}},
                    {{0, 0, 0}, {193, 192, 191}, {0, 0, 0}, {193, 192, 191}, {0, 0, 0}},
                    {{0, 0, 0}, {193, 192, 191}, {0, 0, 0}, {193, 192, 191}, {0, 0, 0}},
                    {{0, 0, 0}, {193, 192, 191}, {193, 192, 191}, {193, 192, 191}, {0, 0, 0}}
                },
                {
                    {{0, 0, 0}, {0, 101, 13}, {0, 101, 13}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 101, 13}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 101, 13}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 101, 13}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 101, 13}, {0, 101, 13}, {0, 101, 13}, {0, 0, 0}}
                },
                {
                    {{0, 0, 0}, {101, 0, 0}, {101, 0, 0}, {101, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {101, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {101, 0, 0}, {101, 0, 0}, {101, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {101, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {101, 0, 0}, {101, 0, 0}, {101, 0, 0}, {0, 0, 0}}
                },
                {
                    {{0, 0, 0}, {0, 16, 101}, {0, 16, 101}, {0, 16, 101}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 16, 101}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 16, 101}, {0, 16, 101}, {0, 16, 101}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 16, 101}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 16, 101}, {0, 16, 101}, {0, 16, 101}, {0, 0, 0}}
                },
                {
                    {{0, 0, 0}, {97, 0, 90}, {0, 0, 0}, {97, 0, 90}, {0, 0, 0}},
                    {{0, 0, 0}, {97, 0, 90}, {0, 0, 0}, {97, 0, 90}, {0, 0, 0}},
                    {{0, 0, 0}, {97, 0, 90}, {97, 0, 90}, {97, 0, 90}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {97, 0, 90}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {97, 0, 90}, {0, 0, 0}}
                },
                {
                    {{0, 0, 0}, {0, 92, 97}, {0, 92, 97}, {0, 92, 97}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 92, 97}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 92, 97}, {0, 92, 97}, {0, 92, 97}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 92, 97}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 92, 97}, {0, 92, 97}, {0, 92, 97}, {0, 0, 0}}
                },
                {
                    {{0, 0, 0}, {101, 0, 80}, {101, 0, 80}, {101, 0, 80}, {0, 0, 0}},
                    {{0, 0, 0}, {101, 0, 80}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {101, 0, 80}, {101, 0, 80}, {101, 0, 80}, {0, 0, 0}},
                    {{0, 0, 0}, {101, 0, 80}, {0, 0, 0}, {101, 0, 80}, {0, 0, 0}},
                    {{0, 0, 0}, {101, 0, 80}, {101, 0, 80}, {101, 0, 80}, {0, 0, 0}}
                },
                {
                    {{0, 0, 0}, {248, 255, 0}, {248, 255, 0}, {248, 255, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {248, 255, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {248, 255, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {248, 255, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {248, 255, 0}, {0, 0, 0}}
                },
                {
                    {{0, 0, 0}, {34, 255, 0}, {34, 255, 0}, {34, 255, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {34, 255, 0}, {0, 0, 0}, {34, 255, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {34, 255, 0}, {34, 255, 0}, {34, 255, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {34, 255, 0}, {0, 0, 0}, {34, 255, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {34, 255, 0}, {34, 255, 0}, {34, 255, 0}, {0, 0, 0}}
                },
                {
                    {{0, 0, 0}, {255, 162, 0}, {255, 162, 0}, {255, 162, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {255, 162, 0}, {0, 0, 0}, {255, 162, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {255, 162, 0}, {255, 162, 0}, {255, 162, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 162, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {255, 162, 0}, {0, 0, 0}}
                },
                {
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
                    {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}
                }};
    
    // Desenhar o número na matriz de LEDs
    for (int linha = 0; linha < 5; linha++) {
        for (int coluna = 0; coluna < 5; coluna++) {
            int posicao = getIndex(linha, coluna);
            npSetLED(posicao, matriz[frame][coluna][linha][0], matriz[frame][coluna][linha][1], matriz[frame][coluna][linha][2]);
        }
    }
    npWrite(); // Atualizar LEDs no hardware
}