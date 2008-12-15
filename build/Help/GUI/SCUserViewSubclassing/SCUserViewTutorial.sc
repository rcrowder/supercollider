// How to subclass SCUserView to make custom GUI interfaces
// For many purposes you can use this as a template, and simply adjust the methods. -  by jostM

MyWidget : SCUserView {

	// (1) Setup instance vars appropriate to your widget. Make sure to define value.
	var <>step, <>value=0, <>leftColor, <>rightColor, <>thumbWidth=7;		  
	
	// (2) Set the viewClass to SCUserView
	*viewClass { ^SCUserView } // this ensures that SCUserView's primitive is called 
	
	
	// (3) Set up your view
	init { |argParent, argBounds|
	
		super.init(argParent, argBounds);  
				
		// this must accord with your mouse and drawing functions
		this.relativeOrigin_( false ); 

		// set defaults of your instance variable
		rightColor=Color.grey(0.8);
		leftColor=Color.grey(0.2);
		step=this.pixelStep;
		
		// set the draw function of the SCUserView
		this.drawFunc={ this.draw}; 
	}
	
	
	// (4) define a drawing function for SCPen
	draw{ 
	
		SCPen.addRect(this.bounds);
		SCPen.fillAxialGradient(
				((this.bounds.width*value)+thumbWidth+this.bounds.left) @ this.bounds.top,
				((this.bounds.width*value)-thumbWidth+this.bounds.left) @ this.bounds.top,
				rightColor,
				leftColor);

	}
		
		
	// (5) define typical widget methods  (only those you need or adjust as needed)
	valueAction_{ arg val; // most widgets have this
		value=val;
		this.doAction;
		this.refresh;
	}
	
			// these are like in SCSlider
	increment { |zoom=1| ^this.valueAction = this.value + (max(this.step, this.pixelStep) * zoom) }
	decrement { |zoom=1| ^this.valueAction = this.value - (max(this.step, this.pixelStep) * zoom) }

	pixelStep {  // like in SCSlider
		var bounds = this.bounds; 
		^(bounds.width-1).reciprocal
	}
	
	
	// (6) override mouseActions
	mouseDown{ arg x, y, modifiers, buttonNumber, clickCount;
		var newVal;
		// this allows for user defined mouseDownAction
		mouseDownAction.value(this, x, y, modifiers, buttonNumber, clickCount); 

		// set the value and do the action
		(modifiers == 256).if{ // restrict to no modifier
		
			newVal= x.linlin(this.bounds.left,this.bounds.left+this.bounds.width,0,1); 
			// translates the mouse position in pixels to a relative value between 0 and 1
			
			if (newVal != value) {this.valueAction_(newVal)}; // only do something if the value changed
		};
	}
	
	mouseMove{ arg x, y, modifiers, buttonNumber, clickCount;
		var newVal;  
		// this allows for user defined mouseMoveAction
		mouseMoveAction.value(this, x, y, modifiers, buttonNumber, clickCount);
		
		// set the value and do the action
		(modifiers == 256).if{ // restrict to no modifier
		
			newVal= x.linlin(this.bounds.left,this.bounds.left+this.bounds.width,0,1); 
			// translates the mouse position in pixels to a relative value between 0 and 1
			
			if (newVal != value) {this.valueAction_(newVal)}; // only do something if the value changed
		};
		
	}
	
	// (7) define default key actions
	// make sure to return "this", if successful, and nil if not successful
	defaultKeyDownAction { arg char, modifiers, unicode,keycode;
		if (unicode == 16rF700, { this.increment; ^this });
		if (unicode == 16rF703, { this.increment; ^this });
		if (unicode == 16rF701, { this.decrement; ^this });
		if (unicode == 16rF702, { this.decrement; ^this });
		
		^nil		// bubble if it's an invalid key
	}
	
	// (8) define drag and drop
	defaultGetDrag {^value} // what to drag
	defaultCanReceiveDrag  {^currentDrag.isNumber} // when to receive
	defaultReceiveDrag { this.valueAction = currentDrag;} // what to do on receiving
	

}