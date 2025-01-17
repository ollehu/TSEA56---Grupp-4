
#include "I2C_master.h"
//#include "constants.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#define F_CPU 14745600UL
#include <util/delay.h>
#include "searchPath-v18.h"

#include <util/delay.h>

int SLA_sensor_R = 0xCB;
int SLA_sensor_W = 0xCA;
int SLA_styr_R = 0xCD;

int counterComputer = 0;
int sendToComputer = 1; //How often sensor data is sent to the computer

int data = 0;
int recieved;
uint8_t computerMessage[3];

volatile int sleep = 0;
volatile int autodrive = 0;
uint8_t switchMode[3] = {0xFC, 0x01, 0x00};
	
uint8_t status = 0;

uint8_t closeClaw[3] = {0xFC, 0x0B, 0x00};
uint8_t stopCommand[3] = {0xFF, 0x00, 0x00};


void sendMap();
/*void btSend(unsigned char data)
{
	// Wait for empty transmit buffer
	while ( !( UCSR0A & (1<<UDRE0)) );
	// Put data into buffer, sends the data
	UDR0 = data;
}*/

void btInit(void){
	//16Mhz, Baud rate = 115.2kbps
	unsigned char baud = 7;

	UBRR0H = (unsigned char)(baud >> 8);
	UBRR0L = (unsigned char)(baud);
	
	// Enable reciever, transmitter and interrupt
	UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);
	
	//8 bit data, no parity and 1 stop bit
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00)|(0<<USBS0);
	sei();
}

ISR(USART0_RX_vect){
	if (data == 0){
		//data = 0;
		computerMessage[data] = UDR0;
		data += 1;
	}  else {
		computerMessage[data] = UDR0;
		data += 1;
		
		if (data == 3){
			if (computerMessage[0] == 252 &&
				computerMessage[1] == 8   &&
				computerMessage[2] == 1){
				
				for(int i = 0; i<3;i++){
					btSend(switchMode[i]);
				}
				
				sendMap();

			} else if (computerMessage[0] == 252 &&
				computerMessage[1] == 15) {
				//ISR(INT0_vect);
				//EIFR |= (1<<INTF0);
				INT0_vect();
				
			} else {
				Master(3,SLA_styr_W,computerMessage);
				
			}
			data = 0;
		}
	}
	
	//recieved = UDR0;
	
	/*if (data == 0){
		if(((autodrive == 0) && (recieved == 255)) || ((autodrive == 1) && (recieved == 252))){
			computerMessage[data] = recieved;
			data = data + 1;
		}
	} else {
		if(((autodrive == 0) & (computerMessage[0] == 255)) | ((autodrive == 1) & (computerMessage[0] == 252))){
			computerMessage[data] = recieved;
			
			if (data < 2){
				data = data + 1;
			} else {
				Master(3,SLA_styr_W,computerMessage);
				data = 0;
			}
		}
	}*/
}

ISR(INT0_vect){
	switch(status){
		case 0:
			status = 1;
			
			PORTD |= (1<<PORTD6);
			PORTD &= ~(1<<PORTD7);
			
			explore();
			break;
		case 1:
			//Do nothing...
			break;
		case 2:			
			status = 3;
			PORTD |= (1<<PORTD6);
			PORTD |= (1<<PORTD7);
			
			Master(3,SLA_styr_W,closeClaw);
			break;
		case 3:
			shortestPathInit();
			shortestPathToTarget();
			status = 4;
			PORTD |= (1<<PORTD6);
			PORTD |= (1<<PORTD7);
			break;
		case 4:
			//Do nothing...
			break;
		default:
			break;
	}
}

ISR(INT1_vect){ //Interrupt from controller module
	switch(status){
		case 0:
			//Do nothing...			
			break;
		case 1:			
			if((map[position[0]][position[1]] == startPositionValue[0]) && (hasFoundTarget() >= 1) && !unexploredPaths()){
				status = 2;
				PORTD &= ~(1<<PORTD6);
				PORTD |= (1<<PORTD7);
				
				Master(3,SLA_styr_W,rotate180);
				updatePath();
			} else {
				explore();
			}
			
			break;
		case 2:
			//Do nothing...
			break;
		case 3:
			//Do nothing...
			break; 
		case 4:
			/*if(lastCommand[1] == claw){
				returnStart = nrOfCoordinates - 1;
			}*/

			if (returnStart != 0xFF){
				if(returnStart == -1){
					Master(3,SLA_styr_W,stopCommand);
					PORTD |= (1<<PORTD4);
				} else {
					returnToStart();
				}
			} else {				
				shortestPathToTarget();
			}
			
			break;
	}

	//SENSORDATA KOMMER I AVBROTTET FR�N SENSORMODULEN!
}

ISR(INT2_vect){ //Interrupt from sensor module
	counterComputer = counterComputer + 1;

	//Get data from module
	Master(19,SLA_sensor_R,sensorData);
	
	//send the collected data to styr module
	if (autodrive == 1){
		Master(19,SLA_styr_W,sensorData);
	}
	//send the collected data to the computer

		for(int i = 0; i < 19; i++){
			btSend(sensorData[i]);
		}

	
}

void sendMap(void)
{
	uint8_t i;
	uint8_t j;
	for(i = 0; i < dimx; i++){
		for(j = 0; j < dimy; j++){
			if (map[i][j] != initialValue){
				btSend(0xFE);
				btSend(i);
				btSend(j);
				btSend(map[i][j]);
			}
		}
	}
}

int main(void)
{	
	TWISetup();
	interruptSetup();
	btInit();
	searchPathInit();
	
	DDRD |= (1<<DDD7)|(1<<DDD6)|(1<<DDD4);
	PORTD &= ~(1<<PORTD7);
	PORTD &= ~(1<<PORTD6);
	PORTD &= ~(1<<PORTD4);
	
	sei();
	
	while(1){
		//Check autonomous mode on/off
		if (PIND & (1<<PIND5)){
			autodrive = 0; // Manual mode
			
			if (switchMode[2] == 1){
				switchMode[2] = 0;
				
				
				//Notify computer
				for(int i = 0; i<3;i++){
					btSend(switchMode[i]);
				}
				
				//Notify controller module
				Master(3,SLA_styr_W,switchMode);
			}
		} else {
			autodrive = 1; // Autonomous mode
			if (switchMode[2] == 0){
				switchMode[2] = 1;
				
				
				//Notify computer
				for(int i = 0; i<3;i++){
					btSend(switchMode[i]);
				}
				
				//Notify controller module
				Master(3,SLA_styr_W,switchMode);
			}
		}
		
		
		if (autodrive == 0) { // Manual mode
			//Do nothing...
		} else if (autodrive == 1) { // Autonomous mode
			//Do nothing...
		}
		
	}
}
