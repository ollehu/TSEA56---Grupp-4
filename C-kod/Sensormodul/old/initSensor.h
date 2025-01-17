#include <avr/io.h>


void init_counter(void)
{
	DDRB |= (1<<DDB3);
	TCCR0A |= (1<<COM0A0)|(1<<WGM01);
	
	TCCR0B &= ~(1<<CS00);
	TCCR0B &= ~(1<<CS01);
	TCCR0B &= ~(1<<CS02);
	OCR0A = 0xFF;
	
	TIMSK0 |= (1<<OCIE0A);
	
}

void initIC(void)
{
	TIMSK1 |= (1<<ICIE1)|(1<<TOIE1);
	TCCR1B |= (1<<ICES1);
	TCCR1B |= (1<<CS10);
}

void SPI_MasterInit(void)
{
	
	unsigned char SPI_send(unsigned char output);
	SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0);
	DDRB |= (1<<DDB7)|(1<<DDB5)|(1<<DDB4);
	DDRB &= ~(1<<DDB6);
	PORTB |= (1<<PORTB4);
	
	PORTB &= ~(1<<PORTB4);
	SPI_send(0x94);
	SPI_send(0x00);
	SPI_send(0x00);
	PORTB |= (1<<PORTB4);
	
}


unsigned char SPI_send(unsigned char output)
{
	SPDR = output;
	/* Wait for transmission complete */
	while(!(SPSR & (1<<SPIF)));
	return SPDR;
}

void initADC(void)
{
	DDRA = 0xE0; // Pin 0-4 input for ADC conversion
	DDRD = 0xBB; // Pin 2 input for INT0
	
	//SPSR = 0x80;
	ADCSRA = 0xCB; // Enables ADC, ADC Start conversion and ADC interrupt enable
	ADMUX = 0x20; // Left adjusted ADC data register (ADLAR = 1)
	EIMSK = (1<<INT0); // Enables interrupt INT0
	EICRA = (1<<ISC01); //Interrupt at low edge
}



void converionStart(void)
{
	PORTB &= ~(1<<PORTB4);
	SPI_send(0x94);
	SPI_send(0x00);
	SPI_send(0x00);
	PORTB |= (1<<PORTB4);
}

unsigned short AR_read(void){
	
	converionStart();
	
	_delay_us(150);
	unsigned char low, high;
	PORTB &= ~(1<<PORTB4);
	SPI_send(0x80);
	high = SPI_send(0x00);
	low = SPI_send(0x00);
	PORTB |= (1<<PORTB4);
	
	return ((high & 0x0F) << 8) + low;
}
