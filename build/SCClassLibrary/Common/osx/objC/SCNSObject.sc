//SCNSClass {
//	var <name;
//	
//	*new{|name|
//		^super.newCopyArgs
//	}
//	
//	asString{
//		^name
//	}
//	
//	//todo implement objc functionality
//	
//	
//}

SCNSObjectAbstract {
	var <dataptr=nil, <> className, < nsAction=nil, < nsDelegate=nil;
	
	*dumpPool {
		_ObjC_DumpPool
		^this.primitiveFailed;
	}
	
	*freePool {
		_ObjC_FreePool
		^this.primitiveFailed;
	}
	
	*new{|classname, initname, args, defer=false|
		^super.new.init(classname.asString, initname, args, defer)
	}
	
	*getClass{|classname| //return only the class id, the object is not allocated yet ...
		^super.new.initClass(classname);
	}
	
	invoke{|method, args, defer=false|
		var result;
		if(dataptr.isNil, {^nil;});
		args = args ? []; //todo: fix that in the primitive ...
		result = this.prInvoke(method, args, defer);
		^result.asNSReturn
	}
	
	initClass{|name|
		this.prGetClass(name)
	}
	
	isSubclassOf {
		|nsclassname|
		_ObjC_IsSubclassOfNSClass
		^this.primitiveFailed;
	}
	
	release {
		if(nsAction.notNil, {nsAction.release; nsAction=nil;});
		if(nsDelegate.notNil, {nsDelegate.release; nsDelegate=nil;});
		this.prDealloc;
		dataptr = nil;
	}
	
	isAllocated {
		^dataptr.notNil
	}
	
	isReleased {
		^dataptr.isNil	
	}
	
	initAction{|actionName = "doFloatAction:"|
		var out;
		out = CocoaAction.newClear;
		this.prSetActionForControl(out, actionName);
		out.prSetClassName;
		nsAction = out;
		^out
	}
	setDelegate{
		var out;
		out = CocoaAction.newClear;
		this.prSetDelegate(out);
		out.prSetClassName;
		nsDelegate = out;
		^out	
	}

	sendMessage{|msgname, args|
		this.prSendMsg(msgname, args)
	}
	
	prSendMsg{|msgname, args|
			_ObjC_SendMessage
	}
//private	
	*newFromRawPointer{|ptr|
		^super.new.initFromRawPointer(ptr)
	}
	
	*newClear{
		^super.new
	}
	
	*newWith{| classname, initname,args|
		^super.new.initWith( classname, initname,args)
	}
		
	initWith{| cn, initname,args|
		className = cn;
		this.prAllocWith( cn, initname,args);
	}
		
	initFromRawPointer{|ptr|
		dataptr = ptr;
		className = this.prGetClassName;
	}
	
	prSetClassName{
		className = this.prGetClassName;
	}
	
	init{|cn, in, args, defer|
		var result;
		className = cn;
		if(cn.isKindOf(String) and:{in.isKindOf(String)}, {
			result = this.prAllocInit(cn, in, args, defer);
			^result;
		});
	}
	
	prAllocInit { arg classname, initname,args;
		_ObjC_AllocInit;
		^this.primitiveFailed;
	}
	
	prDealloc {
		_ObjC_Dealloc;
		^this.primitiveFailed;		
	}
	
	prInvoke { arg initname,args, defer=true;
		_ObjC_Invoke
		^this.primitiveFailed;		
	}
	
	prGetClassName{|it|
		_ObjC_GetClassName
//		^this.primitiveFailed;
	}

	prAllocWith { arg classname, initname,args;
		_ObjC_AllocSend;
		^this.primitiveFailed;
	}
	prGetClass{arg classname;
		_ObjC_GetClass
		^this.primitiveFailed;				
	}
	
	/*
	asPyrString {
		^this.prAsPyrString;
	}
	*/
		
	//for NSControl:
	prSetActionForControl{|control|
		_ObjC_SetActionForControl
	}			
	prSetDelegate{|control|
		_ObjC_SetDelegate
	}	
	
//	sendMsg{|initname,args|
//		^this.prSendMsg(className, initname, args, 0)
//	}

// experimental
	*panel{|path|
		_LoadUserPanel
	}
	
	asArray {arg arrayType, requestedLength=nil;
		if(this.isSubclassOf("NSData"), {
			requestedLength = requestedLength ? this.invoke("length");
			if(arrayType.isKindOf(String), {arrayType = arrayType.asSymbol});
			if(arrayType.isKindOf(Symbol), {
				arrayType = case 
					{ arrayType == \string } { String.new(requestedLength) }
					{ arrayType == \int8   } { Int8Array.new(requestedLength) }
					{ arrayType == \int16  } { Int16Array.new(requestedLength) }
					{ arrayType == \int32  } { Int32Array.new(requestedLength) }
					{ arrayType == \double } { DoubleArray.new(requestedLength) }
					{ arrayType == \float  } { FloatArray.new(requestedLength) };
				^this.prAsArray(arrayType, requestedLength);
			});
			^nil;
		});
	}
	
	prAsArray {|type, len|
		_ObjC_NSDataToSCArray
		^this.primitiveFailed;
	}
	
	/*
	prAsPyrString {
		_ObjC_NSStringToPyrString
		^this.primitiveFailed;
	}
	*/
	
	registerNotification {
		|aNotificationName, aFunc|
		if(nsDelegate.isNil, {
			this.setDelegate;
		});
		nsDelegate.prRegisterNotification(aNotificationName, aFunc);
		this.prRegisterNotification(aNotificationName);
	}
	
	prRegisterNotification {|aNotificationName|
		_ObjC_RegisterNotification
	}
}

