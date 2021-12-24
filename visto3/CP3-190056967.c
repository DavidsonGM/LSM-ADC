// 190056967 - David Gonçalves Mendes

#include <msp430.h> 

#define TRUE    1
#define FALSE   0
#define FECHADA 0
#define ABERTA  1
#define SMCLK   1048576L
#define DBC     1000
#define BR_50K  21  //SMCLK/50K  = 21

// Definição do endereço do PCF_8574
#define PCF_ADR1 0x3F
#define PCF_ADR2 0x27
#define PCF_ADR  PCF_ADR2

void adc_config(void);
void tb0_config(void);
void gpio_config(void);
void i2c_config(void);

int sw_mon(void);
void debounce(int valor);
void delay(long limite); // Usada apenas para a inicialização do LCD

int pcf_read(void);
void pcf_write(char dado);
int pcf_teste(char adr);
void lcd_inic(void);
void lcd_aux(char dado);
void lcd_char(char x);
void lcd_esp(char x, char *vt);
void lcd_cursor(char x);
void lcd_cmdo(char x);
void lcd_num(int n, int digits, int size);
void lcd_medidas(void);
char get_esp(void);
void lcd_clear(void);
void lcd_xy(void);

volatile int mx, my, modo, vx, vy, medidas[10], min = 3300, max, flag = 0;

