#define F_CPU 8000000UL

#include <avr/io.h>
//#include <asf.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "initSensor.h"
#include "I2C_slave.h"

#define SCK PORTB7
#define MISO PORTB6
#define MOSI PORTB5
#define SS PORTB4

int sensorData[15];
int slaveAddress = 0xCA;

volatile const uint8_t adc1 = (1<<ADLAR) | (0<<MUX2)|(0<<MUX1)|(1<<MUX0);
volatile const uint8_t adc2 = (1<<ADLAR) | (0<<MUX2)|(1<<MUX1)|(0<<MUX0);
volatile const uint8_t adc3 = (1<<ADLAR) | (0<<MUX2)|(1<<MUX1)|(1<<MUX0);
volatile const uint8_t adc4 = (1<<ADLAR) | (1<<MUX2)|(0<<MUX1)|(0<<MUX0);

int state = 0;
int ADC_conversion = 0;
int detected = 0;
volatile unsigned int counter = 0; 
double total = 0;

double SI_IR1;
double SI_IR2;
double SI_IR3;
double SI_IR4;

volatile uint8_t Distance_1 = 0;
double Distance_2 = 0;
double Distance_3 = 0;
double Distance_4 = 0;

double SI_IR11 = 0;
double SI_IR12 = 0;
double SI_IR13 = 0;
double SI_IR14 = 0;
double SI_IR15 = 0;
int SI_IR1_Final = 0;

double SI_IR21 = 0;
double SI_IR22 = 0;
double SI_IR23 = 0;
double SI_IR24 = 0;
double SI_IR25 = 0;
int SI_IR2_Final = 0;

double SI_IR31 = 0;
double SI_IR32 = 0;
double SI_IR33 = 0;
double SI_IR34 = 0;
double SI_IR35 = 0;
int SI_IR3_Final = 0;

double SI_IR41 = 0;
double SI_IR42 = 0;
double SI_IR43 = 0;
double SI_IR44 = 0;
double SI_IR45 = 0;
int SI_IR4_Final = 0;


//int angular_velocity = 0;
int SI_lidar;
uint16_t forward_distance;
uint16_t up_value, down_value;
uint16_t pwm16bit = 65535;
int overflow_count = 0;
int count_1 = 0;

uint32_t answer;

int firstRun = 0;

unsigned char SPI_send(unsigned char output);
void SPI_MasterInit(void);
void initIC(void);
void initADC(void);
unsigned short AR_read(void);

ISR(TWI_vect){
	TWCR = (1<<TWEA)|(1<<TWEN)|(0<<TWIE);
	//PORTA = (0<<PORTA0);
	
	int counter = 0;
	
	while(1){
		//SLAVE RECEIVER
		if((TWSR & 0xF8) == 0x60){
			//SLA_W received, ACK returned
			TWCR |= (1<<TWINT)|(1<<TWEA)|(1<<TWEN);
			TWCR &= ~(1<<TWSTA);
			TWCR &= ~(1<<TWSTO);
			} else if ((TWSR & 0xF8) == 0x80){
			//SLA_W, ACK returned, wait for data
			TWCR |= (1<<TWINT)|(1<<TWEA)|(1<<TWEN);
			} else if ((TWSR & 0xF8) == 0x80){
			//SLA_W, NOT ACK returned
			
			//What happens here?
			} else if ((TWSR & 0xF8) == 0xA0){
			//STOP or repeated START
			TWCR |= (1<<TWINT)|(1<<TWEA)|(1<<TWIE);
			break;
			
			
			//SLAVE TRANSMITTER
			} else if ((TWSR & 0xF8) == 0xA8){
			//SLA_R received, ACK returned, transmit data
			TWDR = sensorData[counter];
			counter = counter + 1;
			TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN);
			} else if ((TWSR & 0xF8) == 0xB8){
			//Data transmitted, ACK received, transmit data
			TWDR = sensorData[counter];
			counter = counter + 1;
			TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN);
			} else if ((TWSR & 0xF8) == 0xC0){
			//SLA_R, last byte transmitted
			TWCR |= (1<<TWINT)|(1<<TWEA)|(1<<TWIE);
			break;
		}
		
		while(!(TWCR & (1<<TWINT)));
	}
}

ISR (TIMER1_CAPT_vect)
{

	
	if (TCCR1B & (1<<ICES1)){
		//Rising edge
		up_value = ICR1;
		TCCR1B &= ~(1<<ICES1);
		overflow_count = 0;
		} else {
		//Falling edge
		down_value = ICR1;
		TCCR1B |= (1<<ICES1);
		forward_distance = down_value - up_value; //+ pwm16bit*overflow_count;
		
		
	}
}


ISR (INT0_vect)
{
	if (firstRun <= 1){
	if(!(EICRA & (1<<ISC00))) //Om nedflank
	{
		TCCR0B |= (0<<CS02)|(1<<CS01)|(0<<CS00);
		EICRA |= (1<<ISC00);
		//PORTB |= (1<<PORTB1);
	}  else if(EICRA & (1<<ISC00)){

		TCCR0B &= ~(1<<CS00);
		TCCR0B &= ~(1<<CS01);
		TCCR0B &= ~(1<<CS02);
		EICRA &= ~(1<<ISC00);
		//PORTB &= ~(1<<PORTB1);	
		//counter = 0;
	}
	firstRun = firstRun + 1;
	}

}

ISR (TIMER0_COMPA_vect)
{
	PORTB ^= (1<<PORTB1);
	counter = counter + 1;
	//PORTB &= ~(1<<PORTB1);
	
}

ISR (TIMER1_OVF_vect)
{
	overflow_count++;
}

