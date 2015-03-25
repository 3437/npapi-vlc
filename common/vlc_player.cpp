/*****************************************************************************
 * Copyright � 2002-2011 VideoLAN and VLC authors
 * $Id$
 *
 * Authors: Sergey Radionov <rsatom_gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "vlc_player.h"

bool vlc_player::open(VLC::Instance& inst)
{
    if( !inst )
        return false;

    _libvlc_instance = inst;

    try {
        _mp   = VLC::MediaPlayer(inst);
        _ml   = VLC::MediaList(inst);
        _ml_p = VLC::MediaListPlayer(inst);

        _ml_p.setMediaList( _ml );
        _ml_p.setMediaPlayer( _mp );
    }
    catch (std::runtime_error&) {
        return false;
    }

    return true;
}

int vlc_player::add_item(const char * mrl, unsigned int optc, const char **optv)
{
    VLC::Media media;
    try {
        media = VLC::Media( _libvlc_instance, mrl, VLC::Media::FromLocation );
    }
    catch ( std::runtime_error& ) {
        return -1;
    }

    for( unsigned int i = 0; i < optc; ++i )
        media.addOptionFlag( optv[i], libvlc_media_option_unique );

    VLC::MediaList::Lock lock( _ml );
    if( _ml.addMedia( media ) )
         return _ml.count() - 1;
    return -1;
}

int vlc_player::current_item()
{
    auto media = _mp.media();

    if( !media )
        return -1;
    return _ml.indexOfItem( *media );
}

int vlc_player::items_count()
{
    VLC::MediaList::Lock lock( _ml );
    return _ml.count();
}

bool vlc_player::delete_item(unsigned int idx)
{
    VLC::MediaList::Lock lock( _ml );
    return _ml.removeIndex( idx );
}

void vlc_player::clear_items()
{
    VLC::MediaList::Lock lock( _ml );
    for( int i = _ml.count(); i > 0; --i) {
        _ml.removeIndex( i - 1 );
    }
}

void vlc_player::play()
{
    if( 0 == items_count() )
        return;
    else if( -1 == current_item() ) {
        _ml_p.playItemAtIndex( 0 );
    }
    else
        _ml_p.play();
}

int vlc_player::currentAudioTrack()
{
    auto current = _mp.audioTrack();
    auto tracks = _mp.audioTrackDescription();
    return getTrack( current, tracks );
}

int vlc_player::currentSubtitleTrack()
{
    auto current = _mp.spu();
    auto tracks = _mp.spuDescription();
    return getTrack( current, tracks );
}

int vlc_player::getTrack( int currentId, const std::vector<VLC::TrackDescription>& tracks )
{
    if ( tracks.empty() )
        return -1;

    int trackId = 0;
    for ( const auto& t : tracks )
    {
        if ( t.id() == currentId )
        {
            return trackId;
        }
        ++trackId;
    }
    return -1;
}