//this is usually noy created directly. call SCNSObject-initAction instead.
CocoaAction : SCNSObjectAbstract{
	var <>action, notificationActions=nil, delegateActions=nil;
	
	doAction{|it|
		action.value(this, it);
	}
	
	doNotificationAction {
		|notif, nsNotification|
		var func;
		func = notificationActions.at(notif.asSymbol);
		if(func.notNil, {
			func.value(notif, nsNotification);
		});
	}
	
	doDelegateAction {
		|method, arguments|
		var result, func;
		func = delegateActions.at(method.asSymbol);
		if(func.notNil, {
			result = func.value(method, arguments);
			^result;
		});
	}
	
	addMethod {
		|selectorName, returntype, objctypes, aFunc|
		var types;
		if(selectorName.notNil and:{aFunc.isKindOf(Function)}, {
			if(delegateActions.isNil, {delegateActions = IdentityDictionary.new(16)});
			if(returntype.isNil, {returntype = "v"});
			types = returntype ++ "@:" ++ objctypes; // first and second types are always ID and _cmd
			this.praddMethod(selectorName, types);
			delegateActions.add(selectorName.asSymbol -> aFunc);
		});
	}
	
	prRegisterNotification {
		|aNotName, aFunc|
		if(aNotName.notNil, {
			if(notificationActions.isNil, {notificationActions = IdentityDictionary.new(16);});
			notificationActions.add(aNotName.asSymbol -> aFunc);
		});
	}
	
	praddMethod {
		|selectorName, objctypesAsString|
		_ObjC_DelegateAddSelector
		^this.primitiveFailed;
	}
	
	removeMethod {
		_ObjC_DelegateRemoveSelector
		^this.primitiveFailed;
	}
}

SCNSObject : SCNSObjectAbstract{

}

NSBundle : SCNSObject {
	classvar <> all;
	
	*new{|path|
		^super.newClear.loadBundle(path);
	}
	
	allocPrincipalClass{
		^SCNSObject.newFromRawPointer(this.prAllocPrincipalClass);
	}
	
	allocClassNamed{|name, initname, args, defer=false|
		var ptr;
		ptr = this.prAllocClassNamed(name, initname, args, defer);
		if(ptr.isNil){"could not alloc class: %".format(name).warn; ^nil};
		^SCNSObject.newFromRawPointer(ptr);
	}
	
	
	//private
	
	loadBundle{|path|
		this.prLoadBundle(path);
		if(this.isAllocated){
			all = all.add(this);
		}
	}
	
	prLoadBundle{|path|
		_ObjC_LoadBundle
	}
		
	prAllocPrincipalClass{
		_ObjcBundleAllocPrincipalClass
	}
	
	prAllocClassNamed{|name, initname, args, defer|
		_ObjcBundleAllocClassNamed
	}
	
}

/* cocoa-bridge by Jan Trutzschler 2005 */

NSTypeEncoding {
	*object {^"@"}
	*integer {^"i"}
	*float {^"f"}
	*double {^"d"}
	*boolean {^"i"}
}

+ SCView {
	primitive {^dataptr;}
	*newFromRawPointer {|nsptr|
	}
}
