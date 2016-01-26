/*****************************************************************************
 * vlcplugin_base.cpp: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Damien Fouilleul <damienf.fouilleul@laposte.net>
 *          Jean-Paul Saman <jpsaman@videolan.org>
 *          Sergey Radionov <rsatom@gmail.com>
 *          Cheng Sun <chengsun9@gmail.com>
 *          Yannick Brehon <y.brehon@qiplay.com>
 *          Felix Paul Kühne <fkuehne # videolan.org>
 *          Ludovic Fauvet <etix@videolan.org>
 *          Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "vlcplugin_base.h"
#include "vlcplugin.h"

#include "npruntime/npolibvlc.h"

#include <cctype>

#include <cstdio>
#include <cstdlib>
#include <cstring>

/*****************************************************************************
 * VlcPluginBase constructor and destructor
 *****************************************************************************/
VlcPluginBase::VlcPluginBase( NPP instance, NPuint16_t mode ) :
    i_npmode(mode),
    b_stream(0),
    psz_target(NULL),
    p_scriptClass(NULL),
    p_browser(instance),
    psz_baseURL(NULL)
{
    memset(&npwindow, 0, sizeof(NPWindow));
}

static bool boolValue(const char *value) {
    return ( *value == '\0' ||
             !strcmp(value, "1") ||
             !strcasecmp(value, "true") ||
             !strcasecmp(value, "yes") );
}

NPError VlcPluginBase::init(int argc, char* const argn[], char* const argv[])
{
    /* prepare VLC command line */
    const char *ppsz_argv[MAX_PARAMS];
    int ppsz_argc = 0;

#ifndef NDEBUG
    ppsz_argv[ppsz_argc++] = "--no-plugins-cache";
#endif

    /* locate VLC module path */
#ifdef XP_WIN
    HKEY h_key;
    DWORD i_type, i_data = MAX_PATH + 1;
    char p_data[MAX_PATH + 1];
    if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, "Software\\VideoLAN\\VLC",
                      0, KEY_READ, &h_key ) == ERROR_SUCCESS )
    {
         if( RegQueryValueEx( h_key, "InstallDir", 0, &i_type,
                              (LPBYTE)p_data, &i_data ) == ERROR_SUCCESS )
         {
             if( i_type == REG_SZ )
             {
                 strcat( p_data, "\\plugins" );
                 ppsz_argv[ppsz_argc++] = "--plugin-path";
                 ppsz_argv[ppsz_argc++] = p_data;
             }
         }
         RegCloseKey( h_key );
    }
    ppsz_argv[ppsz_argc++] = "--no-one-instance";

#endif
#ifdef XP_MACOSX
    ppsz_argv[ppsz_argc++] = "--vout=caopengllayer";
    ppsz_argv[ppsz_argc++] = "--scaletempo-stride=30";
    ppsz_argv[ppsz_argc++] = "--scaletempo-overlap=0,2";
    ppsz_argv[ppsz_argc++] = "--scaletempo-search=14";
    ppsz_argv[ppsz_argc++] = "--auhal-volume=256";
    ppsz_argv[ppsz_argc++] = "--auhal-audio-device=0";
    ppsz_argv[ppsz_argc++] = "--no-volume-save";
#endif

    /* common settings */
#ifndef NDEBUG
    ppsz_argv[ppsz_argc++] = "-vv";
#else
    ppsz_argv[ppsz_argc++] = "--quiet";
