package com.thanapat.assaultcube;

import org.libsdl.app.SDLActivity;

public class ACActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
