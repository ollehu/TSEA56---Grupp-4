/*
* styrmodulStable.c
*
* Created: 4/18/2016 11:07:55 AM
*  Author: ollul666
*/

#define F_CPU 16000000UL
#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "constants.h"
#include "I2C_slave.h"
#include "PWM.h"
#include "LCD.h"

/************************************************************************/
/*                            CONTROLLER                                */
/************************************************************************/
uint8_t autonomousMode = 1;
volatile uint8_t lastControlCommand = stop;

float P = 0.45;
float D = 1.2;
float K = 0.7;

volatile uint8_t preferredSpeed = 50;
uint8_t preferredRotationSpeed = 70;
uint8_t preferredDistance = 100;

/************************************************************************/
/*                              SENSOR                                  */
/************************************************************************/
uint8_t sideSensors[4];
int16_t forwardSensor;
int16_t oldForwardSensor;

int8_t angularVelocity;


int16_t accumulatedAngle;
int16_t preferredAccumulatedAngle;

uint8_t lastReceivedData[7];

/************************************************************************/
/*                               LCD                                    */
/************************************************************************/
volatile uint8_t madeChange = 0;
uint8_t debugCount = 0;

/************************************************************************/
/*                              HEADER                                  */
/************************************************************************/
void updateSensorData(uint8_t sensorIndex, uint8_t data);
void respondToSensorData();
void respondToControlData(uint8_t command, uint8_t value);
int16_t convertAngle(int8_t value);
void respondToSettingsData(uint8_t identifier, uint8_t value);
void autonomousForward();
void autonomousRotate();
void initInterrupt();
void callMainInterrupt(void);

/************************************************************************/
/*	Interrupt: TWI vector - receive data from main module.

	Get sensor data or control command from main module.
																		*/
/************************************************************************/
ISR(TWI_vect)
{
	
	uint8_t commandType = 0;
	uint8_t commandSubType = 0;
	uint8_t commandValue = 0;
	
	uint8_t dataOrder = 0;
	
	madeChange = 1;
	
	TWCR = (1<<TWEA)|(1<<TWEN)|(0<<TWIE);
	PORTA = (0<<PORTA0);

	while(1){
		//SLAVE RECEIVER
		if((TWSR & 0xF8) == 0x60){
			//SLA_W received, ACK returned
			TWCR |= (1<<TWINT)|(1<<TWEA)|(1<<TWEN);
			TWCR &= ~(1<<TWSTA);
			TWCR &= ~(1<<TWSTO);
		} else if ((TWSR & 0xF8) == 0x80){
			//SLA_W, ACK returned, wait for data
			if (TWDR > 245){
				commandType = TWDR;
				dataOrder = 1;
				
			} else if (commandType == sensorCommand){
				
				 //Should be working:
				if (dataOrder % 2 == 1){
					commandSubType = TWDR;
					dataOrder =  dataOrder + 1;
				} else {
					commandValue = TWDR;
					updateSensorData(commandSubType, commandValue);
					dataOrder = dataOrder + 1;	
					
					if (dataOrder == 15){
						dataOrder = 0;
						respondToSensorData();
					}
				}
				
			} else if (commandType == controlCommand){

				if (dataOrder == 1){
					commandSubType = TWDR;
					dataOrder =  2;
				} else {
					commandValue = TWDR;
					respondToControlData(commandSubType, commandValue);
				}
				
			} else if (commandType == settingCommand){
				
				if (dataOrder == 1){
					commandSubType = TWDR;
					dataOrder =  dataOrder + 1;
				} else {
					commandValue = TWDR;
					respondToSettingsData(commandSubType, commandValue);
				}
			}
			
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
			TWDR = 0x55;
			TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN);
			
		} else if ((TWSR & 0xF8) == 0xB8){
			//Data transmitted, ACK received, transmit data
			TWDR = 0x55;
			TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN);
		} else if ((TWSR & 0xF8) == 0xC0){
			//SLA_R, last byte transmitted
			TWCR |= (1<<TWINT)|(1<<TWEA)|(1<<TWIE);
			break;
		}
		
		while(!(TWCR & (1<<TWINT)));
	}
}