#endif
    ppsz_argv[ppsz_argc++] = "--no-stats";
    ppsz_argv[ppsz_argc++] = "--no-media-library";
    ppsz_argv[ppsz_argc++] = "--intf=dummy";
    ppsz_argv[ppsz_argc++] = "--no-video-title-show";
    ppsz_argv[ppsz_argc++] = "--no-xlib";

    bool b_autoloop = false;
    bool b_mute = false;
    int i_volume = -1;

    /* parse plugin arguments */
    for( int i = 0; (i < argc) && (ppsz_argc < MAX_PARAMS); i++ )
    {
       /* fprintf(stderr, "argn=%s, argv=%s\n", argn[i], argv[i]); */

        if( !strcmp( argn[i], "target" )
         || !strcmp( argn[i], "mrl")
         || !strcmp( argn[i], "filename")
         || !strcmp( argn[i], "src") )
        {
            psz_target = argv[i];
        }
        else if( !strcmp( argn[i], "text" ) )
        {
            set_bg_text( argv[i] );
        }
        else if( !strcmp( argn[i], "autoplay")
              || !strcmp( argn[i], "autostart") )
        {
            set_autoplay( boolValue(argv[i]) );
        }
        else if( !strcmp( argn[i], "fullscreen" )
              || !strcmp( argn[i], "allowfullscreen" )
              || !strcmp( argn[i], "fullscreenenabled" ) )
        {
            set_enable_fs( boolValue(argv[i]) );
        }
        else if( !strcmp( argn[i], "mute" ) )
        {
            b_mute = boolValue( argv[i] );
        }
        else if( !strcmp( argn[i], "volume" ) )
        {
            i_volume = atoi( argv[i] );
        }
        else if( !strcmp( argn[i], "loop")
              || !strcmp( argn[i], "autoloop") )
        {
            b_autoloop = boolValue(argv[i]);
        }
        else if( !strcmp( argn[i], "toolbar" )
              || !strcmp( argn[i], "controls") )
        {
            set_show_toolbar( boolValue(argv[i]) );
        }
        else if( !strcmp( argn[i], "bgcolor" ) )
        {
            set_bg_color( argv[i] );
        }
        else if( !strcmp( argn[i], "branding" ) )
        {
            set_enable_branding( boolValue(argv[i]) );
        }
    }

    try {
        VLC::Instance instance( ppsz_argc, ppsz_argv );
        m_player.open(instance);
    }
    catch (std::runtime_error&) {
        return NPERR_GENERIC_ERROR;
    }

    m_player.mlp().setPlaybackMode( b_autoloop ? libvlc_playback_mode_loop :
                                      libvlc_playback_mode_default );

    if( b_mute )
        m_player.get_mp().setMute( true );

    if( i_volume >= 0 && i_volume <= 200 )
        m_player.get_mp().setVolume( i_volume );

    /*
    ** fetch plugin base URL, which is the URL of the page containing the plugin
    ** this URL is used for making absolute URL from relative URL that may be
    ** passed as an MRL argument
    */
    NPObject *plugin = NULL;

    if( NPERR_NO_ERROR == NPN_GetValue(p_browser, NPNVWindowNPObject, &plugin) )
    {
        /*
        ** is there a better way to get that info ?
        */
        static const char docLocHref[] = "document.location.href";
        NPString script;
        NPVariant result;

        script.UTF8Characters = docLocHref;
        script.UTF8Length = sizeof(docLocHref)-1;

        if( NPN_Evaluate(p_browser, plugin, &script, &result) )
        {
            if( NPVARIANT_IS_STRING(result) )
            {
                NPString &location = NPVARIANT_TO_STRING(result);

                psz_baseURL = (char *) malloc(location.UTF8Length+1);
                if( psz_baseURL )
                {
                    strncpy(psz_baseURL, location.UTF8Characters, location.UTF8Length);
                    psz_baseURL[location.UTF8Length] = '\0';
                }
            }
            NPN_ReleaseVariantValue(&result);
        }
        NPN_ReleaseObject(plugin);
    }

    if( psz_target )
    {
        // get absolute URL from src
        char *psz_absurl = getAbsoluteURL(psz_target);
        psz_target = psz_absurl ? psz_absurl : strdup(psz_target);
    }

    /* assign plugin script root class */
    /* new APIs */
    p_scriptClass = RuntimeNPClass<LibvlcRootNPObject>::getClass();

    // Update the UI if required when we switch media
    m_player.mlp().eventManager().onNextItemSet([this](VLC::MediaPtr) {
        update_controls();
    });

    return NPERR_NO_ERROR;
}

VlcPluginBase::~VlcPluginBase()
{
    free(psz_baseURL);
    free(psz_target);
}

void VlcPluginBase::setWindow(const NPWindow &window)
{
    npwindow = window;
}

#if defined(XP_MACOSX)
NPError VlcPluginBase::get_root_layer(void *value)
{
    return NPERR_GENERIC_ERROR;
}
#endif

