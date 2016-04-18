package view;

import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JTextField;

public class ControlCoefficientPanel extends JPanel{

	private JLabel nameLabel;
	private JTextField valueField;
	
	private String name;
	private double value = -1;
	
	public ControlCoefficientPanel(String name) {
		this.name = name;
		
		nameLabel = new JLabel(name + ": ");
		add(nameLabel);
		
		valueField = new JTextField("" + value);
		add(valueField);
	}
	
	public void updatePanel(){
		nameLabel.setText(name + ": ");
		valueField.setText("" + value);
	}
}