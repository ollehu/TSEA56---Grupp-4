package serialCOM;

import java.util.ArrayList;
import java.util.regex.Pattern;

import javax.swing.JOptionPane;
import javax.swing.text.html.HTMLDocument.HTMLReader.IsindexAction;

import control.Handler;
import jssc.SerialPort;
import jssc.SerialPortEvent;
import jssc.SerialPortEventListener;
import jssc.SerialPortException;
import jssc.SerialPortList;
import jssc.SerialPortTimeoutException;

/**
 * Handles bluetooth communication
 * @author isak
 *
 */
public class SerialPortCOM {

	private Handler handler;

	private static SerialPort serialPort;

	// standard port settings for firefly
	private int baudRate = 115200;
	private int numberOfDataBits = 8;
	private int numberOfStopBits = 1;
	private int numberOfParityBits = 0;

	public SerialPortCOM(Handler handler) {
		this.handler = handler;
	}

	/**
	 * Connects to the selected port
	 * 
	 * @param portName port selected by user through GUI
	 * @throws SerialPortException
	 */
	public void connectToSerialPort(String portName) throws SerialPortException {
		serialPort = new SerialPort(portName);

		// open port for communication
		serialPort.openPort();
		// baundRate, numberOfDataBits, numberOfStopBits, parity
		serialPort.setParams(baudRate, numberOfDataBits, numberOfStopBits, numberOfParityBits);
		// byte data transfer
		serialPort.setFlowControlMode(SerialPort.FLOWCONTROL_RTSCTS_IN | 
				SerialPort.FLOWCONTROL_RTSCTS_OUT);
		// add port listener
		serialPort.addEventListener(new PortReader(), SerialPort.MASK_RXCHAR);

		// ping robot
		sendToRobot(CommunicationID.CONTROL_SETTING, ControlSettingID.PING, 1);
	}

	/**
	 * Closes port. Called on program exit
	 */
	public void closeSerialPort() {
		if(serialPort != null) {
			try {
				serialPort.closePort();
				System.out.println("Closing port");
			} catch (SerialPortException e) {

			}
		}
	}

	/**
	 * Sends a command to the robot
	 * 
	 * @param dataID "Kommunikations-ID"
	 * @param commandID individual ID
	 * @param value value being sent (0-245)
	 * @throws SerialPortException
	 */
	public void sendToRobot(int dataID, int commandID, int value) throws SerialPortException {
		// send dataID byte
		sendData(convertIntToByte(dataID));

		// send command byte
		sendData(convertIntToByte(commandID));

		// send value byte
		sendData(convertIntToByte(value));
	}

	private byte convertIntToByte(int data) {
		byte binaryByte = (byte) (data & (0xFF));
		return binaryByte;
	}

	private void sendData(byte data) throws SerialPortException {
		if(serialPort != null) {
			serialPort.writeByte(data);
		}
		System.out.println("Data sent: " + (data & 0xFF));
	}

	/**
	 * Listens for port events
	 * 
	 * @author Isak
	 *
	 */
	private class PortReader implements SerialPortEventListener {

