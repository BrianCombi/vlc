/*****************************************************************************
 * skin-main.cpp: skins plugin for VLC
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: skin_main.cpp,v 1.26 2003/05/12 17:33:19 gbazin Exp $
 *
 * Authors: Olivier Teuli�re <ipkiss@via.ecp.fr>
 *          Emmanuel Puig    <karibu@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111,
 * USA.
 *****************************************************************************/

//--- VLC -------------------------------------------------------------------
#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/aout.h>

//--- GENERAL ---------------------------------------------------------------
#ifndef BASIC_SKINS
#ifdef WIN32                                               /* mingw32 hack */
#   undef Yield
#   undef CreateDialog
#endif
/* Let vlc take care of the i18n stuff */
#define WXINTL_NO_GETTEXT_MACRO
#include <wx/wx.h>
#endif

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "event.h"
#include "banks.h"
#include "window.h"
#include "theme.h"
#include "../os_theme.h"
#include "themeloader.h"
#include "vlcproc.h"
#include "skin_common.h"
#ifndef BASIC_SKINS
#include "../../wxwindows/wxwindows.h"
#endif

#ifdef X11_SKINS
#include <X11/Xlib.h>
#endif

//---------------------------------------------------------------------------
// Interface thread
// It is a global variable because we have C code for the parser, and we
// need to access C++ objects from there
//---------------------------------------------------------------------------
intf_thread_t *g_pIntf;

//---------------------------------------------------------------------------
// Exported interface functions.
//---------------------------------------------------------------------------
#ifdef WIN32
extern "C" __declspec( dllexport )
    int __VLC_SYMBOL( vlc_entry ) ( module_t *p_module );
#endif

//---------------------------------------------------------------------------
// Local prototypes.
//---------------------------------------------------------------------------
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );
static void Run    ( intf_thread_t * );

int  SkinManage( intf_thread_t *p_intf );
void OSRun( intf_thread_t *p_intf );

//---------------------------------------------------------------------------
// Open: initialize interface
//---------------------------------------------------------------------------
static int Open ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    g_pIntf = p_intf;

    // Allocate instance and initialize some members
    p_intf->p_sys = (intf_sys_t *) malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return( 1 );
    };

    p_intf->pf_run = Run;


    // Suscribe to messages bank
    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );

    // Set no new theme when opening file
    p_intf->p_sys->p_new_theme_file = NULL;

    // Initialize info on playlist
    p_intf->p_sys->i_index        = -1;
    p_intf->p_sys->i_size         = 0;

    p_intf->p_sys->i_close_status = VLC_NOTHING;

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->p_playlist = (playlist_t *)vlc_object_find( p_intf,
        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

#ifdef GTK2_SKINS
    // Initialize GDK
    int    i_args   = 3;
    char  *p_args[] = { "", "", "--sync", NULL };
    char **pp_args  = p_args;

    gdk_init( &i_args, &pp_args );

#elif defined X11_SKINS
    // Initialize X11
    p_intf->p_sys->display = XOpenDisplay( NULL );

#elif defined WIN32
    // We dynamically load msimg32.dll to get a pointer to TransparentBlt()
    p_intf->p_sys->h_msimg32_dll = LoadLibrary("msimg32.dll");
    if( !p_intf->p_sys->h_msimg32_dll ||
        !( p_intf->p_sys->TransparentBlt =
           (BOOL (WINAPI*)(HDC,int,int,int,int,HDC,
                           int,int,int,int,unsigned int))
           GetProcAddress( p_intf->p_sys->h_msimg32_dll, "TransparentBlt" ) ) )
    {
        p_intf->p_sys->TransparentBlt = NULL;
        msg_Dbg( p_intf, "Couldn't find TransparentBlt(), "
                 "falling back to BitBlt()" );
    }

    // idem for user32.dll and SetLayeredWindowAttributes()
    p_intf->p_sys->h_user32_dll = LoadLibrary("user32.dll");
    if( !p_intf->p_sys->h_user32_dll ||
        !( p_intf->p_sys->SetLayeredWindowAttributes =
           (BOOL (WINAPI *)(HWND,COLORREF,BYTE,DWORD))
           GetProcAddress( p_intf->p_sys->h_user32_dll,
                           "SetLayeredWindowAttributes" ) ) )
    {
        p_intf->p_sys->SetLayeredWindowAttributes = NULL;
        msg_Dbg( p_intf, "Couldn't find SetLayeredWindowAttributes()" );
    }

#endif

#ifndef BASIC_SKINS
    // Initialize conditions and mutexes
    vlc_mutex_init( p_intf, &p_intf->p_sys->init_lock );
    vlc_cond_init( p_intf, &p_intf->p_sys->init_cond );
#endif

    p_intf->p_sys->p_theme = (Theme *)new OSTheme( p_intf );

    return( 0 );
}