ISR(ADC_vect){ //IR-SENSOR
	
	CLKPR = 0x06;
	

	if(ADMUX == adc1){
		Distance_1 = ADCH;
		ADMUX = adc2;
		
	}
	else if(ADMUX == adc2){
		Distance_2 = ADCH;
		ADMUX = adc3;
	}
	else if(ADMUX == adc3){
 		Distance_3 = ADCH;
		ADMUX = adc4;
	}
	else{
		Distance_4 = ADCH;
		ADMUX = adc1;
	}
	
	ADCSRA |= (1<<ADSC);	
}

int main (void)
{
	
	DDRB = (1<<PORTB0);
	DDRB |= (1<<PORTB1);
	
	sensorData[0] = 253;

	TWISetup(slaveAddress);
	initADC();
	initIC();
	init_counter();
	
	
	sei(); // Enables global interrupt

	SPI_MasterInit();
	sei();
	while(1) { 
		PORTB |= (1<<PORTB0);
		
		answer = AR_read();
		total = total + answer - 1996.5;
		
		sensorData[9] = 5;
		sensorData[10] = 10*(0.0127*forward_distance - 4.3678); //SI_lidar
		SI_IR1 = 10*(-0.000021834*Distance_1*Distance_1*Distance_1+0.0065*Distance_1*Distance_1 -0.7227*Distance_1 + 35.016);
		SI_IR2 = 10*(-0.000027779*Distance_2*Distance_2*Distance_2+0.0077*Distance_2*Distance_2 -0.7956*Distance_2 + 35.8363);
		SI_IR3 = 10*(-0.000025789*Distance_3*Distance_3*Distance_3+0.0077*Distance_3*Distance_3 -0.8293*Distance_3 + 37.8186);
		SI_IR4 = 10*(-0.00002338455*Distance_4*Distance_4*Distance_4+0.0071*Distance_4*Distance_4 -0.786*Distance_4 + 37.0525);
		
		
		if ((counter>=15)&(counter<=19)){
			detected = 1;
		} else {
			detected = 0;
		}
		
		if(count_1 == 0){
			SI_IR11 = SI_IR1;
			SI_IR21 = SI_IR2;
			SI_IR31 = SI_IR3;
			SI_IR41 = SI_IR4;
			count_1 = count_1+1;
		}
		else if(count_1 == 1){
			SI_IR12 = SI_IR1;
			SI_IR22 = SI_IR2;
			SI_IR32 = SI_IR3;
			SI_IR42 = SI_IR4;
			count_1 = count_1 + 1;
		}
		else if(count_1 == 2){
			SI_IR13 = SI_IR1;
			SI_IR23 = SI_IR2;
			SI_IR33 = SI_IR3;
			SI_IR43 = SI_IR4;
			count_1 = count_1 + 1;
		}
		else if(count_1 == 3){
			SI_IR14 = SI_IR1;
			SI_IR24 = SI_IR2;
			SI_IR34 = SI_IR3;
			SI_IR44 = SI_IR4;
			count_1 = count_1 + 1;
		}
		else if(count_1 == 4){
			SI_IR15 = SI_IR1;
			SI_IR25 = SI_IR2;
			SI_IR35 = SI_IR3;
			SI_IR45 = SI_IR4;
			count_1 = 0;
		}
		
		
		SI_IR1_Final = SI_IR11/5 + SI_IR12/5 + SI_IR13/5 + SI_IR14/5 + SI_IR15/5; //H�ger fram
		SI_IR2_Final = SI_IR21/5 + SI_IR22/5 + SI_IR23/5 + SI_IR24/5 + SI_IR25/5; //H�ger bak
		SI_IR3_Final = SI_IR31/5 + SI_IR32/5 + SI_IR33/5 + SI_IR34/5 + SI_IR35/5; //V�nster fram
		SI_IR4_Final = SI_IR41/5 + SI_IR42/5 + SI_IR43/5 + SI_IR44/5 + SI_IR45/5; // V�nster bak
		
		if (SI_IR1_Final > 230)
		{
			sensorData[1] = 1;
			sensorData[2] = 245;
		} else if (SI_IR1_Final < 40)
		{
			sensorData[1] = 1;
			sensorData[2] = 0;
		}
		
		
		if (SI_IR2_Final > 230)
		{
			sensorData[5] = 3;
			sensorData[6] = 245;
		} else if (SI_IR2_Final < 40)
		{
			sensorData[5] = 3;
			sensorData[6] = 0;
		}
		
				
		if (SI_IR3_Final > 230)
		{
			sensorData[3] = 2;
			sensorData[4] = 245;
		} else if (SI_IR3_Final < 40)
		{
			sensorData[3] = 2;
			sensorData[4] = 0;
		}
			
						
		if (SI_IR4_Final > 230)
		{
			sensorData[7] = 4;
			sensorData[8] = 245;
		} else if (SI_IR4_Final < 40)
		{
			sensorData[7] = 3;
			sensorData[8] = 0;
		}
		
		for(int i = 1;i<15;i++){
			sensorData[i] = i;
		}

		DDRD = (1<<PORTD0);
		_delay_ms(2000);
		PORTD = (1<<PORTD0);
		_delay_ms(10);
		PORTD = (0<<PORTD0);
		while(1);

		
		//Receive 90*-rotation start signal
		//answer = AR_read();
		//total = total + answer - 1996.5;
		
		//When 90* rotation -> send stop signal
		/*if(total < -18300){ //Left
		stop;
		}
		else if(total > 18300){
		stop;
		} */
		
		
		//Receive 180*-rotation start signal
		//answer = AR_read();
		//total = total + answer - 1996.5;
		
		//When 180* rotation -> send stop signal
		/*if(total < -36600){ //Left
		stop;
		}
		else if(total > 36600){
		stop;
		} */
		
		

	}

}
