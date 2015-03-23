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

bool vlc_player::is_playing()
{
    return _ml_p && _ml_p.isPlaying() != 0;
}

libvlc_state_t vlc_player::get_state()
{
    return _ml_p.state();
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
        play(0);
    }
    else
        _ml_p.play();
}

bool vlc_player::play(unsigned int idx)
{
    return _ml_p.playItemAtIndex( idx );
}

void vlc_player::pause()
{
    _mp.setPause( true );
}

void vlc_player::togglePause()
{
    _ml_p.pause();
}

void vlc_player::stop()
{
    libvlc_media_list_player_stop(_ml_p);
}

bool vlc_player::next()
{
    if( _ml_p.next() ) {
        return true;
    }
    return false;
}

bool vlc_player::prev()
{
    return _ml_p.previous();
}

float vlc_player::get_rate()
{
    return _mp.rate();
}

void vlc_player::set_rate(float rate)
{
    _mp.setRate( rate );
}

float vlc_player::get_fps()
{
    return _mp.fps();
}

bool vlc_player::has_vout()
{
    return _mp.hasVout() > 0;
}

float vlc_player::get_position()
{
    float p = _mp.position();
    return p < 0 ? 0.f : p;
}

void vlc_player::set_position(float p)
{
    _mp.setPosition( p );
}

libvlc_time_t vlc_player::get_time()
{
    auto t = _mp.time();

    return t < 0 ? 0 : t;
}

void vlc_player::set_time(libvlc_time_t t)
{
    _mp.setTime( t );
}

libvlc_time_t vlc_player::get_length()
{
    auto t = _mp.length();
    return t < 0 ? 0 : t;
}

void vlc_player::set_mode(libvlc_playback_mode_t mode)
{
    _ml_p.setPlaybackMode( mode );
}

bool vlc_player::is_muted()
{
    return _mp.mute();
}

void vlc_player::toggle_mute()
{
    _mp.toggleMute();
}

void vlc_player::set_mute(bool mute)
{
    _mp.setMute( mute );
}

unsigned int vlc_player::get_volume()
{
    int v = _mp.volume();
    return v < 0 ? 0 : v;
}

void vlc_player::set_volume(unsigned int volume)
{
    _mp.setVolume( volume );
}

unsigned int vlc_player::track_count()
{
    int tc = _mp.audioTrackCount();

    return tc<0 ? 0 : tc ;
}

unsigned int vlc_player::get_track()
{
    int t = libvlc_audio_get_track(_mp);

    return t < 0 ? 0 : t;
}

void vlc_player::set_track(unsigned int track)
{
    _mp.setAudioTrack( track );
}

unsigned int vlc_player::get_channel()
{
    return _mp.channel();
}

void vlc_player::set_channel(unsigned int channel)
{
    _mp.setChannel( channel );
}