//---------------------------------------------------------------------------
// Close: destroy interface
//---------------------------------------------------------------------------
static void Close ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;


    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }

    if( p_intf->p_sys->p_playlist )
    {
        vlc_object_release( p_intf->p_sys->p_playlist );
    }

    // Delete theme, it's important to do it correctly
    delete (OSTheme *)p_intf->p_sys->p_theme;

    // Unsuscribe to messages bank
    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );

#ifndef BASIC_SKINS
    // Destroy conditions and mutexes
    vlc_cond_destroy( &p_intf->p_sys->init_cond );
    vlc_mutex_destroy( &p_intf->p_sys->init_lock );
#endif

#ifdef WIN32
    // Unload msimg32.dll and user32.dll
    if( p_intf->p_sys->h_msimg32_dll )
        FreeLibrary( p_intf->p_sys->h_msimg32_dll );
    if( p_intf->p_sys->h_user32_dll )
        FreeLibrary( p_intf->p_sys->h_user32_dll );
#endif

    // Destroy structure
    free( p_intf->p_sys );
}


//---------------------------------------------------------------------------
// Run: main loop
//---------------------------------------------------------------------------
static void Run( intf_thread_t *p_intf )
{

    int a = OSAPI_GetTime();

    // Load a theme
    char *skin_last = config_GetPsz( p_intf, "skin_last" );
    ThemeLoader *Loader = new ThemeLoader( p_intf );

    if( skin_last == NULL || ! Loader->Load( skin_last ) )
    {
        // Too bad, it failed. Let's try with the default theme
#if 0
        if( ! Loader->Load( DEFAULT_SKIN_FILE ) )
#else
#ifdef WIN32
        string default_dir = (string)p_intf->p_libvlc->psz_vlcpath +
                             DIRECTORY_SEPARATOR + "skins" +
                             DIRECTORY_SEPARATOR + "default" +
                             DIRECTORY_SEPARATOR + "theme.xml";
#else
// FIXME: find VLC directory 
        string default_dir = (string)"./share" +
                             DIRECTORY_SEPARATOR + "skins" +
                             DIRECTORY_SEPARATOR + "default" +
                             DIRECTORY_SEPARATOR + "theme.xml";
#endif
        if( ! Loader->Load( default_dir ) )
#endif
        {
            // Last chance: the user can  select a new theme file
// FIXME: wxWindows isn't initialized yet !!!
#if 0
#ifndef BASIC_SKINS
            wxFileDialog dialog( NULL, _("Open a skin file"), "", "",
                "Skin files (*.vlt)|*.vlt|Skin files (*.xml)|*.xml|"
                    "All files|*.*", wxOPEN );

            if( dialog.ShowModal() == wxID_OK )
            {
                // try to load selected file
                if( ! Loader->Load( dialog.GetPath().c_str() ) )
                {
                    // He, he, what the hell is he doing ?
                    delete Loader;
                    return;
                }
            }
            else
#endif
#endif
            {
                delete Loader;
                return;
            }
        }
    }

    // Show the theme
    p_intf->p_sys->p_theme->InitTheme();
    p_intf->p_sys->p_theme->ShowTheme();

    delete Loader;

    msg_Err( p_intf, "Load theme time : %i ms", OSAPI_GetTime() - a );

    // Refresh the whole interface
    OSAPI_PostMessage( NULL, VLC_INTF_REFRESH, 0, (int)true );

    OSRun( p_intf );
}

//---------------------------------------------------------------------------
// Module descriptor
//---------------------------------------------------------------------------
#define DEFAULT_SKIN        N_("Last skin actually used")
#define DEFAULT_SKIN_LONG   N_("Last skin actually used")
#define SKIN_CONFIG         N_("Config of last used skin")
#define SKIN_CONFIG_LONG    N_("Config of last used skin")
#define SKIN_TRAY           N_("Show application in system tray")
#define SKIN_TRAY_LONG      N_("Show application in system tray")
#define SKIN_TASKBAR        N_("Show application in taskbar")
#define SKIN_TASKBAR_LONG   N_("Show application in taskbar")