void main(void){
    // Caracteres especiais
    char char_0[]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F};
    char char_1[]={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x00};
    char char_2[]={0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00};
    char char_3[]={0x00, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    char char_4[]={0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00};
    char char_5[]={0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00};
    char char_6[]={0x00, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    char char_7[]={0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer

    tb0_config();
    adc_config();
    gpio_config();
    i2c_config();
    if (pcf_teste(PCF_ADR)==FALSE)
        while(TRUE);        //Travar
    lcd_inic();     //Inicializar LCD

    lcd_esp(0, char_0);
    lcd_esp(1, char_1);
    lcd_esp(2, char_2);
    lcd_esp(3, char_3);
    lcd_esp(4, char_4);
    lcd_esp(5, char_5);
    lcd_esp(6, char_6);
    lcd_esp(7, char_7);
    __enable_interrupt();

    while(1){

        if (flag == 1){
            flag = 0;
            if (modo == 0 || modo == 1)
                lcd_medidas();
            else
                lcd_xy();
        }

        // Monitora a chave para alterar o modo
        if (sw_mon() == TRUE){
            lcd_clear();
            modo += 1;
            if (modo > 2)
                modo = 0;
        }
    }
}

#pragma vector = ADC12_VECTOR // Interrupção 54
__interrupt void calc_medias(void) {
    static unsigned int i = 0;

    // Resetar o mínimo e máximo a cada 10 conversões
    if (i >= 10){
        i = 0;
        max = 0;
        min = 3300;
    }

    P1OUT ^= BIT0; // Inverter o led vermelho

    // Calcular a média das conversões
    mx = (ADC12MEM0 + ADC12MEM2 + ADC12MEM4 + ADC12MEM6 + ADC12MEM8 + ADC12MEM10 + ADC12MEM12 + ADC12MEM14) >> 3; // Dividir por 8
    my = (ADC12MEM1 + ADC12MEM3 + ADC12MEM5 + ADC12MEM7 + ADC12MEM9 + ADC12MEM11 + ADC12MEM13 + ADC12MEM15) >> 3;
    my = 4095 - my; // Invertendo o eixo y

    // Tensões em Vx e Vy (em micro Volts para evitar operações com float)
    vx = (mx*3300L) >> 12; // Dividindo por 4096
    vy = (my*3300L) >> 12;

    if(modo == 0)
        medidas[i] = vx;
    else
        medidas[i] = vy;

    // Obter valores de mínimo e máximo
    if (medidas[i] < min)
        min = medidas[i];
    if (medidas[i] > max)
        max = medidas[i];

    flag = 1;
    i++;
}

// Configurar o ADC12
void adc_config(void){
    ADC12CTL0 = ADC12ON;            // Ligar o conversor AD
    ADC12CTL1 = ADC12MEM0       |   // Iniciar no endereço 0 de memória
                ADC12SHS_3      |   // Timer TB0.1 dispara o ADC
                //ADC12SHP      |   // SHP = 0, Disparo vem direto do SHI (1 para vir do Sample Timer)
                //ADC12ISSH     |   // ISSH = 0, Não invertido
                ADC12DIV_0      |   // Não dividir o clock
                ADC12SSEL_3     |   // SMCLK
                ADC12CONSEQ_3;      // Sequencia de canais com repeticao
    ADC12CTL2 = ADC12RES_2      |   // Resolução de 12 bits (13 clocks)
                ADC12TCOFF;         // Sensor de temperatura desligado

    // Controle da memória 0, selecionada em CTL1
    ADC12MCTL0 = ADC12SREF_0    |   // Referência 0 (VR+ = AVCC e VR- = AVSS)
                 ADC12INCH_0;       // Entrada A0 (P6.0)

    ADC12MCTL1 = ADC12SREF_0    |   // Referência 0 (VR+ = AVCC e VR- = AVSS)
                 ADC12INCH_1;       // A1 (P6.1)

    ADC12MCTL2 = ADC12SREF_0    |
                 ADC12INCH_0;

    ADC12MCTL3 = ADC12SREF_0    |
                  ADC12INCH_1;

    ADC12MCTL4 = ADC12SREF_0    |
                  ADC12INCH_0;

    ADC12MCTL5 = ADC12SREF_0    |
                 ADC12INCH_1;

    ADC12MCTL6 = ADC12SREF_0    |
                 ADC12INCH_0;

    ADC12MCTL7 = ADC12SREF_0    |
                 ADC12INCH_1;

    ADC12MCTL8 = ADC12SREF_0    |
                 ADC12INCH_0;

    ADC12MCTL9 = ADC12SREF_0    |
                  ADC12INCH_1;

    ADC12MCTL10 = ADC12SREF_0   |
                  ADC12INCH_0;

    ADC12MCTL11 = ADC12SREF_0   |
                  ADC12INCH_1;

    ADC12MCTL12 = ADC12SREF_0   |
                   ADC12INCH_0;

    ADC12MCTL13 = ADC12SREF_0   |
                   ADC12INCH_1;

    ADC12MCTL14 = ADC12SREF_0   |
                 ADC12INCH_0;

    ADC12MCTL15 = ADC12EOS      |   // Indicar que a memória 15 é a última
                 ADC12SREF_0    |
                 ADC12INCH_1;

    ADC12IE = ADC12IE15; // Hab interrup no registrador 15

    ADC12CTL0 |= ADC12ENC;          // Habilitar ADC
}
// Configurar o timer TB0.1
void tb0_config(void){
    TB0CTL = TASSEL_2 | ID_0 | MC_1;
    TB0CCR0 = SMCLK/64; //(64 Hz)
    TB0CCTL1 = OUTMOD_6;
    TB0CCR1 = TB0CCR0 >> 1; // TB0CCR0/2
}

void gpio_config(void){
    // Led Vermelho
    P1DIR |=  BIT0;
    P1OUT &= ~BIT0;

    // Chave do Joystick em P6.2
    P6DIR &= ~BIT2;
    P6REN |=  BIT2;
    P6OUT |=  BIT2;
}

// Monitorar SW (P6.2), retorna TRUE se foi acionada
int sw_mon(void){
    static int psw = ABERTA; // Guardar passado de Sw
    if ((P6IN&BIT2) == 0){  // Verificar o estado atual de Sw
        if (psw == ABERTA){
            debounce(DBC);
            psw = FECHADA;
            return TRUE;
        }
    }
    else {
        if (psw == FECHADA){
            debounce(DBC);
            psw = ABERTA;
        }
    }
    return FALSE;
}

// Imprimir uma letra no LCD (x = abcd efgh)
void lcd_char(char x){
    char lsn,msn;   //nibbles
    lsn=(x<<4)&0xF0;        //lsn efgh 0000
    msn=x&0xF0;             //msn abcd 0000
    pcf_write(msn | 0x9);
    pcf_write(msn | 0xD);
    pcf_write(msn | 0x9);

    pcf_write(lsn | 0x9);
    pcf_write(lsn | 0xD);
    pcf_write(lsn | 0x9);
}

// Imprimir no numeros no lcd, o parametro digits indica
// o número de digitos do número a serem printados e o
// parâmetro size indica quantos algarismos tem o numero
// Ignora os algarismo menos signficativos
void lcd_num(int n, int digits, int size){
    int i, dim = 1;
    for (i = 1; i < digits; i++)
        dim *= 10;

//    // Verifica se a dimensão do número é compatível com o número de dígitos a
//    // serem printados, descartando os dígitos menos significativos, caso não seja
//    while (n/dim > 9)
//        n /= 10;
    for (i = digits; i < size; i++)
        n /= 10;
    char c;

    while (dim > 0){
        c = n/dim;
        lcd_char(c + 0x30);
        n -= dim*c;
        dim /= 10;
    }
}

void lcd_medidas(void){
    // Modo != 0 -> tensão é em y, modo = 0 -> tensão em x
    int tensao = modo ? vy : vx;

    // A0/A1
    lcd_cursor(0);
    lcd_char('A');
    lcd_num(modo, 1, 1);
    lcd_char('=');
    lcd_num(tensao, 1, 4);
    lcd_char(',');
    tensao -= 1000*(tensao/1000);   // Zerando bit mais significativo
    lcd_num(tensao, 3, 3);
    lcd_char('V');

    // Valor decimal medido
    lcd_cursor(0xC);
    lcd_num(modo ? my : mx, 4, 4);

    // Minimo
    lcd_cursor(0x40);
    lcd_char('M');
    lcd_char('n');
    lcd_char('=');
    lcd_num(min, 1, 4);
    lcd_char(',');
    lcd_num(min - 1000*(min/1000), 2, 3);

    // Maximo
    lcd_cursor(0x49);
    lcd_char('M');
    lcd_char('x');
    lcd_char('=');
    lcd_num(max, 1, 4);
    lcd_char(',');
    lcd_num(max - 1000*(max/1000), 2, 3);
}

char get_esp(void){
    int aux = my < 2048 ? my : my - 2048;
    return (char) (aux >> 8); // Dividindo por 256 para 8 intervalos iguais
}

void lcd_clear(void){
    int i;
    lcd_cursor(0);
    for(i = 16; i > 0; i--)
        lcd_char(0x20);
    lcd_cursor(0x40);
    for(i = 16; i > 0; i--)
        lcd_char(0x20);
}

// Posicionar cursor
void lcd_cursor(char x){
    lcd_cmdo(0x80 | x);
}

// Enviar um comando (RS=0) para o LCD (x = abcd efgh)
void lcd_cmdo(char x){
    char lsn,msn;   //nibbles
    lsn=(x<<4)&0xF0;        //lsn efgh 0000
    msn=x&0xF0;             //msn abcd 0000
    pcf_write(msn | 0x8);
    pcf_write(msn | 0xC);
    pcf_write(msn | 0x8);

    pcf_write(lsn | 0x8);
    pcf_write(lsn | 0xC);
    pcf_write(lsn | 0x8);
}

// Mapeia no caracter "x" o vetor vt[]
// Reposiciona o cursor em lin=0 e col=0
void lcd_esp(char x, char *vt){
    unsigned int adr,i;
    adr = x<<3;
    lcd_cmdo(0x40 | adr);
    for (i=0; i<8; i++)
        lcd_char(vt[i]);
    lcd_cursor(0);
}

void lcd_xy(void){
    char adr;
    static char previous_adr = 0x10;
    if (my < 2048)
        adr = 0x40;
    else
        adr = 0;
    adr += mx >> 8; // Posicao entre 0 e 15
    if (adr != previous_adr){
        lcd_cursor(previous_adr);
        lcd_char(0x20);
    }
    lcd_cursor(adr);
    lcd_char(get_esp());
    lcd_cursor(0x10);  // Movendo cursor para uma posição fantasma
    previous_adr = adr;
}

// Incializar LCD modo 4 bits
void lcd_inic(void){

    // Preparar I2C para operar
    UCB0I2CSA = PCF_ADR;    //Endereço Escravo
    UCB0CTL1 |= UCTR    |   //Mestre TX
                UCTXSTT;    //Gerar START
    while ( (UCB0IFG & UCTXIFG) == 0);          //Esperar TXIFG=1
    UCB0TXBUF = 0;                              //Saída PCF = 0;
    while ( (UCB0CTL1 & UCTXSTT) == UCTXSTT);   //Esperar STT=0
    if ( (UCB0IFG & UCNACKIFG) == UCNACKIFG)    //NACK?
                while(1);

    // Começar inicialização
    lcd_aux(0);     //RS=RW=0, BL=1
    delay(20000);
    lcd_aux(3);     //3
    delay(10000);
    lcd_aux(3);     //3
    delay(10000);
    lcd_aux(3);     //3
    delay(10000);
    lcd_aux(2);     //2

    // Entrou em modo 4 bits
    lcd_aux(2);     lcd_aux(8);     //0x28
    lcd_aux(0);     lcd_aux(8);     //0x08
    lcd_aux(0);     lcd_aux(1);     //0x01
    lcd_aux(0);     lcd_aux(6);     //0x06
    lcd_aux(0);     lcd_aux(0xF);   //0x0F

    while ( (UCB0IFG & UCTXIFG) == 0)   ;          //Esperar TXIFG=1
    UCB0CTL1 |= UCTXSTP;                           //Gerar STOP
    while ( (UCB0CTL1 & UCTXSTP) == UCTXSTP)   ;   //Esperar STOP
    delay(50);

    lcd_cmdo(0x0E); //Cursor estático
    pcf_write(8);   //Acender Back Light
}

// Auxiliar inicialização do LCD (RS=RW=0)
// *** Só serve para a inicialização ***
void lcd_aux(char dado){
    while ( (UCB0IFG & UCTXIFG) == 0);              //Esperar TXIFG=1
    UCB0TXBUF = ((dado<<4)&0XF0) | BIT3;            //PCF7:4 = dado;
    delay(50);
    while ( (UCB0IFG & UCTXIFG) == 0);              //Esperar TXIFG=1
    UCB0TXBUF = ((dado<<4)&0XF0) | BIT3 | BIT2;     //E=1
    delay(50);
    while ( (UCB0IFG & UCTXIFG) == 0);              //Esperar TXIFG=1
    UCB0TXBUF = ((dado<<4)&0XF0) | BIT3;            //E=0;
}

// Ler a porta do PCF
int pcf_read(void){
    int dado;
    UCB0I2CSA = PCF_ADR;                //Endereço Escravo
    UCB0CTL1 &= ~UCTR;                  //Mestre RX
    UCB0CTL1 |= UCTXSTT;                //Gerar START
    while ( (UCB0CTL1 & UCTXSTT) == UCTXSTT);
    UCB0CTL1 |= UCTXSTP;                //Gerar STOP + NACK
    while ( (UCB0CTL1 & UCTXSTP) == UCTXSTP)   ;   //Esperar STOP
    while ( (UCB0IFG & UCRXIFG) == 0);  //Esperar RX
    dado=UCB0RXBUF;
    return dado;
}

// Escrever dado na porta
void pcf_write(char dado){
    UCB0I2CSA = PCF_ADR;        //Endereço Escravo
    UCB0CTL1 |= UCTR    |       //Mestre TX
                UCTXSTT;        //Gerar START
    while ( (UCB0IFG & UCTXIFG) == 0)   ;          //Esperar TXIFG=1
    UCB0TXBUF = dado;                              //Escrever dado
    while ( (UCB0CTL1 & UCTXSTT) == UCTXSTT)   ;   //Esperar STT=0
    if ( (UCB0IFG & UCNACKIFG) == UCNACKIFG)       //NACK?
                while(1);                          //Escravo gerou NACK
    UCB0CTL1 |= UCTXSTP;                        //Gerar STOP
    while ( (UCB0CTL1 & UCTXSTP) == UCTXSTP)   ;   //Esperar STOP
}

// Testar endereço I2C
// TRUE se recebeu ACK
int pcf_teste(char adr){
    UCB0I2CSA = adr;                            //Endereço do PCF
    UCB0CTL1 |= UCTR | UCTXSTT;                 //Gerar START, Mestre transmissor
    while ( (UCB0IFG & UCTXIFG) == 0);          //Esperar pelo START
    UCB0CTL1 |= UCTXSTP;                        //Gerar STOP
    while ( (UCB0CTL1 & UCTXSTP) == UCTXSTP);   //Esperar pelo STOP
    if ((UCB0IFG & UCNACKIFG) == 0)     return TRUE;
    else                                return FALSE;
}

// Configurar UCSB0 e Pinos I2C
// P3.0 = SDA e P3.1=SCL
void i2c_config(void){
    UCB0CTL1 |= UCSWRST;    // UCSI B0 em ressete
    UCB0CTL0 = UCSYNC |     //Síncrono
               UCMODE_3 |   //Modo I2C
               UCMST;       //Mestre
    UCB0BRW = BR_50K;      //100 kbps
    P3SEL |=  BIT1 | BIT0;  // Use dedicated module
    UCB0CTL1 = UCSSEL_2;    //SMCLK e remove ressete
}

void debounce(int valor){
    volatile int i;
    for (i = 0; i < valor; i++);
}

void delay(long limite){
    volatile long cont=0;
    while (cont++ < limite) ;
}
