/*****************************************************************************
 * npolibvlc.cpp: official Javascript APIs
 *****************************************************************************
 * Copyright (C) 2002-2014 VLC authors and VideoLAN
 * Copyright (C) 2010 M2X BV
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
 *          JP Dinger <jpd@videolan.org>
 *          Felix Paul Kühne <fkuehne # videolan.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "vlcplugin.h"
#include "npolibvlc.h"

#include "../../common/position.h"

/*
** Local helper macros and function
*/
#define COUNTNAMES(a,b,c) const int a::b = sizeof(a::c)/sizeof(NPUTF8 *)
#define RETURN_ON_ERROR                             \
    do {                                            \
        NPN_SetException(this, libvlc_errmsg());    \
        return INVOKERESULT_GENERIC_ERROR;          \
    }while(0)

#define ERROR_EVENT_NOT_FOUND "ERROR: One or more events could not be found."
#define ERROR_API_VERSION "ERROR: NPAPI version not high enough. (Gecko >= 1.9 needed)"


/*
** implementation of libvlc root object
*/

LibvlcRootNPObject::~LibvlcRootNPObject()
{
    /*
    ** When the plugin is destroyed, firefox takes it upon itself to
    ** destroy all 'live' script objects and ignores refcounting.
    ** Therefore we cannot safely assume that refcounting will control
    ** lifespan of objects. Hence they are only lazily created on
    ** request, so that firefox can take ownership, and are not released
    ** when the plugin is destroyed.
    */
    if( isValid() )
    {
        if( audioObj    ) NPN_ReleaseObject(audioObj);
        if( inputObj    ) NPN_ReleaseObject(inputObj);
        if( playlistObj ) NPN_ReleaseObject(playlistObj);
        if( subtitleObj ) NPN_ReleaseObject(subtitleObj);
        if( videoObj    ) NPN_ReleaseObject(videoObj);
        if( mediaDescriptionObj ) NPN_ReleaseObject(mediaDescriptionObj);
    }
}

const NPUTF8 * const LibvlcRootNPObject::propertyNames[] =
{
    "audio",
    "input",
    "playlist",
    "subtitle",
    "video",
    "VersionInfo",
    "mediaDescription"
};
COUNTNAMES(LibvlcRootNPObject,propertyCount,propertyNames);

enum LibvlcRootNPObjectPropertyIds
{
    ID_root_audio = 0,
    ID_root_input,
    ID_root_playlist,
    ID_root_subtitle,
    ID_root_video,
    ID_root_VersionInfo,
    ID_root_MediaDescription,
};

