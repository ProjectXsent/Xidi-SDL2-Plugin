#include <Xidi/Plugin.h>

#include "SDL2Backend.h"

static Xidi::IPlugin* plugin = new XidiSDL2Plugin::SDL2Backend();

extern "C" int __fastcall XidiPluginGetCount(void)
{
    return 1;
}

extern "C" Xidi::IPlugin* __fastcall XidiPluginGetInterface(int index)
{
    return plugin;
}