vlc_module_begin();
    add_string( "skin_last", "", NULL, DEFAULT_SKIN, DEFAULT_SKIN_LONG,
                VLC_TRUE );
    add_string( "skin_config", "", NULL, SKIN_CONFIG, SKIN_CONFIG_LONG,
                VLC_TRUE );
    add_bool( "show_in_tray", VLC_FALSE, NULL, SKIN_TRAY, SKIN_TRAY_LONG,
              VLC_FALSE );
    add_bool( "show_in_taskbar", VLC_TRUE, NULL, SKIN_TASKBAR,
              SKIN_TASKBAR_LONG, VLC_FALSE );
    set_description( _("Skinnable Interface") );
    set_capability( "interface", 30 );
    set_callbacks( Open, Close );
    add_shortcut( "skins" );
vlc_module_end();


//---------------------------------------------------------------------------
// Refresh procedure
//---------------------------------------------------------------------------
int SkinManage( intf_thread_t *p_intf )
{
    vlc_mutex_lock( &p_intf->change_lock );

    // Update the input
    if( p_intf->p_sys->p_input == NULL )
    {
        p_intf->p_sys->p_input = (input_thread_t *)
                    vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    }
    else if( p_intf->p_sys->p_input->b_dead )
    {
        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;
    }

    OSAPI_PostMessage( NULL, VLC_INTF_REFRESH, 0, (long)false );

#ifndef BASIC_SKINS
    // Update the log window
    p_intf->p_sys->MessagesDlg->UpdateLog();

    // Update the file info window
    p_intf->p_sys->InfoDlg->UpdateFileInfo();
#endif

    //-------------------------------------------------------------------------
    if( p_intf->p_sys->p_input != NULL && !p_intf->p_sys->p_input->b_die )
    {
        input_thread_t  * p_input = p_intf->p_sys->p_input;

        vlc_mutex_lock( &p_input->stream.stream_lock );

        // Refresh sound volume
        audio_volume_t volume;

        // Get sound volume from VLC
        aout_VolumeGet( p_intf, &volume);

        // Update sliders
        OSAPI_PostMessage( NULL, CTRL_SET_SLIDER,
            (unsigned int)
            p_intf->p_sys->p_theme->EvtBank->Get( "volume_refresh" ),
            (long)( volume * SLIDER_RANGE / AOUT_VOLUME_MAX ) );


        // Refresh slider
        // if( p_input->stream.b_seekable && p_intf->p_sys->b_playing )
        if( p_input->stream.b_seekable )
        {
#define p_area p_input->stream.p_selected_area

            // Set value of sliders
            long Value = SLIDER_RANGE *
                p_input->stream.p_selected_area->i_tell /
                p_input->stream.p_selected_area->i_size;

            // Update sliders
            OSAPI_PostMessage( NULL, CTRL_SET_SLIDER, (unsigned int)
                p_intf->p_sys->p_theme->EvtBank->Get( "time" ), (long)Value );

            // Text char * for updating text controls
            char *text = new char[OFFSETTOTIME_MAX_SIZE];

            // Create end time text
            input_OffsetToTime( p_intf->p_sys->p_input, &text[1],
                                p_area->i_size - p_area->i_tell );
            text[0] = '-';
            p_intf->p_sys->p_theme->EvtBank->Get( "left_time" )
                ->PostTextMessage( text );

            // Create time text and update
            input_OffsetToTime( p_intf->p_sys->p_input, text, p_area->i_tell );
            p_intf->p_sys->p_theme->EvtBank->Get( "time" )
                ->PostTextMessage( text );

            // Create total time text
            input_OffsetToTime( p_intf->p_sys->p_input, text, p_area->i_size );
            p_intf->p_sys->p_theme->EvtBank->Get( "total_time" )
                ->PostTextMessage( text );

            // Free memory
            delete[] text;

#undef p_area
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    //-------------------------------------------------------------------------
    vlc_mutex_unlock( &p_intf->change_lock );

    return( VLC_TRUE );
}
