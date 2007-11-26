// Since SC v3.2 dev, Document is an ABSTRACT class. Can't be instantiated directly. 
// Subclasses provide the editor-specific implementation, e.g. CocoaDocument for the standard Mac interface.
// Subclasses also (in their SC code files) add a "implementationClass" method to Document to tell it to use them.
Document {

	classvar <dir="", <wikiDir="", <allDocuments, <>current;
	classvar <>globalKeyDownAction, <> globalKeyUpAction, <>initAction;
	
	classvar <>autoRun = true;
	classvar <>wikiBrowse = true;

	//don't change the order of these vars:
	var <dataptr, <>keyDownAction, <>keyUpAction, <>mouseDownAction, <>toFrontAction, <>endFrontAction, <>onClose;
	
	var <stringColor;
// 	var >path, visible; 
// replaced by primitves: title, background
	var <editable;
	
	*startup {
		var num, doc;
		allDocuments = [];
		num = this.numberOfOpen;
		num.do({arg i;
			doc = this.newFromIndex(i);
		});
	}
	*open { arg path, selectionStart=0, selectionLength=0;
		^Document.implementationClass.prBasicNew.initFromPath(path, selectionStart, selectionLength)
	}
	
	*new { arg title="Untitled", string="", makeListener=false;
		^Document.implementationClass.new(title, string, makeListener);
	}
	*prBasicNew {
		^super.new
	}
	
//class:

	*dir_ { arg path; path = path.standardizePath;
		if(path == "") {dir = path } {
			if(pathMatch(path).isEmpty) { ("there is no such path:" + path).postln } {
				dir = path ++ "/"
			}
		}
	}
	
	*wikiDir_ { arg path; path = path.standardizePath;
		if(path == "") {wikiDir = path } {
			if(pathMatch(path).isEmpty) { ("there is no such path:" + path).postln } {
				wikiDir = path ++ "/"
			}
		}
	}
	
	*standardizePath { arg p;
		var pathName;
		pathName = PathName.fromOS9(p.standardizePath);
		^if(pathName.isRelativePath,{
			dir  ++ pathName.fullPath
		},{
			pathName.fullPath
		})
	}
	*abrevPath { arg path;
		if(path.size < dir.size,{ ^path });
		if(path.copyRange(0,dir.size - 1) == dir,{
			^path.copyRange(dir.size, path.size - 1)
		});
		^path
	}
	
	*openDocuments {
		^allDocuments
	}
	
	*hasEditedDocuments {
		allDocuments.do({arg doc, i;
			if(doc.isEdited, {
				^true;
			})
		})
		^false
	}
	
	*closeAll {arg leavePostWindowOpen = true;
		allDocuments.do({arg doc, i;
			if(leavePostWindowOpen.not, {
				doc.close;
			},{
				if(doc.isListener.not,{
					doc.close;
				})
			});
		})
	}
	
	*closeAllUnedited {arg leavePostWindowOpen = true;
		var listenerWindow;
		allDocuments.do({arg doc, i;
			if(doc.isListener,{ 
				listenerWindow = doc;
			},{
				if(doc.isEdited.not, {
					doc.close;
					});
			})
		});
		if(leavePostWindowOpen.not, {
			listenerWindow.close;
		})
	}
	
	*listener {
		^allDocuments[this.implementationClass.prGetIndexOfListener];
	}
	isListener {
		^allDocuments.indexOf(this) == this.class.prGetIndexOfListener
	}
	
//document setup	
	path{
		^this.prGetFileName
	}
	path_{|apath|
		this.prSetFileName(apath);
	}
	dir { var path = this.path; ^path !? { path.dirname } }	
	title {
		^this.prGetTitle
	}
	
	title_ {arg argName;
		this.prSetTitle(argName);
	}
	
	background_ {arg color;
		this.prSetBackgroundColor(color);
	}
	background{
		var color;
		color = Color.new;
		this.prGetBackgroundColor(color);
		^color;
	}
	
	stringColor_ {arg color, rangeStart = -1, rangeSize = 0;
		stringColor = color;
		this.setTextColor(color,rangeStart, rangeSize);
	}
	bounds {
		^this.prGetBounds(Rect.new);
	}
	bounds_ {arg argBounds;
		^this.prSetBounds(argBounds);
	}
//interaction:	

	close {
		this.prclose
	}
	
	front {
		^this.subclassResponsibility
	}
	
	unfocusedFront {
		^this.subclassResponsibility
	}
	
	alwaysOnTop_{|boolean=true|
		^this.subclassResponsibility
	}
	
	alwaysOnTop{
		^this.subclassResponsibility
	}
		
	syntaxColorize {
		^this.subclassResponsibility
	}
	
	selectLine {arg line;
		this.prSelectLine(line);
	}
	
	selectRange {arg start=0, length=0;
		^this.subclassResponsibility
	}
	
	editable_{arg abool=true;
		editable = abool;
		this.prIsEditable_(abool);
	}
	removeUndo{
		^this.subclassResponsibility
	}
	
	promptToSave_{|bool|
		^this.subclassResponsibility
	}
	
	promptToSave{
		^this.subclassResponsibility
	}	
	
	underlineSelection{
		^this.subclassResponsibility
	}
	
// state info
	isEdited {
		^this.subclassResponsibility
	}
	isFront {
		^Document.current === this
	}
	
	selectionStart {
		^this.selectedRangeLocation
	}
	
	selectionSize {
		^this.selectedRangeSize
	}
	
	string {arg rangestart, rangesize = 1;
		if(rangestart.isNil,{
		^this.text;
		});
		^this.rangeText(rangestart, rangesize);
	}
	
	string_ {arg string, rangestart = -1, rangesize = 1;
		this.insertTextRange(string, rangestart, rangesize);
	}
	selectedString {
		^this.selectedText
	}
	
	
	font_ {arg font, rangestart = -1, rangesize=0;
		this.setFont(font, rangestart, rangesize)
	}
		 
	selectedString_ {arg txt;
		this.prinsertText(txt)
	}
	
	currentLine {
		var start, end, str, max;
		str = this.string;
		max = str.size;
		end = start = this.selectionStart;
		while { 
			str[start] !== Char.nl and: { start >= 0 }
		} { start = start - 1 };
		while { 
			str[end] !== Char.nl and: { end < max }
		} { end = end + 1 };
		^str.copyRange(start + 1, end);
	}
	
//actions:	
	didBecomeKey {
		this.class.current = this;
		toFrontAction.value(this);
	}
	
	didResignKey {
		endFrontAction.value(this);
	}
	
	makeWikiPage { arg wikiWord, extension=(".rtf"), directory;
		var filename, file, doc, string, dirName;
		directory = directory ? wikiDir;
		filename = directory ++ wikiWord ++ extension;
		file = File(filename, "w");
		if (file.isOpen) {
			string = "{\\rtf1\\mac\\ansicpg10000\\cocoartf102\\n{\\fonttbl}\n"
				"{\\colortbl;\\red255\\green255\\blue255;}\n"
				"Write about " ++ wikiWord ++ " here.\n}";
			file.write(string);
			file.close;
			
			doc = this.class.open(filename);
			doc.path = filename;
			doc.selectRange(0,0x7FFFFFFF);
			doc.onClose = {
				if(doc.string == ("Write about " ++ wikiWord ++ " here.")) {  
					unixCmd("rm" + filename)
				};
			};
		} {
			// in a second try, check if a path must be created.
			// user makes double click on string.
			dirName = wikiWord.dirname;
			if(dirName != ".") {
				dirName = directory ++ dirName;
				"created directory: % \n".postf(dirName); 
				unixCmd("mkdir -p" + dirName); 
			};
		}
	}
	openWikiPage {
		var selectedText, filename, index, directory;
		var extensions = #[".rtf", ".sc", ".scd", ".txt", "", ".rtfd", ".html"];
		selectedText = this.selectedText;
		index = this.selectionStart;
		
		this.selectRange(index, 0);
		
		// refer to local link with round parens
		if(selectedText.first == $( /*)*/ and: {�/*(*/ selectedText.last == $) }) {
				selectedText = selectedText[1 .. selectedText.size-2];
				directory = Document.current.path.dirname ++ "/";
		} {
				directory = wikiDir;
		};
		
		case { selectedText[0] == $* }
		{ 
			// execute file
			selectedText = selectedText.drop(1);
			extensions.do {|ext|
				filename = directory ++ selectedText ++ ext;
				if (File.exists(filename)) {
					// open existing wiki page
					filename.load;
					^this
				}
				{
				filename = "Help/help-scripts/" ++ selectedText ++ ext;
				if (File.exists(filename)) {
					// open help-script document
					filename.load;
					^this
				}
				}
			};
		}
		{ selectedText.first == $[ and: { selectedText.last == $] }}
		{
			// open help file
			selectedText[1 .. selectedText.size-2].openHelpFile
		}
		{ selectedText.containsStringAt(0, "http://") 
			or: { selectedText.containsStringAt(0, "file://") } }
		{
			// open URL
			("open " ++ selectedText).unixCmd;
		}
		{ selectedText.containsStringAt(selectedText.size-1, "/") }
		{
			Document(selectedText,
				pathMatch(directory ++ selectedText).collect({|it|it.basename ++ "\n"}).join
			)
		}
		
		{
			if(index + selectedText.size > this.text.size) { ^this };
			extensions.do {|ext|
				filename = directory ++ selectedText ++ ext;
				if (File.exists(filename)) {
					// open existing wiki page
					this.class.open(filename);
					^this
				}
			};
			// make a new wiki page
			this.makeWikiPage(selectedText, nil, directory);
		};
	}
		
	mouseDown { arg clickPos;	
		mouseDownAction.value(this, clickPos);
		if (wikiBrowse and: { this.linkAtClickPos(clickPos).not } 
			and: { this.selectUnderlinedText(clickPos) } ) {
			^this.openWikiPage
		};		
	}
	
	keyDown {arg character, modifiers, unicode, keycode;
		this.class.globalKeyDownAction.value(this,character, modifiers, unicode, keycode);
		keyDownAction.value(this,character, modifiers, unicode, keycode);
	}

	keyUp {arg character, modifiers, unicode, keycode;
		this.class.globalKeyUpAction.value(this,character, modifiers, unicode, keycode);
		keyUpAction.value(this,character, modifiers, unicode, keycode);
	}
		
	== { arg doc;
		^if(this.path.isNil or: { doc.path.isNil }) { doc === this } {
			this.path == doc.path
		}
	}
	
//private-----------------------------------
	prIsEditable_{arg editable=true;
		^this.subclassResponsibility
	}
	prSetTitle { arg argName;
		^this.subclassResponsibility
	}
	prGetTitle {
		^this.subclassResponsibility
	}	
	prGetFileName {
		^this.subclassResponsibility
	}
	prSetFileName {|apath|
		^this.subclassResponsibility
	}
	prGetBounds { arg argBounds;
		^this.subclassResponsibility
	}

	prSetBounds { arg argBounds;
		^this.subclassResponsibility
	}

	//if range is -1 apply to whole doc
	setFont {arg font, rangeStart= -1, rangeSize=100;
		^this.subclassResponsibility
	}
	
	setTextColor { arg color,  rangeStart = -1, rangeSize = 0;
		^this.subclassResponsibility
	}
	
	text {
		^this.subclassResponsibility
	}
	selectedText {
		^this.subclassResponsibility
	}
	selectUnderlinedText { arg clickPos;
		^this.subclassResponsibility
	}
	
	linkAtClickPos { arg clickPos;
		^this.subclassResponsibility
	}
	
	rangeText { arg rangestart=0, rangesize=1; 
		^this.subclassResponsibility
	}
	
	prclose {
		^this.subclassResponsibility
	}

	closed {
		onClose.value(this); // call user function
		allDocuments.remove(this);
		dataptr = nil;
	}
	
	prinsertText { arg dataPtr, txt;
		^this.subclassResponsibility
	}
	insertTextRange { arg string, rangestart, rangesize;
		^this.subclassResponsibility
	}

	prAdd {
		allDocuments = allDocuments.add(this);
		this.editable = true;
		if (autoRun) {
			if (this.rangeText(0,7) == "/*RUN*/")
			{
				this.text.interpret;
			}
		};
		current = this;
		initAction.value(this);
	
	}
	
	//this is called after recompiling the lib
	*prnumberOfOpen {
		^this.subclassResponsibility
	}
	*numberOfOpen {
		thisProcess.platform.when(\_NumberOfOpenTextWindows) {
			^this.prnumberOfOpen
		};
		^0
	}
	
	*newFromIndex { arg idx;
		^super.new.initByIndex(idx)
	}
	initByIndex { arg idx;
		//allDocuments = allDocuments.add(this);
		var doc;
		doc = this.prinitByIndex(idx);
		if(doc.isNil,{^nil});
		this.prAdd;
	}
	prinitByIndex { arg idx;
		^this.subclassResponsibility
	}
	
	//this is called from the menu: open, new
	*prGetLast {
		^Document.implementationClass.prBasicNew.initLast
	}
	
	initLast {
		this.prGetLastIndex;
		if(dataptr.isNil,{^nil});
		this.background_(Color.white);
		this.prAdd;
	}
	
	prGetLastIndex {
		^this.subclassResponsibility
	}
	//private open
	initFromPath { arg path, selectionStart, selectionLength;
		var stpath;
//		path = apath;
		stpath = this.class.standardizePath(path);
		this.propen(stpath, selectionStart, selectionLength);
		if(dataptr.isNil,{ 
			this.class.allDocuments.do{|d| 
					if(d.path == stpath.absolutePath){
						^d
					}
				};
			^nil
		});
		this.background_(Color.white);
		^this.prAdd;
	}
	propen { arg path, selectionStart=0, selectionLength=0;
		^this.subclassResponsibility
	}
	//private newTextWindow
	initByString{arg argTitle, str, makeListener;
	
		this.prinitByString(argTitle, str, makeListener);
		this.background_(Color.white);		
		if(dataptr.isNil,{^nil});
		this.prAdd;
		this.title = argTitle;
	
	}
	prinitByString { arg title, str, makeListener;
		^this.subclassResponsibility
	}

	//other private
	//if -1 whole doc
	prSetBackgroundColor { arg color;
		^this.subclassResponsibility
	}
	prGetBackgroundColor { arg color;
		^this.subclassResponsibility
	}	
	selectedRangeLocation {
		^this.subclassResponsibility
	}
	selectedRangeSize {
		^this.subclassResponsibility
	}
	
	prSelectLine { arg line;
		^this.subclassResponsibility
	}
	
	*prGetIndexOfListener {
		^this.subclassResponsibility
	}
	//---not yet implemented
	// ~/Documents
	// /Volumes
	// Music/Patches
	
	//*reviewUnsavedDocumentsWithAlertTitle
	//*saveAllDocuments
	//*recentDocumentPaths
	//save
	//saveAs
	//print
	//
	//hasPath  was loaded
	
}


EnvirDocument : Document {
	var <>envir;
	
	*new { arg envir, title="- Untitled Environment", string="";
		^super.new(title, string).envir_(envir)
	}
	
	*open { arg envir, path, selectionStart=0, selectionLength=0;
		var doc = super.open(path, selectionStart, selectionLength);
		^doc !? { doc.envir_(envir) }	}
	
	didBecomeKey {
		envir !? { envir.push };
		this.class.current = this;
		toFrontAction.value(this);
	}
	
	didResignKey {
		envir !? { envir.pop };
		endFrontAction.value(this);
	}

}