bool VlcPluginBase::handle_event(void *)
{
    return false;
}

struct AsyncEventWrapper
{
    AsyncEventWrapper(NPP b, NPObject* l, npapi::VariantArray&& a)
        : browser( b )
        , listener( l )
        , args( std::move( a ) )
    {
    }

    NPP browser;
    NPObject* listener;
    npapi::VariantArray args;
};

template <typename... Args>
static void invokeEvent( NPP browser, NPObject* listener, Args&&... args )
{
    auto wrapper = new AsyncEventWrapper( browser, listener, npapi::wrap( std::forward<Args>( args )... ) );
    NPN_PluginThreadAsyncCall( browser, [](void* data) {
        auto w = reinterpret_cast<AsyncEventWrapper*>( data );
        NPVariant result;
        if (NPN_InvokeDefault( w->browser, w->listener, w->args, w->args.size(), &result ))
        {
            // "result" content is unspecified when invoke fails. Don't clean potential garbage
            NPN_ReleaseVariantValue( &result );
        }
        delete w;
    }, wrapper);
}

class CallbackClosure
{
public:
    CallbackClosure(NPP browser, npapi::Variant listener)
        : _browser( browser )
        , _listener( std::move( listener ) )
    {
    }

    CallbackClosure(const CallbackClosure&) = delete;

#ifndef _MSC_VER
    CallbackClosure(CallbackClosure&&) = default;
#else
    CallbackClosure(CallbackClosure&& c)
        : _browser( std::move(c._browser) )
        , _listener( std::move(c._listener) )
    {
    }
#endif

    template <typename... Args>
    void operator()(Args&&... params) const
    {
        // This is expected to receive a copy of the listener
        invokeEvent( _browser, _listener, std::forward<Args>( params )... );
    }

    void operator()(VLC::MediaPtr) const
    {
        // Force Media to be ignored since we don't have a binding for it.
        // This is expected to receive a copy of the listener
        invokeEvent( _browser, _listener );
    }
private:
    NPP _browser;
    npapi::Variant _listener;
};

static struct vlcevents_t {
    const char* name;
    libvlc_event_type_t type;
} vlcevents[] = {
    { "MediaPlayerMediaChanged", libvlc_MediaPlayerMediaChanged },
    { "MediaPlayerNothingSpecial", libvlc_MediaPlayerNothingSpecial },
    { "MediaPlayerOpening", libvlc_MediaPlayerOpening },
    { "MediaPlayerBuffering", libvlc_MediaPlayerBuffering },
    { "MediaPlayerPlaying", libvlc_MediaPlayerPlaying },
    { "MediaPlayerPaused", libvlc_MediaPlayerPaused },
    { "MediaPlayerStopped", libvlc_MediaPlayerStopped },
    { "MediaPlayerForward", libvlc_MediaPlayerForward },
    { "MediaPlayerBackward", libvlc_MediaPlayerBackward },
    { "MediaPlayerEndReached", libvlc_MediaPlayerEndReached },
    { "MediaPlayerEncounteredError", libvlc_MediaPlayerEncounteredError },
    { "MediaPlayerTimeChanged", libvlc_MediaPlayerTimeChanged },
    { "MediaPlayerPositionChanged", libvlc_MediaPlayerPositionChanged },
    { "MediaPlayerSeekableChanged", libvlc_MediaPlayerSeekableChanged },
    { "MediaPlayerPausableChanged", libvlc_MediaPlayerPausableChanged },
    { "MediaPlayerTitleChanged", libvlc_MediaPlayerTitleChanged },
    { "MediaPlayerLengthChanged", libvlc_MediaPlayerLengthChanged },
};

