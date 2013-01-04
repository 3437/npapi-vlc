/*****************************************************************************
 * vlcwindowless_win.h: a VLC plugin for Mozilla (Windows windowless)
 *****************************************************************************
 * Copyright (C) 2011 the VideoLAN team
 * $Id$
 *
 * Authors: Cheng Sun <chengsun9@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef __VLCWINDOWS_LINUX_H__
#define __VLCWINDOWS_LINUX_H__

#include "vlcwindowless_base.h"

class VlcWindowlessWin : public VlcWindowlessBase
{
public:
    VlcWindowlessWin(NPP instance, NPuint16_t mode);
    virtual ~VlcWindowlessWin();

    bool handle_event(void *event);

private:
    HBRUSH m_hBgBrush;
};


#endif /* __VLCWINDOWLESS_LINUX_H__ */