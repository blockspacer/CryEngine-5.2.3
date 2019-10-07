﻿/**
 * The public interface that all item renderers must expose to be used in a list-type component, such as ScrollingList, TileList, etc. Note that this interface is not implemented in the existing components, and does not need to be implemented, it is just a reference.
 */

/**********************************************************************
 Copyright (c) 2009 Scaleform Corporation. All Rights Reserved.
 Licensees may use this file in accordance with the valid Scaleform
 License Agreement provided with the software. This file is provided 
 AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE WARRANTY OF DESIGN, 
 MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.
**********************************************************************/

interface gfx.interfaces.IListItemRenderer {

// Public Methods:
	/**
	 * Set the list data to an itemRenderer when the list changes, either by scrolling, or refreshing.
	 * @param index The index of the data in the DataProvider that the itemRenderer represents
	 * @param label The string label for the itemRenderer, determined by the labelField or labelFunction of the list component.
	 * @param selected The selected state of the itemRenderer
	 */
	public function setListData(index:Number, label:String, selected:Boolean):Void;
	
	/**
	 * The data in the DataProvider of the list component that the itemRenderer represents.
	 * @param data A generic data object.  This method is primarily just to store the value, but it can also customize the renderer based on its contents.
	 */
	public function setData(data:Object):Void;
	

}