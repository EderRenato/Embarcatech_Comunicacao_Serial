# Embarcatech_Comunicacao_Serial
Repositório criado para a tarefa de Microcontroladores - Comunicação Serial

Desenvolvedor:

*Eder Renato da Silva Cardoso Casar*

# Instruções de compilação

Para compilar o código, são necessárias as seguintes extensões: 

*Plataforma de Desenvolvimento BitDogLab v6.3*

*Cmake*


Após instalá-las basta buildar o projeto pelo CMake.

Após isso, basta conectar a BitDogLab em modo BOOTSEL e dar run.

Na placa, o usuário pode clicar nos botões A e B para alterar o estado ligado/desligado nos leds verde e azul, respectivamente, e informa no Serial Monitor o estado do led.

Nos botões foram adicionados debounce via software para evitar leituras erradas e interrupções para não interromper o fluxo do programa.

Também tem um display OLED 1306 conectado via I2C, que registra os estados dos leds Azul e Verde, além de exibir o caractere enviado via UART no Serial Monitor no PC.

Caso seja enviado um número de 0 a 9, esse número além de ser exibido no display OLED também é exibido em uma matriz de LEDs 5x5.

A funções de captura de dados são aplicadas no segundo núcleo do Raspberry Pi Pico W para evitar interromper o fluxo do programa no primeiro núcleo.

As demais funções foram aplicadas no loop principal do programa.

# Vídeo demonstrativo

https://youtu.be/TTQtet1SDYc