/************************************************************************/
/*	updateSensorData - update vector containing last received data
		with argument data.

	sensorIndex = 1 : Front right IR-sensor
				  2 : Front left IR-sensor
				  3 : Rear right IR-sensor
				  4 : Rear left IR-sensor
				  5 : Front laser sensor (high byte)
				  6 : Front laser sensor (low byte)
				  7 : Angular velocity
																		*/
/************************************************************************/
void updateSensorData(uint8_t sensorIndex, uint8_t data)
{
	lastReceivedData[sensorIndex - 1] = data;
}

/************************************************************************/
/*	respondToSensorData - act on sensor data.

	Fill sideSensors, forwardSensor and angularVelocity.
																		*/
/************************************************************************/
void respondToSensorData()
{
	//IR-sensors
	for(uint8_t sensorIndex = 0; sensorIndex < 4; sensorIndex += 1)
	{
		sideSensors[sensorIndex] = lastReceivedData[sensorIndex];
	}
		
	//Lidar
	forwardSensor = lastReceivedData[4]*128 + lastReceivedData[5];
		
	if (oldForwardSensor == 0){
		oldForwardSensor = forwardSensor;
	}

	//Gyro
	if (lastReceivedData[6] >= 124){
		angularVelocity = lastReceivedData[6] - zeroAngVel;
		} else {
		angularVelocity = -(zeroAngVel - lastReceivedData[6]);
	}
	
	switch(lastControlCommand){
		case stop:
			stopWheels();
			break;
		case forward:
			autonomousForward();
			break;
		case rotation:
			autonomousRotate();
			break;
		default:
			break;
	}
	
	//Update LCD
	madeChange = 1;
}

/************************************************************************/
/*	respondToControlData - act on control data.
	
	autonomousMode = 0:
	  	command = 1 : Forward							value : speed
			      2 : Reverse							value : speed
				  3 : Rotate clockwise					value : speed
				  4 : Rotate c-clockwise				value : speed
				  5 : Forward and right					value : speed
				  6 : Forward and left					value : speed
				  7 : Reverse and right					value : speed
				  8 : Reverse and left					value : speed
				  9 : Control Claw						value : 1 (O)
																0 (C)
				  0 : Stop								value : n/a
				  
	autonomouseMode = 1:
		command = 1 : Autonomous forward				value : n/a
				  2 : Autonomous rotate 180*			value : n/a
				  3 : Autonomous rotate clockwise		value : n/a
				  4 : Autonomous rotate c-clockwise		value : n/a
																		*/
/************************************************************************/
void respondToControlData(uint8_t command, uint8_t value)
{
	if (autonomousMode == 0){
		switch(command){
			case commandForward:
				leftWheelPair(value, 1);
				rightWheelPair(value, 1);
				break;
			case commandReverse:
				leftWheelPair(value, 0);
				rightWheelPair(value, 0);
				break;
			case commandRight:
				leftWheelPair(value, 0);
				rightWheelPair(value, 1);
				break;
			case commandLeft:
				leftWheelPair(value, 1);
				rightWheelPair(value, 0);
				break;
			case commandForwardRight:
				rightWheelPair(value*0.6, 1);
				leftWheelPair(value, 1);
				break;
			case commandForwardLeft:
				rightWheelPair(value, 1);
				leftWheelPair(value*0.6, 1);
				break;
			case commandReverseRight:
				rightWheelPair(value*0.6, 0);
				leftWheelPair(value, 0);
				break;
			case commandReverseLeft:
				rightWheelPair(value, 0);
				leftWheelPair(value*0.6, 0);
				break;
			case commandClaw:
				if (value == 1){
					openPicker();
				} else {
					closePicker();
				}
				break;
			default:
				stopWheels();
				break;
		} 
	} else if (autonomousMode == 1){
		switch(command){
			case commandForward:
				oldForwardSensor = forwardSensor;
				lastControlCommand = forward;
				break;
			case commandReverse:
				lastControlCommand = rotation;
				if (sideSensors[frontRightIndex] > sideSensors[frontLeftIndex]){
					preferredAccumulatedAngle = convertAngle(2);
				} else {
					preferredAccumulatedAngle = convertAngle(-2);
				}
				accumulatedAngle = 0;
				break;
			case commandRight:
				lastControlCommand = rotation;
				preferredAccumulatedAngle = convertAngle(1);
				accumulatedAngle = 0;
				break;
			case commandLeft:
				lastControlCommand = rotation;
				preferredAccumulatedAngle = convertAngle(-1);
				accumulatedAngle = 0;
				break;
			case commandStop:
				lastControlCommand = stop;
				break;
			default:
				break;
		}
	}
}

