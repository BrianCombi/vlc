/*****************************************************************************
 * x11_run.cpp:
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: x11_run.cpp,v 1.2 2003/05/12 17:33:19 gbazin Exp $
 *
 * Authors: Cyril Deguet     <asmax@videolan.org>
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

#ifdef X11_SKINS

//--- X11 -------------------------------------------------------------------
#include <X11/Xlib.h>

//--- WWWINDOWS -------------------------------------------------------------
#include <wx/wx.h>

//--- VLC -------------------------------------------------------------------
#include <vlc/intf.h>

//--- SKIN ------------------------------------------------------------------
#include "../os_api.h"
#include "../src/event.h"
#include "../os_event.h"
#include "../src/banks.h"
#include "../src/window.h"
#include "../os_window.h"
#include "../src/theme.h"
#include "../os_theme.h"
#include "../src/skin_common.h"
#include "../src/vlcproc.h"
#include "../../wxwindows/wxwindows.h"

// include the icon graphic
#include "share/vlc32x32.xpm"

//---------------------------------------------------------------------------
class CallBackObjects
{
    public:
        VlcProc *Proc;
};

//---------------------------------------------------------------------------
// Specific method
//---------------------------------------------------------------------------
bool IsVLCEvent( unsigned int msg );
int  SkinManage( intf_thread_t *p_intf );


//---------------------------------------------------------------------------
// Local classes declarations.
//---------------------------------------------------------------------------
class Instance: public wxApp
{
public:
    Instance();
    Instance( intf_thread_t *_p_intf, CallBackObjects *callback );

    bool OnInit();
    OpenDialog *open;

private:
    intf_thread_t *p_intf;
    CallBackObjects *callbackobj;
};


//---------------------------------------------------------------------------
// GTK2 interface
//---------------------------------------------------------------------------
/*void GTK2Proc( GdkEvent *event, gpointer data )
{
    // Get objects from data
    CallBackObjects *obj = (CallBackObjects *)data;
    VlcProc *proc        = obj->Proc;

    // Get pointer to thread info
    intf_thread_t *p_intf = proc->GetpIntf();

    // Variables
    unsigned int msg;
    Event *evt;
    list<SkinWindow *>::const_iterator win;
    GdkWindow *gwnd = ((GdkEventAny *)event)->window;

    // Create event to dispatch in windows
    // Skin event
    if( event->type == GDK_CLIENT_EVENT )
    {
        msg = ( (GdkEventClient *)event )->data.l[0];
        evt = (Event *)new OSEvent( p_intf, 
            ((GdkEventAny *)event)->window,
            msg,
            ( (GdkEventClient *)event )->data.l[1],
            ( (GdkEventClient *)event )->data.l[2] );
    }
    // System event
    else
    {
        msg = event->type;
        evt = (Event *)new OSEvent( p_intf,
            ((GdkEventAny *)event)->window, msg, 0, (long)event );
    }

    // Process keyboard shortcuts
    if( msg == GDK_KEY_PRESS )
    {
        int KeyModifier = 0;
        // If key is ALT
        if( ((GdkEventKey *)event)->state & GDK_MOD1_MASK )
        {
            KeyModifier = 1;
        }
        // If key is CTRL
        else if( ((GdkEventKey *)event)->state & GDK_CONTROL_MASK )
        {
            KeyModifier = 2;
        }
        int key = ((GdkEventKey *)event)->keyval;
        // Translate into lower case
        if( key >= 'a' && key <= 'z' )
        {
            key -= ('a' - 'A');
        }
        if( KeyModifier > 0 )
            p_intf->p_sys->p_theme->EvtBank->TestShortcut( key , KeyModifier );
    }

    // Send event
    else if( IsVLCEvent( msg ) )
    {
        if( !proc->EventProc( evt ) )
        {
            wxExit();
            return;      // Exit VLC !
        }
    }
    else if( gwnd == NULL )
    {
        for( win = p_intf->p_sys->p_theme->WindowList.begin();
             win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
        {
            (*win)->ProcessEvent( evt );
        }
    }
    else
    {
        // Find window matching with gwnd
        for( win = p_intf->p_sys->p_theme->WindowList.begin();
             win != p_intf->p_sys->p_theme->WindowList.end(); win++ )
        {
            // If it is the correct window
            if( gwnd == ( (GTK2Window *)(*win) )->GetHandle() )
            {
                // Send event and check if processed
                if( (*win)->ProcessEvent( evt ) )
                {
                    delete (OSEvent *)evt;
                    return;
                }
                else
                {
                    break;
                }
            }
        }
    }

    evt->DestructParameters();
    delete (OSEvent *)evt;

    // Check if vlc is closing
    proc->IsClosing();

    gtk_main_do_event( event )
}*/
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// REFRESH TIMER CALLBACK
//---------------------------------------------------------------------------
/*gboolean RefreshTimer( gpointer data )
{
    intf_thread_t *p_intf = (intf_thread_t *)data;
    SkinManage( p_intf );
    return true;
}*/
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// Implementation of Instance class
//---------------------------------------------------------------------------
Instance::Instance( )
{
}

Instance::Instance( intf_thread_t *_p_intf, CallBackObjects *callback )
{
    // Initialization
    p_intf = _p_intf;
    callbackobj = callback;
}

IMPLEMENT_APP_NO_MAIN(Instance)

bool Instance::OnInit()
{
    // Set event callback. Yes, it's a big hack ;)
//    gdk_event_handler_set( GTK2Proc, (gpointer)callbackobj, NULL );

    p_intf->p_sys->p_icon = new wxIcon( vlc_xpm );
    p_intf->p_sys->OpenDlg = new OpenDialog( p_intf, NULL, FILE_ACCESS );
    p_intf->p_sys->MessagesDlg = new Messages( p_intf, NULL );
    p_intf->p_sys->SoutDlg = new SoutDialog( p_intf, NULL );
    p_intf->p_sys->PrefsDlg = new PrefsDialog( p_intf, NULL );
    p_intf->p_sys->InfoDlg = new FileInfo( p_intf, NULL );
    
    // Add timer
//    g_timeout_add( 200, (GSourceFunc)RefreshTimer, (gpointer)p_intf );

    return TRUE;
}



//---------------------------------------------------------------------------
// GTK2 interface
//---------------------------------------------------------------------------
void OSRun( intf_thread_t *p_intf )
{
    static char  *p_args[] = { "" };

    // Create VLC event object processing
    CallBackObjects *callbackobj = new CallBackObjects();
    callbackobj->Proc = new VlcProc( p_intf );

/*    wxTheApp = new Instance( p_intf, callbackobj );

    wxEntry( 1, p_args );*/

    Display *display = ((OSTheme *)p_intf->p_sys->p_theme)->GetDisplay();
    
    // Main event loop
    while( 1 )
    {
        XEvent *event;
        XNextEvent( display, event );
        fprintf(stderr,"event %d\n", event->type);
    }
    
    delete callbackobj;
}
//---------------------------------------------------------------------------
bool IsVLCEvent( unsigned int msg )
{
    return( msg > VLC_MESSAGE && msg < VLC_WINDOW );
}
//---------------------------------------------------------------------------

#endif
