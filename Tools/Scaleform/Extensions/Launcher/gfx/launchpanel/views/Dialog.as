﻿import gfx.launchpanel.views.BaseDialog;import mx.utils.Delegate;class gfx.launchpanel.views.Dialog extends MovieClip {		// Constants:		public static var CLASS_REF = gfx.launchpanel.views.Dialog;	public static var LINKAGE_ID:String = "Dialog";	private static var instance:Dialog;	public static function getInstance():Dialog {		return instance;	}	public static function setSize(p_width:Number, p_height:Number):Void {		getInstance().iSetSize(p_width, p_height);	}	public static function show(p_linkage:String, p_props:Object):BaseDialog {		return getInstance().iShow(p_linkage, p_props);	}	public static function hide():Void {		getInstance().iHide();	}				// Public Properties:		// Private Properties:	private var content:BaseDialog;	private var closeDelegate:Function;		// UI Elements:	private var bg:MovieClip;	// Initialization:	private function Dialog() {		if (instance != null) { return; }		instance = this;	}		private function onLoad():Void { configUI(); }	// Public Methods:		// Private Methods:	private function configUI():Void {		_visible = false;		closeDelegate = Delegate.create(this, close);		bg.onRelease = function() {}		bg.useHandCursor = false;	}	private function iSetSize(p_width:Number, p_height:Number):Void {		bg._width = p_width;		bg._height = p_height;		content._x = bg._width - content._width >> 1;		content._y = bg._height - content._height >> 1;	}	private function iShow(p_linkage:String, p_props:Object):BaseDialog {		iHide();		content = BaseDialog(attachMovie(p_linkage, "content", 1, p_props));		content.addEventListener("close", closeDelegate);		content.addEventListener("submit", closeDelegate);		content.addEventListener("save", closeDelegate);		content.addEventListener("edit", closeDelegate);				content._x = bg._width - content._width >> 1;		content._y = bg._height - content._height >> 1;		_visible = true;		return content;	}	private function iHide():Void {		if (content != null) {			content.removeEventListener("close", closeDelegate);			content.removeEventListener("submit", closeDelegate);			content.removeEventListener("save", closeDelegate);			content.removeEventListener("edit", closeDelegate);			content.removeMovieClip();			content = null;		}		_visible = false;	}	private function close(p_evtObj:Object):Void {		//dispatchEvent(p_evtObj);		iHide();	}}