/************************************************************************/
/*	ConvertAngle - convert integer input to match preferred 
		accumulated angular velocity.

	value = -1 : Rotate 90 left
			 1 : Rotate 90 right
			-2 : Rotate 180 left
			 2 : Rotate 180 right
																		*/
/************************************************************************/
int16_t convertAngle(int8_t value)
{
	if (value == -1) {
		return -3.4*preferredRotationSpeed + 841;
	} else if (value == 1) {
		return 3.4*preferredRotationSpeed - 841;
	} else if (value == 2) {
		return (3.4*preferredRotationSpeed - 841)*2.3;
	} else if (value == -2) {
		return -(3.4*preferredRotationSpeed - 841)*2.3;
	}
	
	return 0;
}

/************************************************************************/
/*	respondToSettingsData - act on settings data.

	identifier = 1 : Set autonomous mode: value = 1 <-> ON
												= 0 <-> OFF
			     2 : Set P value		    : P = value / 100
				 3 : Set D value		    : D = value / 100
				 4 : Set K value		    : K = value / 10
				 5 : Set preferred speed
																		*/
/************************************************************************/
void respondToSettingsData(uint8_t identifier, uint8_t value)
{
	switch(identifier){
		case 1:
			autonomousMode = value;
			break;
		case 2:
			P = ((double) value)/100.0;
			break;
		case 3:
			D = ((double) value)/100.0;
			break;
		case 4:
			K = ((double) value)/10.0;
			break;
		case 5:
			preferredSpeed = value;
			break;
		default:
			break;
	}
}

/************************************************************************/
/*	autonomousForward - autonomously drive forward.

	Prefer to follow left wall, if not available follow right wall.
	If neither are found, continue forward with preferredSpeed. If 
	both walls are found update the preferred distance 
	(preferredDistance) to nearby walls.
	
	Call main module for new control command (callMainInterrupt) each 
	module (40 cm) or when forward sensor detects a wall ahead.
	
	Regulate by adding or subtracting speed from preferredSpeed
	with respect to 
		i) Difference between average distance to wall and 
		   preferredDistance
	   ii) Difference in distance between forward and rear sensor
																		*/