void VlcPluginBase::subscribe(const char* eventName, npapi::Variant listener)
{
    auto event = std::find_if(std::begin(vlcevents), std::end(vlcevents), [eventName](const vlcevents_t& e) {
        return !strcmp( e.name, eventName);
    });
    if (event == std::end(vlcevents))
        return;

    auto listenerRaw = (NPObject*)listener;
    auto closure = CallbackClosure{ p_browser, std::move( listener ) };

    VLC::EventManager::RegisteredEvent e = nullptr;
    switch ( event->type )
    {
        case libvlc_MediaPlayerNothingSpecial:
            e = player().get_mp().eventManager().onNothingSpecial( std::move( closure ) );
            break;
        case libvlc_MediaPlayerOpening:
            e = player().get_mp().eventManager().onOpening( std::move( closure ) );
            break;
        case libvlc_MediaPlayerPlaying:
            e = player().get_mp().eventManager().onPlaying( std::move( closure ) );
            break;
        case libvlc_MediaPlayerPaused:
            e = player().get_mp().eventManager().onPaused( std::move( closure ) );
            break;
        case libvlc_MediaPlayerStopped:
            e = player().get_mp().eventManager().onStopped( std::move( closure ) );
            break;
        case libvlc_MediaPlayerForward:
            e = player().get_mp().eventManager().onForward( std::move( closure ) );
            break;
        case libvlc_MediaPlayerBackward:
            e = player().get_mp().eventManager().onBackward( std::move( closure ) );
            break;
        case libvlc_MediaPlayerEndReached:
            e = player().get_mp().eventManager().onEndReached( std::move( closure ) );
            break;
        case libvlc_MediaPlayerEncounteredError:
            e = player().get_mp().eventManager().onEncounteredError( std::move( closure ) );
            break;
        case libvlc_MediaPlayerBuffering:
            e = player().get_mp().eventManager().onBuffering( std::move( closure ) );
            break;
        case libvlc_MediaPlayerTimeChanged:
            e = player().get_mp().eventManager().onTimeChanged( std::move( closure ) );
            break;
        case libvlc_MediaPlayerMediaChanged:
            e = player().get_mp().eventManager().onMediaChanged( std::move( closure ) );
            break;
        case libvlc_MediaPlayerPositionChanged:
            e = player().get_mp().eventManager().onPositionChanged( std::move( closure ) );
            break;
        case libvlc_MediaPlayerSeekableChanged:
            e = player().get_mp().eventManager().onSeekableChanged( std::move( closure ) );
            break;
        case libvlc_MediaPlayerPausableChanged:
            e = player().get_mp().eventManager().onPausableChanged( std::move( closure ) );
            break;
        case libvlc_MediaPlayerTitleChanged:
            e = player().get_mp().eventManager().onTitleChanged( std::move( closure ) );
            break;
        case libvlc_MediaPlayerLengthChanged:
            e = player().get_mp().eventManager().onLengthChanged( std::move( closure ) );
            break;
        default:
            break;
    }
    if ( e != nullptr )
    {
        m_events.emplace_back( std::string(eventName), listenerRaw, e );
    }
}

void VlcPluginBase::unsubscribe(const char* eventName, npapi::Variant listener)
{
    auto event = std::find_if( begin( m_events ), end( m_events ), [eventName, listener](const decltype(m_events)::value_type e) {
        return std::get<0>( e ) == eventName && std::get<1>( e ) == listener;
    });
    if ( event == end( m_events ) )
        return;
    std::get<2>( *event )->unregister();
    m_events.erase( event );
}

/*****************************************************************************
 * VlcPluginBase methods
 *****************************************************************************/

