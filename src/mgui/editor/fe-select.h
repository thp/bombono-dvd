//
// mgui/editor/fe-select.h
// This file is part of Bombono DVD project.
//
// Copyright (c) 2010 Ilya Murav'jov
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
// 

#ifndef __MGUI_EDITOR_FE_SELECT_H__
#define __MGUI_EDITOR_FE_SELECT_H__

#include <mgui/menu-rgn.h>

#include <mlib/range/any_range.h>
#include <mlib/foreach.h>

//
// Iterating over selected objects; example:
//  boost_foreach( Comp::MediaObj* obj, SelectedMediaObjs(edt_area) )
//  {
//      res_mi = obj->MediaItem();
//      break;
//  }
//
fe::range<Comp::MediaObj*> SelectedMediaObjs();


#endif // __MGUI_EDITOR_FE_SELECT_H__