RuntimeNPObject::InvokeResult
LibvlcRootNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        switch( index )
        {
            case ID_root_audio:
                InstantObj<LibvlcAudioNPObject>( audioObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(audioObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_input:
                InstantObj<LibvlcInputNPObject>( inputObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(inputObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_playlist:
                InstantObj<LibvlcPlaylistNPObject>( playlistObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(playlistObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_subtitle:
                InstantObj<LibvlcSubtitleNPObject>( subtitleObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(subtitleObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_video:
                InstantObj<LibvlcVideoNPObject>( videoObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(videoObj), result);
                return INVOKERESULT_NO_ERROR;
            case ID_root_VersionInfo:
                return invokeResultString(libvlc_get_version(),result);
            case ID_root_MediaDescription:
            {
                InstantObj<LibvlcMediaDescriptionNPObject>( mediaDescriptionObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(mediaDescriptionObj), result);
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcRootNPObject::methodNames[] =
{
    "versionInfo",
    "addEventListener",
    "removeEventListener",
};
COUNTNAMES(LibvlcRootNPObject,methodCount,methodNames);

enum LibvlcRootNPObjectMethodIds
{
    ID_root_versionInfo,
    ID_root_addeventlistener,
    ID_root_removeeventlistener,
};

RuntimeNPObject::InvokeResult LibvlcRootNPObject::invoke(int index,
                  const NPVariant *args, uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    switch( index )
    {
    case ID_root_versionInfo:
        if( 0 != argCount )
            return INVOKERESULT_NO_SUCH_METHOD;
        return invokeResultString(libvlc_get_version(),result);

    case ID_root_addeventlistener:
    case ID_root_removeeventlistener:
        if ( argCount < 2 )
            return INVOKERESULT_INVALID_ARGS;

        // Don't wrap eventName as it would copy the string even though it's not required.
        auto listener = npapi::Variant( args[1] );

        if ( !npapi::is_string( args[0] ) ||
            !listener.is<NPObject>() )
            break;

        if( !VlcPluginBase::canUseEventListener() )
        {
            NPN_SetException(this, ERROR_API_VERSION);
            return INVOKERESULT_GENERIC_ERROR;
        }

        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();

        if( ID_root_addeventlistener == index )
        {
            p_plugin->subscribe( npapi::to_tmp_string( args[0] ), listener );
        }
        else
        {
            p_plugin->unsubscribe( npapi::to_tmp_string( args[0] ), listener );
        }
        VOID_TO_NPVARIANT(result);

        return INVOKERESULT_NO_ERROR;
    }
    return INVOKERESULT_NO_SUCH_METHOD;
}

/*
** implementation of libvlc audio object
*/

const NPUTF8 * const LibvlcAudioNPObject::propertyNames[] =
{
    "mute",
    "volume",
    "track",
    "count",
    "channel",
};
COUNTNAMES(LibvlcAudioNPObject,propertyCount,propertyNames);

enum LibvlcAudioNPObjectPropertyIds
{
    ID_audio_mute,
    ID_audio_volume,
    ID_audio_track,
    ID_audio_count,
    ID_audio_channel,
};

RuntimeNPObject::InvokeResult
LibvlcAudioNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();

        auto& mp = p_plugin->getMD();
        if( !mp )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_audio_mute:
            {
                bool muted = mp.mute();
                BOOLEAN_TO_NPVARIANT(muted, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_volume:
            {
                int volume = mp.volume();
                INT32_TO_NPVARIANT(volume, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_track:
            {
                int track = p_plugin->player().currentAudioTrack();
                INT32_TO_NPVARIANT(track, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_count:
            {
                // get the number of audio track available
                int i_track = mp.audioTrackCount();
                // return it
                INT32_TO_NPVARIANT(i_track, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_audio_channel:
            {
                int channel = mp.channel();
                INT32_TO_NPVARIANT(channel, result);
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcAudioNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();

        auto& mp = p_plugin->getMD();
        if( !mp )
            RETURN_ON_ERROR;

        auto v = npapi::Variant( value );
        switch( index )
        {
            case ID_audio_mute:
                if( v.is<bool>() )
                {
                    mp.setMute( v );
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            case ID_audio_volume:
                if( v.is<int>() )
                {
                    mp.setVolume( v );
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            case ID_audio_track:
                if( v.is<int>() )
                {
                    auto tracks = mp.audioTrackDescription();
                    if ( v >= tracks.size() )
                        return INVOKERESULT_INVALID_VALUE;
                    if ( mp.setAudioTrack( tracks[v].id() ) )
                        return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            case ID_audio_channel:
                if( v.is<int>() )
                {
                    if ( mp.setChannel( v ) )
                        return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcAudioNPObject::methodNames[] =
{
    "toggleMute",
    "description",
};
COUNTNAMES(LibvlcAudioNPObject,methodCount,methodNames);

enum LibvlcAudioNPObjectMethodIds
{
    ID_audio_togglemute,
    ID_audio_description,
};

RuntimeNPObject::InvokeResult
LibvlcAudioNPObject::invoke(int index, const NPVariant *args,
                            uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
        auto& mp = p_plugin->getMD();
        if( !mp )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_audio_togglemute:
                if( argCount == 0 )
                {
                    mp.toggleMute();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_audio_description:
            {
                if ( argCount < 1 )
                    return INVOKERESULT_INVALID_ARGS;
                auto v = npapi::Variant( args[0] );
                if( v.is<int>() )
                {
                    auto tracks = mp.audioTrackDescription();
                    if ( v >= tracks.size() )
                        return INVOKERESULT_INVALID_VALUE;
                    /* display the name of the track chosen */
                    return invokeResultString( tracks[v].name().c_str(), result );
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

/*
** implementation of libvlc input object
*/

const NPUTF8 * const LibvlcInputNPObject::propertyNames[] =
{
    "length",
    "position",
    "time",
    "state",
    "rate",
    "fps",
    "hasVout",
};
COUNTNAMES(LibvlcInputNPObject,propertyCount,propertyNames);

enum LibvlcInputNPObjectPropertyIds
{
    ID_input_length,
    ID_input_position,
    ID_input_time,
    ID_input_state,
    ID_input_rate,
    ID_input_fps,
    ID_input_hasvout,
};

RuntimeNPObject::InvokeResult
LibvlcInputNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
        auto& mp = p_plugin->getMD();
        if( !mp )
        {
            if( index != ID_input_state )
                RETURN_ON_ERROR;
            else
            {
                /* for input state, return CLOSED rather than an exception */
                INT32_TO_NPVARIANT(0, result);
                return INVOKERESULT_NO_ERROR;
            }
        }

        switch( index )
        {
            case ID_input_length:
            {
                double val = mp.length();
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_position:
            {
                double val = mp.position();
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_time:
            {
                double val = (double)mp.time();
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_state:
            {
                int val = mp.state();
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_rate:
            {
                float val = mp.rate();
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_fps:
            {
                double val = mp.fps();
                DOUBLE_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_hasvout:
            {
                bool val = p_plugin->player().get_mp().hasVout() != 0;
                BOOLEAN_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcInputNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
        auto& mp = p_plugin->getMD();
        if( !mp )
            RETURN_ON_ERROR;

        auto v = npapi::Variant( value );
        switch( index )
        {
            case ID_input_position:
            {
                if( !v.is<double>() )
                {
                    return INVOKERESULT_INVALID_VALUE;
                }

                mp.setPosition( v );
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_time:
            {
                if( !v.is<int>() )
                {
                    return INVOKERESULT_INVALID_VALUE;
                }

                mp.setTime( v );
                return INVOKERESULT_NO_ERROR;
            }
            case ID_input_rate:
            {
                if( !v.is<double>() )
                {
                    return INVOKERESULT_INVALID_VALUE;
                }

                mp.setRate( v );
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcInputNPObject::methodNames[] =
{
    /* no methods */
    "none",
};
COUNTNAMES(LibvlcInputNPObject,methodCount,methodNames);

enum LibvlcInputNPObjectMethodIds
{
    ID_none,
};

RuntimeNPObject::InvokeResult
LibvlcInputNPObject::invoke(int index, const NPVariant *,
                            uint32_t, NPVariant &)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        switch( index )
        {
            case ID_none:
                return INVOKERESULT_NO_SUCH_METHOD;
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

/*
** implementation of libvlc MediaDescription object
*/

const NPUTF8 * const LibvlcMediaDescriptionNPObject::propertyNames[] =
{
    "title",
    "artist",
    "genre",
    "copyright",
    "album",
    "trackNumber",
    "description",
    "rating",
    "date",
    "setting",
    "URL",
    "language",
    "nowPlaying",
    "publisher",
    "encodedBy",
    "artworkURL",
    "trackID",
};
COUNTNAMES(LibvlcMediaDescriptionNPObject,propertyCount,propertyNames);

enum LibvlcMediaDescriptionNPObjectPropertyIds
{
    ID_meta_title,
    ID_meta_artist,
    ID_meta_genre,
    ID_meta_copyright,
    ID_meta_album,
    ID_meta_trackNumber,
    ID_meta_description,
    ID_meta_rating,
    ID_meta_date,
    ID_meta_setting,
    ID_meta_URL,
    ID_meta_language,
    ID_meta_nowPlaying,
    ID_meta_publisher,
    ID_meta_encodedBy,
    ID_meta_artworkURL,
    ID_meta_trackID,
};
RuntimeNPObject::InvokeResult
LibvlcMediaDescriptionNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPlugin* p_plugin = getPrivate<VlcPlugin>();
        auto& mp = p_plugin->getMD();
        if( !mp )
            RETURN_ON_ERROR;
        auto media = mp.media();
        if( !media )
            RETURN_ON_ERROR;
        const char *info;
        switch( index )
        {
            case ID_meta_title:
            case ID_meta_artist:
            case ID_meta_genre:
            case ID_meta_copyright:
            case ID_meta_album:
            case ID_meta_trackNumber:
            case ID_meta_description:
            case ID_meta_rating:
            case ID_meta_date:
            case ID_meta_setting:
            case ID_meta_URL:
            case ID_meta_language:
            case ID_meta_nowPlaying:
            case ID_meta_publisher:
            case ID_meta_encodedBy:
            case ID_meta_artworkURL:
            case ID_meta_trackID:
            {
                auto m = media->meta( (libvlc_meta_t)index );
                return invokeResultString(m.c_str(), result);
            }
            default:
            ;
        }
     }
     return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcMediaDescriptionNPObject::methodNames[] =
{
    "None"
};
COUNTNAMES(LibvlcMediaDescriptionNPObject,methodCount,methodNames);

enum LibvlcMediaDescriptionNPObjectMethodIds
{
    ID_mediadescription_method_none,
};


/*
** implementation of libvlc playlist items object
*/

const NPUTF8 * const LibvlcPlaylistItemsNPObject::propertyNames[] =
{
    "count",
};
COUNTNAMES(LibvlcPlaylistItemsNPObject,propertyCount,propertyNames);

enum LibvlcPlaylistItemsNPObjectPropertyIds
{
    ID_playlistitems_count,
};

RuntimeNPObject::InvokeResult
LibvlcPlaylistItemsNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();

        switch( index )
        {
            case ID_playlistitems_count:
            {
                int val = p_plugin->player().items_count();
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcPlaylistItemsNPObject::methodNames[] =
{
    "clear",
    "remove",
};
COUNTNAMES(LibvlcPlaylistItemsNPObject,methodCount,methodNames);

enum LibvlcPlaylistItemsNPObjectMethodIds
{
    ID_playlistitems_clear,
    ID_playlistitems_remove,
};

RuntimeNPObject::InvokeResult
LibvlcPlaylistItemsNPObject::invoke(int index, const NPVariant *args,
                                    uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();

        switch( index )
        {
            case ID_playlistitems_clear:
                if( argCount == 0 )
                {
                    p_plugin->player().clear_items();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlistitems_remove:
            {
                if ( argCount < 1 )
                    INVOKERESULT_INVALID_ARGS;
                auto v = npapi::Variant( args[0] );
                if( v.is<int>() )
                {
                    if( !p_plugin->player().delete_item( v ) )
                        return INVOKERESULT_GENERIC_ERROR;
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

/*
** implementation of libvlc playlist object
*/

LibvlcPlaylistNPObject::~LibvlcPlaylistNPObject()
{
    // Why the isValid()?
    if( isValid() && playlistItemsObj )
        NPN_ReleaseObject(playlistItemsObj);
};

const NPUTF8 * const LibvlcPlaylistNPObject::propertyNames[] =
{
    "itemCount", /* deprecated */
    "isPlaying",
    "currentItem",
    "items",
};
COUNTNAMES(LibvlcPlaylistNPObject,propertyCount,propertyNames);

enum LibvlcPlaylistNPObjectPropertyIds
{
    ID_playlist_itemcount,
    ID_playlist_isplaying,
    ID_playlist_currentitem,
    ID_playlist_items,
};

RuntimeNPObject::InvokeResult
LibvlcPlaylistNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();

        switch( index )
        {
            case ID_playlist_itemcount: /* deprecated */
            {
                int val = p_plugin->player().items_count();
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_playlist_isplaying:
            {
                int val = p_plugin->player().mlp().isPlaying();
                BOOLEAN_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_playlist_currentitem:
            {
                int val = p_plugin->player().current_item();
                INT32_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_playlist_items:
            {
                InstantObj<LibvlcPlaylistItemsNPObject>( playlistItemsObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(playlistItemsObj), result);
                return INVOKERESULT_NO_ERROR;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcPlaylistNPObject::methodNames[] =
{
    "add",
    "play",
    "playItem",
    "pause",
    "togglePause",
    "stop",
    "next",
    "prev",
    "clear", /* deprecated */
    "removeItem", /* deprecated */
};
COUNTNAMES(LibvlcPlaylistNPObject,methodCount,methodNames);

enum LibvlcPlaylistNPObjectMethodIds
{
    ID_playlist_add,
    ID_playlist_play,
    ID_playlist_playItem,
    ID_playlist_pause,
    ID_playlist_togglepause,
    ID_playlist_stop,
    ID_playlist_next,
    ID_playlist_prev,
    ID_playlist_clear,
    ID_playlist_removeitem
};

RuntimeNPObject::InvokeResult
LibvlcPlaylistNPObject::invoke(int index, const NPVariant *args,
                               uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();

        switch( index )
        {
            // XXX FIXME this needs squashing into something much smaller
            case ID_playlist_add:
            {
                if( (argCount < 1) || (argCount > 3) )
                    return INVOKERESULT_NO_SUCH_METHOD;
                if( !npapi::is_string( args[0] ) )
                    return INVOKERESULT_NO_SUCH_METHOD;

                // grab URL
                if( NPVARIANT_IS_NULL(args[0]) )
                    return INVOKERESULT_NO_SUCH_METHOD;

                // Don't assign url pointer to s, they don't have the same
                // deleter function.
                auto s = npapi::to_string( args[0] );
                auto url = CStr( p_plugin->getAbsoluteURL( s.get() ), free );

                //FIXME: This is never used
                auto name = npapi::NPStringPtr( nullptr, NPN_MemFree );
                // grab name if available
                if( argCount > 1 )
                {
                    if( NPVARIANT_IS_NULL(args[1]) )
                    {
                        // do nothing
                    }
                    else if( NPVARIANT_IS_STRING(args[1]) )
                    {
                        name = npapi::to_string( args[1] );
                    }
                    else
                    {
                        return INVOKERESULT_INVALID_VALUE;
                    }
                }

                int i_options = 0;
                char** ppsz_options = NULL;

                // grab options if available
                if( argCount > 2 )
                {
                    npapi::Variant v = args[2];
                    if( v.is<std::nullptr_t>() )
                    {
                        // do nothing
                    }
                    else if( v.is<NPString>() )
                    {
                        parseOptions((NPString)v, &i_options, &ppsz_options);

                    }
                    else if( v.is<NPObject>() )
                    {
                        parseOptions((NPObject*)v, &i_options, &ppsz_options);
                    }
                    else
                    {
                        return INVOKERESULT_INVALID_VALUE;
                    }
                }

                int item = p_plugin->player().add_item( url ? url.get() : s.get(), i_options, const_cast<const char **>(ppsz_options));
                if( item == -1 )
                    RETURN_ON_ERROR;

                for( int i=0; i< i_options; ++i )
                {
                    free(ppsz_options[i]);
                }
                free(ppsz_options);

                INT32_TO_NPVARIANT(item, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_playlist_play:
                if( argCount == 0 )
                {
                    p_plugin->player().play();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_playItem:
            {
                if ( argCount < 1 )
                    return INVOKERESULT_INVALID_ARGS;
                npapi::Variant v = args[0];
                if ( v.is<int>() )
                {
                    p_plugin->player().mlp().playItemAtIndex( v );
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_playlist_pause:
                if( argCount == 0 )
                {
                    p_plugin->player().get_mp().setPause( true );
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_togglepause:
                if( argCount == 0 )
                {
                    p_plugin->player().mlp().pause();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_stop:
                if( argCount == 0 )
                {
                    p_plugin->player().mlp().stop();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_next:
                if( argCount == 0 )
                {
                    p_plugin->player().mlp().next();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_prev:
                if( argCount == 0 )
                {
                    p_plugin->player().mlp().previous();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_clear: /* deprecated */
                if( argCount == 0 )
                {
                    p_plugin->player().clear_items();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            case ID_playlist_removeitem: /* deprecated */
            {
                if ( argCount < 1 )
                    return INVOKERESULT_INVALID_ARGS;
                npapi::Variant v = args[0];
                if ( v.is<int>() )
                {
                    if( !p_plugin->player().delete_item( v ) )
                        return INVOKERESULT_GENERIC_ERROR;
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            default:
                ;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

// XXX FIXME The new playlist_add creates a media instance and feeds it
// XXX FIXME these options one at a time, so this hunk of code does lots
// XXX FIXME of unnecessairy work. Break out something that can do one
// XXX FIXME option at a time and doesn't need to realloc().
// XXX FIXME Same for the other version of parseOptions.

void LibvlcPlaylistNPObject::parseOptions(const NPString& nps,
                                         int *i_options, char*** ppsz_options)
{
    if( nps.UTF8Length )
    {
        auto s = CStr( strdup( nps.UTF8Characters ), free );
        char *val = s.get();
        if( val )
        {
            long capacity = 16;
            char **options = (char **)malloc(capacity*sizeof(char *));
            if( options )
            {
                int nOptions = 0;

                char *end = val + nps.UTF8Length;
                while( val < end )
                {
                    // skip leading blanks
                    while( (val < end)
                        && ((*val == ' ' ) || (*val == '\t')) )
                        ++val;

                    char *start = val;
                    // skip till we get a blank character
                    while( (val < end)
                        && (*val != ' ' )
                        && (*val != '\t') )
                    {
                        char c = *(val++);
                        if( ('\'' == c) || ('"' == c) )
                        {
                            // skip till end of string
                            while( (val < end) && (*(val++) != c ) );
                        }
                    }

                    if( val > start )
                    {
                        if( nOptions == capacity )
                        {
                            capacity += 16;
                            char **moreOptions = (char **)realloc(options, capacity*sizeof(char*));
                            if( ! moreOptions )
                            {
                                /* failed to allocate more memory */
                                /* return what we got so far */
                                *i_options = nOptions;
                                *ppsz_options = options;
                                return;
                            }
                            options = moreOptions;
                        }
                        *(val++) = '\0';
                        options[nOptions++] = strdup(start);
                    }
                    else
                        // must be end of string
                        break;
                }
                *i_options = nOptions;
                *ppsz_options = options;
            }
        }
    }
}

// XXX FIXME See comment at the other parseOptions variant.
void LibvlcPlaylistNPObject::parseOptions(NPObject *obj, int *i_options,
                                          char*** ppsz_options)
{
    /* WARNING: Safari does not implement NPN_HasProperty/NPN_HasMethod */

    npapi::Variant value;

    /* we are expecting to have a Javascript Array object */
    NPIdentifier propId = NPN_GetStringIdentifier("length");
    if( NPN_GetProperty(_instance, obj, propId, value) )
    {
        int count = value;

        if( count )
        {
            long capacity = 16;
            char **options = (char **)malloc(capacity*sizeof(char *));
            if( options )
            {
                int nOptions = 0;

                while( nOptions < count )
                {
                    npapi::Variant value;
                    propId = NPN_GetIntIdentifier(nOptions);
                    if( ! NPN_GetProperty(_instance, obj, propId, value) )
                        /* return what we got so far */
                        break;

                    if( !value.is<NPString>() )
                    {
                        /* return what we got so far */
                        break;
                    }

                    if( nOptions == capacity )
                    {
                        capacity += 16;
                        char **moreOptions = (char **)realloc(options, capacity*sizeof(char*));
                        if( ! moreOptions )
                        {
                            /* failed to allocate more memory */
                            /* return what we got so far */
                            *i_options = nOptions;
                            *ppsz_options = options;
                            break;
                        }
                        options = moreOptions;
                    }

                    options[nOptions++] = strdup(value);
                }
                *i_options = nOptions;
                *ppsz_options = options;
            }
        }
    }
}

/*
** implementation of libvlc subtitle object
*/

const NPUTF8 * const LibvlcSubtitleNPObject::propertyNames[] =
{
    "track",
    "count",
};

enum LibvlcSubtitleNPObjectPropertyIds
{
    ID_subtitle_track,
    ID_subtitle_count,
};
COUNTNAMES(LibvlcSubtitleNPObject,propertyCount,propertyNames);

RuntimeNPObject::InvokeResult
LibvlcSubtitleNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
        auto& mp = p_plugin->getMD();
        if( !mp )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_subtitle_track:
            {
                /* get the current internal subtitles track ID */
                int actualTrack = p_plugin->player().currentSubtitleTrack();
                INT32_TO_NPVARIANT(actualTrack, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_subtitle_count:
            {
                /* get the number of subtitles available */
                int i_spu = mp.spuCount();
                /* return it */
                INT32_TO_NPVARIANT(i_spu, result);
                return INVOKERESULT_NO_ERROR;
            }
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcSubtitleNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
        auto& mp = p_plugin->getMD();
        if( !mp )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_subtitle_track:
            {
                auto v = npapi::Variant( value );
                if( v.is<int>() )
                {
                    auto tracks = mp.spuDescription();
                    if ( v >= tracks.size() )
                        return INVOKERESULT_INVALID_ARGS;
                    if ( mp.setSpu( tracks[ v ].id() ) )
                        return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            }
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcSubtitleNPObject::methodNames[] =
{
    "description"
};
COUNTNAMES(LibvlcSubtitleNPObject,methodCount,methodNames);

enum LibvlcSubtitleNPObjectMethodIds
{
    ID_subtitle_description
};

RuntimeNPObject::InvokeResult
LibvlcSubtitleNPObject::invoke(int index, const NPVariant *args,
                            uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
        auto& mp = p_plugin->getMD();
        if( !mp )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_subtitle_description:
            {
                if ( argCount < 1 )
                    return INVOKERESULT_INVALID_ARGS;
                auto v = npapi::Variant( args[0] );
                if ( v.is<int>() )
                {
                    auto tracks = mp.spuDescription();
                    if ( v >= tracks.size() )
                        return INVOKERESULT_INVALID_VALUE;
                    /* display the name of the track chosen */
                    return invokeResultString( tracks[v].name().c_str(), result );
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            default:
                return INVOKERESULT_NO_SUCH_METHOD;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

/*
** implementation of libvlc video object
*/

LibvlcVideoNPObject::~LibvlcVideoNPObject()
{
    if( isValid() )
    {
        if( marqueeObj ) NPN_ReleaseObject(marqueeObj);
        if( logoObj    ) NPN_ReleaseObject(logoObj);
        if( deintObj   ) NPN_ReleaseObject(deintObj);
    }
}

const NPUTF8 * const LibvlcVideoNPObject::propertyNames[] =
{
    "fullscreen",
    "height",
    "width",
    "aspectRatio",
    "subtitle",
    "crop",
    "teletext",
    "marquee",
    "logo",
    "deinterlace",
};

enum LibvlcVideoNPObjectPropertyIds
{
    ID_video_fullscreen,
    ID_video_height,
    ID_video_width,
    ID_video_aspectratio,
    ID_video_subtitle,
    ID_video_crop,
    ID_video_teletext,
    ID_video_marquee,
    ID_video_logo,
    ID_video_deinterlace,
};
COUNTNAMES(LibvlcVideoNPObject,propertyCount,propertyNames);

RuntimeNPObject::InvokeResult
LibvlcVideoNPObject::getProperty(int index, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
        auto& mp = p_plugin->getMD();
        if( !mp )
            RETURN_ON_ERROR;

        switch( index )
        {
            case ID_video_fullscreen:
            {
                int val = p_plugin->get_fullscreen();
                BOOLEAN_TO_NPVARIANT(val, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_height:
            {
                unsigned width, height;
                mp.size( 0, &width, &height );
                INT32_TO_NPVARIANT(height, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_width:
            {
                unsigned width, height;
                mp.size( 0, &width, &height );
                INT32_TO_NPVARIANT(width, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_aspectratio:
            {
                auto ar = mp.aspectRatio();
                if( ar.empty() )
                    return INVOKERESULT_GENERIC_ERROR;

                STRINGZ_TO_NPVARIANT(ar.c_str(), result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_subtitle:
            {
                int i_spu = mp.spu();
                INT32_TO_NPVARIANT(i_spu, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_crop:
            {
                auto geo = mp.cropGeometry();
                if( geo.empty() )
                    return INVOKERESULT_GENERIC_ERROR;

                STRINGZ_TO_NPVARIANT(geo.c_str(), result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_teletext:
            {
                int i_page = mp.teletext();
                if( i_page < 0 )
                    return INVOKERESULT_GENERIC_ERROR;
                INT32_TO_NPVARIANT(i_page, result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_marquee:
            {
                InstantObj<LibvlcMarqueeNPObject>( marqueeObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(marqueeObj), result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_logo:
            {
                InstantObj<LibvlcLogoNPObject>( logoObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(logoObj), result);
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_deinterlace:
            {
                InstantObj<LibvlcDeinterlaceNPObject>( deintObj );
                OBJECT_TO_NPVARIANT(NPN_RetainObject(deintObj), result);
                return INVOKERESULT_NO_ERROR;
            }
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcVideoNPObject::setProperty(int index, const NPVariant &value)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
        auto& mp = p_plugin->getMD();
        if( !mp )
            RETURN_ON_ERROR;

        auto v = npapi::Variant( value );

        switch( index )
        {
            case ID_video_fullscreen:
            {
                if( !v.is<bool>() )
                    return INVOKERESULT_INVALID_VALUE;

                p_plugin->set_fullscreen( (bool)v );
                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_aspectratio:
            {

                if( !v.is<NPString>() )
                    return INVOKERESULT_INVALID_VALUE;

                std::string ar = v;
                if ( ar == "default" )
                    ar = "";
                mp.setAspectRatio( ar );

                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_subtitle:
            {
                if( v.is<int>() )
                {
                    mp.setSpu( v );
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            }
            case ID_video_crop:
            {
                if( !v.is<NPString>() )
                {
                    return INVOKERESULT_INVALID_VALUE;
                }

                const char *psz_geometry = v;
                if( !psz_geometry )
                {
                    return INVOKERESULT_GENERIC_ERROR;
                }
                mp.setCropGeometry( psz_geometry );

                return INVOKERESULT_NO_ERROR;
            }
            case ID_video_teletext:
            {
                if( v.is<int>() )
                {
                    mp.setTeletext( v );
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_INVALID_VALUE;
            }
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

const NPUTF8 * const LibvlcVideoNPObject::methodNames[] =
{
    "toggleFullscreen",
    "toggleTeletext",
};
COUNTNAMES(LibvlcVideoNPObject,methodCount,methodNames);

enum LibvlcVideoNPObjectMethodIds
{
    ID_video_togglefullscreen,
    ID_video_toggleteletext,
};

RuntimeNPObject::InvokeResult
LibvlcVideoNPObject::invoke(int index, const NPVariant *,
                            uint32_t argCount, NPVariant &result)
{
    /* is plugin still running */
    if( isPluginRunning() )
    {
        VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();

        switch( index )
        {
            case ID_video_togglefullscreen:
            {
                if( argCount == 0 )
                {
                    p_plugin->toggle_fullscreen();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            case ID_video_toggleteletext:
            {
                if( argCount == 0 )
                {
                    p_plugin->getMD().toggleTeletext();
                    VOID_TO_NPVARIANT(result);
                    return INVOKERESULT_NO_ERROR;
                }
                return INVOKERESULT_NO_SUCH_METHOD;
            }
            default:
                return INVOKERESULT_NO_SUCH_METHOD;
        }
    }
    return INVOKERESULT_GENERIC_ERROR;
}

/*
** implementation of libvlc marquee object
*/

const NPUTF8 * const LibvlcMarqueeNPObject::propertyNames[] =
{
    "color",
    "opacity",
    "position",
    "refresh",
    "size",
    "text",
    "timeout",
    "x",
    "y",
};

enum LibvlcMarqueeNPObjectPropertyIds
{
    ID_marquee_color,
    ID_marquee_opacity,
    ID_marquee_position,
    ID_marquee_refresh,
    ID_marquee_size,
    ID_marquee_text,
    ID_marquee_timeout,
    ID_marquee_x,
    ID_marquee_y,
};

COUNTNAMES(LibvlcMarqueeNPObject,propertyCount,propertyNames);

static const unsigned char marquee_idx[] = {
    libvlc_marquee_Color,
    libvlc_marquee_Opacity,
    libvlc_marquee_Position,
    libvlc_marquee_Refresh,
    libvlc_marquee_Size,
    0,
    libvlc_marquee_Timeout,
    libvlc_marquee_X,
    libvlc_marquee_Y,
};

RuntimeNPObject::InvokeResult
LibvlcMarqueeNPObject::getProperty(int index, NPVariant &result)
{
    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
    auto& mp = p_plugin->getMD();
    if( !mp )
        RETURN_ON_ERROR;

    switch( index )
    {
    case ID_marquee_color:
    case ID_marquee_opacity:
    case ID_marquee_refresh:
    case ID_marquee_timeout:
    case ID_marquee_size:
    case ID_marquee_x:
    case ID_marquee_y:
        INT32_TO_NPVARIANT(
            mp.marqueeInt( marquee_idx[index]),
            result );
        return INVOKERESULT_NO_ERROR;

    case ID_marquee_position:
        STRINGZ_TO_NPVARIANT( position_bynumber(
            mp.marqueeInt( libvlc_marquee_Position ) ),
            result );

        break;

    case ID_marquee_text:
        auto marquee = mp.marqueeString( libvlc_marquee_Text );
        if( !marquee.empty() )
        {
            STRINGZ_TO_NPVARIANT(marquee.c_str(), result);
            return INVOKERESULT_NO_ERROR;
        }
        break;
    }
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcMarqueeNPObject::setProperty(int index, const NPVariant &value)
{
    size_t i;

    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
    auto& mp= p_plugin->getMD();
    if( !mp )
        RETURN_ON_ERROR;

    auto v = npapi::Variant( value );
    switch( index )
    {
    case ID_marquee_color:
    case ID_marquee_opacity:
    case ID_marquee_refresh:
    case ID_marquee_timeout:
    case ID_marquee_size:
    case ID_marquee_x:
    case ID_marquee_y:
        if( v.is<int>() )
        {
            mp.setMarqueeInt( marquee_idx[index], v );
            return INVOKERESULT_NO_ERROR;
        }
        break;

    case ID_marquee_position:
        if( !v.is<const char*>() ||
            !position_byname( v, i ) )
            return INVOKERESULT_INVALID_VALUE;

        mp.setMarqueeInt( libvlc_marquee_Position, i );
        return INVOKERESULT_NO_ERROR;

    case ID_marquee_text:
        if( v.is<const char*>() )
        {
            mp.setMarqueeString( libvlc_marquee_Text, v );
            return INVOKERESULT_NO_ERROR;
        }
        break;
    }
    return INVOKERESULT_NO_SUCH_METHOD;
}

const NPUTF8 * const LibvlcMarqueeNPObject::methodNames[] =
{
    "enable",
    "disable",
};
COUNTNAMES(LibvlcMarqueeNPObject,methodCount,methodNames);

enum LibvlcMarqueeNPObjectMethodIds
{
    ID_marquee_enable,
    ID_marquee_disable,
};

RuntimeNPObject::InvokeResult
LibvlcMarqueeNPObject::invoke(int index, const NPVariant *,
                              uint32_t, NPVariant &result)
{
    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
    auto& mp = p_plugin->getMD();
    if( !mp )
        RETURN_ON_ERROR;

    switch( index )
    {
    case ID_marquee_enable:
    case ID_marquee_disable:
        mp.setMarqueeInt( libvlc_marquee_Enable, index != ID_marquee_disable);
        VOID_TO_NPVARIANT(result);
        return INVOKERESULT_NO_ERROR;
    }
    return INVOKERESULT_NO_SUCH_METHOD;
}

const NPUTF8 * const LibvlcLogoNPObject::propertyNames[] = {
    "delay",
    "repeat",
    "opacity",
    "position",
    "x",
    "y",
};
enum LibvlcLogoNPObjectPropertyIds {
    ID_logo_delay,
    ID_logo_repeat,
    ID_logo_opacity,
    ID_logo_position,
    ID_logo_x,
    ID_logo_y,
};
COUNTNAMES(LibvlcLogoNPObject,propertyCount,propertyNames);
static const unsigned char logo_idx[] = {
    libvlc_logo_delay,
    libvlc_logo_repeat,
    libvlc_logo_opacity,
    0,
    libvlc_logo_x,
    libvlc_logo_y,
};

RuntimeNPObject::InvokeResult
LibvlcLogoNPObject::getProperty(int index, NPVariant &result)
{
    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
    auto& mp = p_plugin->getMD();
    if( !mp )
        RETURN_ON_ERROR;

    switch( index )
    {
    case ID_logo_delay:
    case ID_logo_repeat:
    case ID_logo_opacity:
    case ID_logo_x:
    case ID_logo_y:

        INT32_TO_NPVARIANT(
            mp.logoInt( logo_idx[index] ), result);
        break;

    case ID_logo_position:
        STRINGZ_TO_NPVARIANT( position_bynumber(
            mp.logoInt( libvlc_logo_position) ),
            result );
        break;
    default:
        return INVOKERESULT_GENERIC_ERROR;
    }
    return INVOKERESULT_NO_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcLogoNPObject::setProperty(int index, const NPVariant &value)
{
    size_t i;

    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    VlcPluginBase* p_plugin = getPrivate<VlcPluginBase>();
    auto& mp = p_plugin->getMD();
    if( !mp )
        RETURN_ON_ERROR;

    auto v = npapi::Variant( value );

    switch( index )
    {
    case ID_logo_delay:
    case ID_logo_repeat:
    case ID_logo_opacity:
    case ID_logo_x:
    case ID_logo_y:
        if( !v.is<int>() )
            return INVOKERESULT_INVALID_VALUE;

        mp.setLogoInt( logo_idx[index], v );
        break;

    case ID_logo_position:
        if( !NPVARIANT_IS_STRING(value) ||
            !position_byname( NPVARIANT_TO_STRING(value).UTF8Characters, i ) )
            return INVOKERESULT_INVALID_VALUE;

        mp.setLogoInt( libvlc_logo_position, i );
        break;
    default:
        return INVOKERESULT_GENERIC_ERROR;
    }
    return INVOKERESULT_NO_ERROR;
}


const NPUTF8 * const LibvlcLogoNPObject::methodNames[] = {
    "enable",
    "disable",
    "file",
};
enum LibvlcLogoNPObjectMethodIds {
    ID_logo_enable,
    ID_logo_disable,
    ID_logo_file,
};
COUNTNAMES(LibvlcLogoNPObject,methodCount,methodNames);

RuntimeNPObject::InvokeResult
LibvlcLogoNPObject::invoke(int index, const NPVariant *args,
                           uint32_t argCount, NPVariant &result)
{
    char *buf, *h;
    size_t i, len;

    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    auto& mp = getPrivate<VlcPluginBase>()->getMD();
    if( !mp )
        RETURN_ON_ERROR;

    switch( index )
    {
    case ID_logo_enable:
    case ID_logo_disable:
        if( argCount != 0 )
            return INVOKERESULT_GENERIC_ERROR;
        mp.setLogoInt( libvlc_logo_enable, index != ID_logo_disable);
        VOID_TO_NPVARIANT(result);
        break;

    case ID_logo_file:
        if( argCount == 0 )
            return INVOKERESULT_GENERIC_ERROR;

        for( len=0,i=0;i<argCount;++i )
        {
            if( !NPVARIANT_IS_STRING(args[i]) )
                return INVOKERESULT_INVALID_VALUE;
            len+=NPVARIANT_TO_STRING(args[i]).UTF8Length+1;
        }

        buf = (char *)malloc( len+1 );
        if( !buf )
            return INVOKERESULT_OUT_OF_MEMORY;

        for( h=buf,i=0;i<argCount;++i )
        {
            if(i) *h++=';';
            len=NPVARIANT_TO_STRING(args[i]).UTF8Length;
            memcpy(h,NPVARIANT_TO_STRING(args[i]).UTF8Characters,len);
            h+=len;
        }
        *h='\0';

        mp.setLogoString( libvlc_logo_file, buf );
        free( buf );
        VOID_TO_NPVARIANT(result);
        break;
    default:
        return INVOKERESULT_NO_SUCH_METHOD;
    }
    return INVOKERESULT_NO_ERROR;
}

// MSVC++ doesn't support zero length arrays, so insert dummy "0"
const NPUTF8 * const LibvlcDeinterlaceNPObject::propertyNames[] = {
    0
};
enum LibvlcDeinterlaceNPObjectPropertyIds {
};
const int LibvlcDeinterlaceNPObject::propertyCount=0;

RuntimeNPObject::InvokeResult
LibvlcDeinterlaceNPObject::getProperty(int, NPVariant &)
{
    return INVOKERESULT_GENERIC_ERROR;
}

RuntimeNPObject::InvokeResult
LibvlcDeinterlaceNPObject::setProperty(int, const NPVariant &)
{
    return INVOKERESULT_GENERIC_ERROR;
}


const NPUTF8 * const LibvlcDeinterlaceNPObject::methodNames[] = {
    "enable",
    "disable",
};
enum LibvlcDeinterlaceNPObjectMethodIds {
    ID_deint_enable,
    ID_deint_disable,
};
COUNTNAMES(LibvlcDeinterlaceNPObject,methodCount,methodNames);

RuntimeNPObject::InvokeResult
LibvlcDeinterlaceNPObject::invoke(int index, const NPVariant *args,
                                  uint32_t argCount, NPVariant &)
{
    char *psz;

    if( !isPluginRunning() )
        return INVOKERESULT_GENERIC_ERROR;

    auto& mp = getPrivate<VlcPluginBase>()->getMD();
    if( !mp )
        RETURN_ON_ERROR;

    switch( index )
    {
    case ID_deint_disable:
        mp.setDeinterlace( std::string() );
        break;

    case ID_deint_enable:
    {
        if( argCount < 1 )
            return INVOKERESULT_INVALID_VALUE;
        auto v = npapi::Variant( args[0] );
        if ( !v.is<const char*>() )
            return INVOKERESULT_INVALID_VALUE;

        mp.setDeinterlace( v );
        break;
    }
    default:
        return INVOKERESULT_NO_SUCH_METHOD;
    }
    return INVOKERESULT_NO_ERROR;
}