char *VlcPluginBase::getAbsoluteURL(const char *url)
{
    if( NULL != url )
    {
        // check whether URL is already absolute
        const char *end=strchr(url, ':');
        if( (NULL != end) && (end != url) )
        {
            // validate protocol header
            const char *start = url;
            char c = *start;
            if( isalpha(c) )
            {
                ++start;
                while( start != end )
                {
                    c  = *start;
                    if( ! (isalnum(c)
                       || ('-' == c)
                       || ('+' == c)
                       || ('.' == c)
                       || ('/' == c)) ) /* VLC uses / to allow user to specify a demuxer */
                        // not valid protocol header, assume relative URL
                        goto relativeurl;
                    ++start;
                }
                /* we have a protocol header, therefore URL is absolute */
                return strdup(url);
            }
            // not a valid protocol header, assume relative URL
        }

relativeurl:

        if( psz_baseURL )
        {
            size_t baseLen = strlen(psz_baseURL);
            char *href = (char *) malloc(baseLen+strlen(url)+1);
            if( href )
            {
                /* prepend base URL */
                memcpy(href, psz_baseURL, baseLen+1);

                /*
                ** relative url could be empty,
                ** in which case return base URL
                */
                if( '\0' == *url )
                    return href;

                /*
                ** locate pathname part of base URL
                */

                /* skip over protocol part  */
                char *pathstart = strchr(href, ':');
                char *pathend = href+baseLen;
                if( pathstart )
                {
                    if( '/' == *(++pathstart) )
                    {
                        if( '/' == *(++pathstart) )
                        {
                            ++pathstart;
                        }
                    }
                    /* skip over host part */
                    pathstart = strchr(pathstart, '/');
                    if( ! pathstart )
                    {
                        // no path, add a / past end of url (over '\0')
                        pathstart = pathend;
                        *pathstart = '/';
                    }
                }
                else
                {
                    /* baseURL is just a UNIX path */
                    if( '/' != *href )
                    {
                        /* baseURL is not an absolute path */
                        free(href);
                        return NULL;
                    }
                    pathstart = href;
                }

                /* relative URL made of an absolute path ? */
                if( '/' == *url )
                {
                    /* replace path completely */
                    strcpy(pathstart, url);
                    return href;
                }

                /* find last path component and replace it */
                while( '/' != *pathend)
                    --pathend;

                /*
                ** if relative url path starts with one or more '../',
                ** factor them out of href so that we return a
                ** normalized URL
                */
                while( pathend != pathstart )
                {
                    const char *p = url;
                    if( '.' != *p )
                        break;
                    ++p;
                    if( '\0' == *p  )
                    {
                        /* relative url is just '.' */
                        url = p;
                        break;
                    }
                    if( '/' == *p  )
                    {
                        /* relative url starts with './' */
                        url = ++p;
                        continue;
                    }
                    if( '.' != *p )
                        break;
                    ++p;
                    if( '\0' == *p )
                    {
                        /* relative url is '..' */
                    }
                    else
                    {
                        if( '/' != *p )
                            break;
                        /* relative url starts with '../' */
                        ++p;
                    }
                    url = p;
                    do
                    {
                        --pathend;
                    }
                    while( '/' != *pathend );
                }
                /* skip over '/' separator */
                ++pathend;
                /* concatenate remaining base URL and relative URL */
                strcpy(pathend, url);
            }
            return href;
        }
    }
    return NULL;
}

void VlcPluginBase::control_handler(vlc_toolbar_clicked_t clicked)
{
    switch( clicked )
    {
        case clicked_Play:
        {
            player().play();
        }
        break;

        case clicked_Pause:
        {
            player().get_mp().pause();
        }
        break;

        case clicked_Stop:
        {
            player().get_mp().stop();
        }
        break;

        case clicked_Fullscreen:
        {
            toggle_fullscreen();
        }
        break;

        case clicked_Mute:
        case clicked_Unmute:
#if 0
        {
            if( p_md )
                libvlc_audio_toggle_mute( p_md );
        }
#endif
        break;

        case clicked_timeline:
#if 0
        {
            /* if a movie is loaded */
            if( p_md )
            {
                int64_t f_length;
                f_length = libvlc_media_player_get_length( p_md ) / 100;

                f_length = (float)f_length *
                        ( ((float)i_xPos-4.0 ) / ( ((float)i_width-8.0)/100) );

                libvlc_media_player_set_time( p_md, f_length );
            }
        }
#endif
        break;

        case clicked_Time:
        {
            /* Not implemented yet*/
        }
        break;

        default: /* button_Unknown */
            fprintf(stderr, "button Unknown!\n");
        break;
    }
}

// Verifies the version of the NPAPI.
// The eventListeners use a NPAPI function available
// since Gecko 1.9.
bool VlcPluginBase::canUseEventListener()
{
    int plugin_major, plugin_minor;
    int browser_major, browser_minor;

    NPN_Version(&plugin_major, &plugin_minor,
                &browser_major, &browser_minor);

    if (browser_minor >= 19 || browser_major > 0)
        return true;
    return false;
}

