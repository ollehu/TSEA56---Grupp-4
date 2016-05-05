package view;

import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics;

import javax.swing.JComponent;
import javax.swing.UIManager;

import resources.CustomColors;

public class MapVisualElement extends JComponent{

	/**
	 * 245 (0xF5) = unexplored, 244 (0xF4) =  wall, 
	 * 243 (0xF3) = target, 242 = (0xF2) = start, 0-225 = range from start
	 */
	private int value;
	
	private int width = 15;
	private int height = 15;
	
	public MapVisualElement(int value) {
		setPreferredSize(new Dimension(WIDTH, HEIGHT));
		this.value = value;
	}
	
	public void setDimension(int width, int height) {
		this.width = width;
		this.height = height;
		repaint();
	}
	
	@Override
	protected void paintComponent(Graphics graphics) {
		super.paintComponent(graphics);
		
		if(value <= 225){
			graphics.setColor(CustomColors.EXPLORED_BACKGROUND_COLOR);
		} else if(value == 244){
			graphics.setColor(CustomColors.WALL_BACKGROUND_COLOR);
		} else if(value == 243){
			graphics.setColor(CustomColors.TARGET_BACKGROUND_COLOR);
		} else if(value == 242){
			graphics.setColor(CustomColors.START_BACKGROUND_COLOR);
		}
		
		graphics.fillRect(0, 0, width, height);
	}
	
}