		@Override
		public void serialEvent(SerialPortEvent event) {
			if(event.isRXCHAR() && event.getEventValue() > 0) {
				try {
					byte[] communicationsIDByte = serialPort.readBytes(1);
					int communicationsID = Byte.toUnsignedInt(communicationsIDByte[0]);
					
					// get byte string length
					int byteStringLength = 100;
					
					if(communicationsID == CommunicationID.CONTROL_SETTING) {
						byteStringLength = 2;
					} else if(communicationsID == CommunicationID.SENSOR_DATA) {
						byteStringLength = 14;
					} else if(communicationsID == CommunicationID.MAP_DATA) {
						byteStringLength = 3;
					} else if(communicationsID == CommunicationID.CONTROL_DATA) {
						byteStringLength = 2;
					} else {
						serialPort.purgePort(SerialPort.PURGE_RXCLEAR);
					}
					
					byte[] receivedData = new byte[byteStringLength];
					
					try {
						receivedData = serialPort.readBytes(byteStringLength, 70);
					} catch (SerialPortTimeoutException e) {
//						System.out.println("BT time out. ID: " + communicationsID);
						handler.getLogWriter().appendToLog("BT time out. ID: " + communicationsID);
						return;
					}
					
					if(communicationsID == CommunicationID.CONTROL_SETTING) {
						//TODO handle switch from auto/man
						updateControlSettings(receivedData);
					} else if(communicationsID == CommunicationID.SENSOR_DATA) {
						//TODO handle sensor array
						updateSensorValues(receivedData);
					} else if(communicationsID == CommunicationID.MAP_DATA) {
						//TODO handle map data
						updateMap(receivedData);
					} else if(communicationsID == CommunicationID.CONTROL_DATA) {
						System.out.println("Sensorer: " + handler.getAnimator().getTablePanel().getLatestSensorValues());
						System.out.println("Styrkommando: " + Byte.toUnsignedInt(receivedData[0]));
						System.out.println("");
						handler.getLogWriter().appendToLog("Styrkommando: " + Byte.toUnsignedInt(receivedData[0]));
						handler.getLogWriter().appendToLog("Sensorer: " + handler.getAnimator().getTablePanel().getLatestSensorValues());
					}
					
				}
				catch (SerialPortException ex) {
					System.out.println("Error in receiving string from COM-port: " + ex);
				}
			}
		}

	}

	private void updateControlSettings(byte[] receivedData) {
		int controlSettingID = Byte.toUnsignedInt(receivedData[0]);
		int value = Byte.toUnsignedInt(receivedData[1]);
		
		if(controlSettingID == ControlSettingID.AUTONOMOUS_MODE) {
			boolean isAutonomousModeOn = value == 1;
			
			handler.setAutomousMode(isAutonomousModeOn);
			if(isAutonomousModeOn) {
				handler.getLogWriter().appendToLog("Autonomous mode on");
				JOptionPane.showMessageDialog(handler.getAnimator().getFrame(),
						"Autonomous mode: on!",
						"Autonomous mode",
						JOptionPane.INFORMATION_MESSAGE);
			} else {
				handler.getLogWriter().appendToLog("Autonomous mode off");
				JOptionPane.showMessageDialog(handler.getAnimator().getFrame(),
						"Autonomous mode: off!",
						"Autonomous mode",
						JOptionPane.INFORMATION_MESSAGE);
			}
		} else if (controlSettingID == ControlSettingID.DEBUG_MODE) {
			boolean isDebugModeOn = value == 1;
			
			handler.getAnimator().setDebugMode(isDebugModeOn);
		}
		
		
		
	}

	private void updateSensorValues(byte[] receivedData) {
		
		// convert byte to unsigned ints
		int[] sensorValues = new int[6];
		int j = 0;
		
		for(int i = 1; i < receivedData.length; i+=2) {
			if(i == 9) {
				sensorValues[j] = Byte.toUnsignedInt(receivedData[i]) * 128;
			} else if(i == 11) {
				sensorValues[j] += Byte.toUnsignedInt(receivedData[i]);
				j++;
			} else if(i == 13){
				sensorValues[j] = Byte.toUnsignedInt(receivedData[i]) - 124;
				j++;
			} else {
				sensorValues[j] =  Byte.toUnsignedInt(receivedData[i]);
				j++;
			}
			
		}
		
		// update graph panel
		handler.getAnimator().getGraphPanel().updateSensorValues(sensorValues);
		handler.getAnimator().getTablePanel().updateSensorValues(sensorValues);
	}

	private void updateMap(byte[] receivedData) {
		// get x- and y-coordinate
		int xCoordinate = Byte.toUnsignedInt(receivedData[0]);
		int yCoordinate = Byte.toUnsignedInt(receivedData[1]);
		
		int value = Byte.toUnsignedInt(receivedData[2]);
	
		// update map
		handler.getAnimator().getMapPanel().getMap().update(xCoordinate, yCoordinate, value);
		System.out.println("Uppdaterat kartan! X: " + xCoordinate + ", Y: " + yCoordinate + ", value: " + value);
	}

	public SerialPort getSerialPort() {
		return serialPort;
	}

}
