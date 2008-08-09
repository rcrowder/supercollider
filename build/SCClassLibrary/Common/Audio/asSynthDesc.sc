
// methods for in-memory SynthDesc building from a SynthDef
// implemented as a method of SynthDef
// because someday there might be a subclass of SynthDef that would require different code

+ SynthDesc {
	*newFrom { |synthdef|
		^synthdef.asSynthDesc
	}
}

+ SynthDescLib {
	add { |synthdesc|
		synthDescs.put(synthdesc.name.asSymbol, synthdesc);
	}
	addServer { |server|
		servers = servers.add(server);
	}
	removeServer { |server|
		servers.remove(server);
	}
}

+ SynthDef {
		// dir argument is not needed because no file will be written
	memStore { arg libname=\global, completionMsg;
		var	lib = SynthDescLib.all[libname] ?? {
				Error("library" + libname  + "not found").throw
			},
			desc = this.asSynthDesc;
		lib.add(desc);
		lib.servers.do({ |server|
			server.value.sendBundle(nil, ["/d_recv", this.asBytes] ++ completionMsg)
		});
	}

	asSynthDesc {
		var	bus, control, desc;
		desc = SynthDesc.new;
		desc.def = this;
		desc.name = name;
		desc.metadata = metadata;
			// .select is for InstrSynthDef, which may have Patch controls not in the Synth itself
		desc.controls = Array.new;
		allControlNames.select({ |cn| cn.rate != \noncontrol })
			.do({ |cn|
					// InstrSynthDefs may have non-numeric values as control defaults
					// replace with 0.0 (must be a float or makeMsgFunc breaks)
				control = cn.copy.name_(cn.name.asString)
					.defaultValue_(cn.defaultValue.tryPerform(\asFloat) ? 0.0);
				// if the default value is a single value (not array)
				(cn.defaultValue.size == 0).if({
					desc.controls = desc.controls.add(control);
				}, {
					cn.defaultValue.do({ |value, i|
						control = control.copy.index_(cn.index + i)
							.defaultValue_(value.tryPerform(\asFloat) ? 0.0);
						(i > 0).if({ control.name_("?") });
						desc.controls = desc.controls.add(control);
					});
				});
			});
		desc.controlNames = desc.controls.collect(_.name).reject(_ == "?");
		desc.inputs = [];
		desc.outputs = [];
		children.do({ |ugen|
			ugen.class.isInputUGen.if({
				bus = ugen.inputs !? { ugen.inputs[0].source };
				if (bus.class.isControlUGen) {
					control = desc.controls.detect {|item| item.index == bus.specialIndex };
					if (control.notNil) { bus = control.name };
				};
				desc.inputs = desc.inputs.add( IODesc(ugen.rate, ugen.channels.size, bus))
			}, {
			ugen.class.isOutputUGen.if({
				bus = ugen.inputs !? { ugen.inputs[0].source };
				if (bus.class.isControlUGen) {
					control = desc.controls.detect {|item| item.index == bus.specialIndex };
					if (control.notNil) { bus = control.name };
				};
				desc.outputs = desc.outputs.add( IODesc(ugen.rate,
					ugen.inputs.size - ugen.class.numFixedArgs, bus))
			}) });
		});
		desc.constants = FloatArray.newClear(constants.size);
		constants.keysValuesDo { arg value, index;
			desc.constants[index] = value;
		};
		desc.makeMsgFunc;
		^desc
	}
}
