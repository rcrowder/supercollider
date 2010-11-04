QView : QObject {
  classvar <>globalKeyDownAction, <>globalKeyUpAction;
  classvar <hSizePolicy;
  classvar <vSizePolicy;
  classvar <orientationDict, <alignmentDict;

  var <parent;
  // general props
  var <font, <palette, <resize = 1, <alpha = 1.0;
  // container props
  var <children, <decorator;
  // top window props
  var <>userCanClose=true, <name, <>deleteOnClose = true;
  // actions
  var <action;
  var <toFrontAction, <endFrontAction;
  var <mouseDownAction, <mouseUpAction, <mouseOverAction, <mouseMoveAction;
  var <keyDownAction, <keyUpAction, <keyModifiersChangedAction;
  var <>keyTyped;
  var <>beginDragAction, <>canReceiveDragHandler, <>receiveDragHandler;
  var <>onClose;

  //TODO
  var <>acceptsClickThrough=false, <>acceptsMouseOver=false,
      <>alwaysOnTop=false;

  *initClass {
    hSizePolicy = [1,2,3,1,2,3,1,2,3];
    vSizePolicy = [1,1,1,2,2,2,3,3,3];

    orientationDict = IdentityDictionary.new;
    orientationDict.put( \horizontal, 1 );
    orientationDict.put( \vertical, 2 );

    alignmentDict = IdentityDictionary.new;
    alignmentDict.put( \left, 16r1 | 16r80 );
    alignmentDict.put( \center, 16r4 | 16r80 );
    alignmentDict.put( \right, 16r2 | 16r80 );
  }

  *new { arg parent, bounds;
    if( parent.notNil ) {
      if( parent.isValid.not ) {^nil;}
    }
    ^super.new( this.qtClass, [parent, bounds.asRect] )
      .initQView( parent );
  }

  *newCustom { arg customArgs;
    ^super.new( this.qtClass, customArgs ).initQView( nil );
  }

  *qtClass { ^"QcDefaultWidget" }

  asView { ^this }

  refresh {
    children.do { |child| child.refresh };
    // Do nothing here. Reimplement if real work needed.
  }

  remove {
    if( parent.notNil ) {
      parent.removeChild( this );
    } {
      QWindow.removeWindow( this );
    };

    this.removeAll;

    if( this.isValid ) {
      onClose.value(this);
      this.destroy;
    };
  }

  // ----------------- properties --------------------------
  font_ { arg f;
    font = f;
    this.setProperty( \font, f );
  }

  background {
    ^palette.windowColor;
  }

  background_ { arg color;
    palette.windowColor = color;
    this.setProperty( \palette, palette );
    this.setProperty( \autoFillBackground, true );
  }

  bounds {
    ^this.getProperty(\geometry, Rect.new)
  }
  bounds_ { arg rect;
    this.setProperty(\geometry, rect)
  }

  moveTo { arg x, y;
    this.bounds_( this.bounds.moveTo( x, y ) );
  }

  resizeTo { arg width, height;
    this.bounds_( this.bounds.resizeTo( width, height ) );
  }

  visible {
    ^this.getProperty(\visible)
  }
  visible_ { arg bool;
    this.setProperty(\visible, bool, false)
  }

  enabled {
    ^this.getProperty(\enabled)
  }
  enabled_ { arg bool;
    this.setProperty(\enabled, bool)
  }

  resize_ { arg anInt;
    this.setProperty(\_qc_hSizePolicy, hSizePolicy[anInt-1]);
    this.setProperty(\_qc_vSizePolicy, vSizePolicy[anInt-1]);
  }

  canFocus {
    var policy = this.getProperty(\focusPolicy);
    ^( policy > 0 )
  }

  canFocus_ { arg bool;
    var policy;
    if( bool ) { policy = 16r1 | 16r2 | 16r8 } { policy = 0 };
    this.setProperty(\focusPolicy, policy);
  }

  focus { arg flag=true;
    this.invokeMethod( \setFocus, flag );
  }

  hasFocus {
    ^this.getProperty( \focus );
  }

  focusColor_ {
    this.nonimpl( "focusColor_" );
  }

  focusColor {
    this.nonimpl( "focusColor" );
    ^Color.new;
  }

  // ------------------ container stuff ----------------------------

  add { arg child;
    children = children.add(child);
    if (decorator.notNil, { decorator.place(child); });
  }

  removeChild { arg child;
    children.remove(child);
  }

  removeAll {
    children.copy.do { |child| child.remove };
  }

  addFlowLayout { arg margin, gap;
    this.decorator_( FlowLayout( this.bounds.moveTo(0, 0), margin, gap ) );
    ^this.decorator;
  }

  decorator_ { arg decor;
    decor.bounds = decor.bounds.moveTo(0, 0);
    decor.reset;
    decorator = decor;
  }

  // ................. top window stuff ............................

  name_ { arg aString;
    name = aString;
    this.setProperty( \windowTitle, aString );
  }

  front {
    this.invokeMethod( \bringFront );
  }

  minimize {
    this.nonimpl("minimize");
  }

  fullScreen {
    this.nonimpl("fullScreen");
  }

  endFullScreen {
    this.nonimpl("endFullScreen");
  }

  alpha_ { arg aFloat;
    alpha = aFloat;
    this.setProperty( \windowOpacity, aFloat );
  }

  close {
    if( deleteOnClose )
      { this.remove; }
      { this.visible_( false ); }
  }

  isClosed {
    if( this.isValid ) {
      ^this.visible.not;
    } {
      ^true;
    }
  }

  setInnerExtent {
    this.nonimpl("setInnerExtent");
  }

  // ----------------- actions .....................................

  action_ { arg func;
    action = func;
    this.connect( "action()", \doAction );
  }

  addAction { arg func, selector=\action;
    this.perform(selector.asSetter, this.perform(selector).addFunc(func));
  }

  removeAction { arg func, selector=\action;
    this.perform(selector.asSetter, this.perform(selector).removeFunc(func));
  }

  keyDownAction_ { arg aFunction;
    keyDownAction = aFunction;
    this.registerEventHandler( QObject.keyDownEvent, \keyDown, true );
  }

  keyUpAction_ { arg aFunction;
    keyUpAction = aFunction;
    this.registerEventHandler( QObject.keyUpEvent, \keyUp, true );
  }

  keyModifiersChangedAction_ { arg aFunction;
    keyModifiersChangedAction = aFunction;
    this.registerEventHandler( QObject.keyDownEvent, \keyDown, true );
    this.registerEventHandler( QObject.keyUpEvent, \keyUp, true );
  }

  mouseDownAction_ { arg aFunction;
    mouseDownAction = aFunction;
    this.registerEventHandler( QObject.mouseDownEvent, \mouseDown );
    this.registerEventHandler( QObject.mouseDblClickEvent, \mouseDown );
  }

  mouseUpAction_ { arg aFunction;
    mouseUpAction = aFunction;
    this.registerEventHandler( QObject.mouseUpEvent, \mouseUp );
  }

  mouseMoveAction_ { arg aFunction;
    mouseMoveAction = aFunction;
    this.registerEventHandler( QObject.mouseMoveEvent, \mouseMove );
  }

  mouseOverAction_ { arg aFunction;
    mouseOverAction = aFunction;
    this.registerEventHandler( QObject.mouseOverEvent, \mouseOver );
  }

  toFrontAction_ { arg aFunction;
    toFrontAction = aFunction;
    this.registerEventHandler( QObject.windowActivateEvent,
                               \onWindowActivateEvent );
  }

  endFrontAction_ { arg aFunction;
    endFrontAction = aFunction;
    this.registerEventHandler( QObject.windowDeactivateEvent,
                               \onWindowDeactivateEvent );
  }

  doAction {
    action.value(this);
  }

  defaultKeyDownAction {}

  defaultKeyUpAction {}

  keyDown { arg char, modifiers, unicode, keycode;
    if( char.size == 1 ) {char = char[0]; keyTyped = char;};

    if( (keycode == 16r1000020) || (keycode == 16r1000021) ||
        (keycode == 16r1000022) || (keycode == 16r1000023 ) )
      { this.keyModifiersChanged( modifiers ) };

    if( keyDownAction.notNil ) {
      ^keyDownAction.value( this, char, modifiers, unicode, keycode );
    } {
      ^this.tryPerform( \defaultKeyDownAction,
                        char, modifiers, unicode, keycode );
    };
  }

  keyUp { arg char, modifiers, unicode, keycode;
    if( char.size == 1 ) {char = char[0]};

    if( (keycode == 16r1000020) || (keycode == 16r1000021) ||
        (keycode == 16r1000022) || (keycode == 16r1000023 ) )
      { this.keyModifiersChanged( modifiers ) };

    if( keyUpAction.notNil ) {
      ^keyUpAction.value( this, char, modifiers, unicode, keycode );
    } {
      ^this.tryPerform( \defaultKeyUpAction,
                        char, modifiers, unicode, keycode );
    };
  }

  keyModifiersChanged { arg modifiers;
    keyModifiersChangedAction.value( this, modifiers);
  }

  mouseDown { arg x, y, modifiers, buttonNumber, clickCount;
    mouseDownAction.value( this, x, y, modifiers, buttonNumber, clickCount );
  }

  mouseUp { arg x, y, modifiers, buttonNumber;
    mouseUpAction.value( this, x, y, modifiers, buttonNumber );
  }

  mouseMove { arg x, y, modifiers;
    mouseMoveAction.value( this, x, y, modifiers );
  }

  mouseOver { arg x, y;
    mouseOverAction.value( this, x, y );
  }
  /* ---------------- private ----------------------- */

  initQView { arg parentArg;

    var handleKeyDown, handleKeyUp;

    parent = parentArg;

    if( parentArg.notNil ) {
      parentArg.add( this );
    } {
      QWindow.addWindow( this );
      QWindow.initAction.value( this );
    };

    palette = QPalette.new;

    this.registerEventHandler( QObject.closeEvent, \onCloseEvent, true );

    handleKeyDown = handleKeyUp = this.overrides( \keyModifiersChanged );
    if( handleKeyDown.not )
      { handleKeyDown = this.overrides( \defaultKeyDownAction ) };
    if( handleKeyUp.not )
      { handleKeyUp = this.overrides( \defaultKeyUpAction )};

    if( handleKeyDown )
      { this.registerEventHandler( QObject.keyDownEvent, \keyDown, true ) };
    if( handleKeyUp )
      { this.registerEventHandler( QObject.keyUpEvent, \keyUp, true ) };

    if( this.overrides( \mouseDown ) ) {
      this.registerEventHandler( QObject.mouseDownEvent, \mouseDown );
      this.registerEventHandler( QObject.mouseDblClickEvent, \mouseDown )
    };
    if( this.overrides( \mouseUp ) )
      {this.registerEventHandler( QObject.mouseUpEvent, \mouseUp )};
    if( this.overrides( \mouseMove ) )
      {this.registerEventHandler( QObject.mouseMoveEvent, \mouseMove )};
    if( this.overrides( \mouseOver ) )
      {this.registerEventHandler( QObject.mouseOverEvent, \mouseOver )};
  }

  onCloseEvent {
    if( userCanClose != false ) {
        if( deleteOnClose != false ) { this.remove; } { ^nil; };
    }{
        ^false;
    };
  }

  onWindowActivateEvent {
    toFrontAction.value(this);
  }

  onWindowDeactivateEvent {
    endFrontAction.value(this);
  }

  overrides { arg symMethod;
    ^ ( this.class.findRespondingMethodFor(symMethod) !=
        QView.findRespondingMethodFor(symMethod) );
  }

  nonimpl { arg methodName;
    this.class.nonimpl( methodName );
  }

  *nonimpl { arg methodName;
    (this.asString ++ ": " ++ methodName.asString ++ " is not implemented yet").postln;
  }
}