/************************************************************************/
void autonomousForward()
{
	uint8_t frontIndex = noWallsIndex;
	uint8_t backIndex;
	
	if (((oldForwardSensor - forwardSensor > moduleDepth) && 
		 (forwardSensor > (moduleDepth + 5))) || 
		 (forwardSensor < minDistanceForward)){
			 
		//callMainInterrupt();
		lastControlCommand = stop;
		stopWheels();
	}
	
	if ((sideSensors[frontLeftIndex] != maxDistance) && 
		(sideSensors[rearLeftIndex] != maxDistance) ){
			
		frontIndex = frontLeftIndex;
		backIndex = rearLeftIndex;
	} else if ((sideSensors[frontRightIndex] != maxDistance) && 
			   (sideSensors[rearRightIndex] != maxDistance)){
				   
		frontIndex = frontRightIndex;
		backIndex = rearRightIndex;
	}
	
	if (frontIndex == noWallsIndex){
		rightWheelPair(preferredSpeed, 1);
		leftWheelPair(preferredSpeed, 1);
	} else {
	
		// if both walls available, adjust preferred distance
		if (sideSensors[frontRightIndex] != maxDistance && 
			sideSensors[frontLeftIndex] != maxDistance  &&
			sideSensors[rearRightIndex] != maxDistance &&
			sideSensors[rearLeftIndex] != maxDistance ){
			
			preferredDistance = (sideSensors[frontRightIndex] + 
								 sideSensors[frontLeftIndex]  + 
								 sideSensors[rearRightIndex]  + 
								 sideSensors[rearLeftIndex])/4;
		}
	
		uint8_t distance = (sideSensors[frontIndex] + 
							sideSensors[backIndex])/2;
	
		float p_out = P * (distance - preferredDistance);
		float d_out = D * (sideSensors[frontIndex] - 
						   sideSensors[backIndex]);

		int16_t y_out = K * (p_out + d_out);

		if (frontIndex == 0){
			y_out = -y_out;
		}
	
		if(y_out < 0) {
		
			if (preferredSpeed - y_out > maxSpeed){
				rightWheelPair(maxSpeed + y_out, 1);
				leftWheelPair(maxSpeed, 1);
			} else {
				rightWheelPair(preferredSpeed + y_out/2, 1);
				leftWheelPair(preferredSpeed - y_out/2, 1);
			}
	
		} else {
		
			if (preferredSpeed + y_out > maxSpeed){
				rightWheelPair(maxSpeed, 1);
				leftWheelPair(maxSpeed - y_out, 1);
			} else {
				rightWheelPair(preferredSpeed + y_out/2, 1);
				leftWheelPair(preferredSpeed - y_out/2, 1);
			}
		}
	}
}

/************************************************************************/
/*	autonomousRotate - autonomously rotate.

	Rotate autonomously with respect to preferred accumulated angle
	(preferredAccumulatedAngle) and call for new control command
	when rotation is considered done.
	
	Before autonomousRotate is called, preferredAccumulatedAngle should 
	be updated using the convertAngle function. convertAngle sets the
	number of degrees and direction to rotate.
	
	Note: labs = long abs 
																		*/
/************************************************************************/
void autonomousRotate()
{
	if(labs(accumulatedAngle) > labs(preferredAccumulatedAngle)){
		//callMainInterrupt();
		lastControlCommand = stop;
		stopWheels();
	} else if (preferredAccumulatedAngle > 0){
		rightWheelPair(preferredRotationSpeed, 1);
		leftWheelPair(preferredRotationSpeed, 0);
	} else {
		rightWheelPair(preferredRotationSpeed, 0);
		leftWheelPair(preferredRotationSpeed, 1);
	}
	
	accumulatedAngle += angularVelocity;
}

/************************************************************************/
/*	initInterrupt - Initiate interrupt where main module is called.     */
/************************************************************************/
void initInterrupt()
{
	DDRB |= (1<<DDB4);
	PORTB &= ~(1<<PORTB4);
}

/************************************************************************/
/*	callMainInterrupt - Call main module for new control command.       */
/************************************************************************/
void callMainInterrupt(void)
{
	PORTB |= (1<<PORTB4);
	PORTB &= ~(1<<PORTB4);
}

/************************************************************************/
/*	main - Main function.

	Initiate PWM, LCD, interrupt and TWI. Print data to LCD when
	madeChange is set to 1.
																		*/
/************************************************************************/
int main(void)
{
	char bottomRowMessage[16] = "";
	char topRowMessage[16] = "";
	
	TWISetup(mySlaveAdress);
	initPWM();
	initLCD();
	initInterrupt();
	
	sei();

	autonomousMode = 1;
	while(1)
	{
	
		if (madeChange >= 1){
			//sprintf(topRowMessage, "CC:%d S:%u", lastControlCommand, preferredSpeed);
			lcdWriteTopRow("-- Forward: --");
			sprintf(bottomRowMessage, "%u", forwardSensor);
			lcdWriteBottomRow(bottomRowMessage);
			madeChange = 0;
		} 
	}
}