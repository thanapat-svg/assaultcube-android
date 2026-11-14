// touch.cpp: on-screen touch controls (Call of Duty Mobile style) for the Android port
//
// Gameplay: floating joystick on the left, drag anywhere on the right to aim,
// big fire button that also aims while held, plus jump / crouch / reload / grenade /
// knife / weapon swap / scope / scoreboard / menu buttons.
// Menus: tap an item to select/activate it, drag to scroll, and nav buttons on the right.
//
// All actions go through the normal key binds (keypress), so rebinding in config still works.

#include "cube.h"

#ifdef AC_TOUCH

VARP(touchcontrols, 0, 1, 1);           // show and use the on-screen controls
FVARP(touchsens, 0.05f, 1.0f, 10.0f);   // aim sensitivity for touch drags
VARP(touchsize, 50, 100, 200);          // button size in percent
VARP(touchalpha, 10, 60, 100);          // button opacity in percent
VARP(touchleftfire, 0, 1, 1);           // second fire button above the joystick
VARP(touchcrouchtoggle, 0, 1, 1);       // crouch button toggles instead of hold
VAR(touchdebug, 0, 0, 1);               // log touch handling to the console

extern keym *findbind(const char *key);
extern gmenu *curmenu;
extern bool saycommandon;
extern bool editmode;

enum { BTN_HOLD = 0, BTN_TOGGLE, BTN_TAP };
enum { ICON_FIRE = 0, ICON_JUMP, ICON_CROUCH, ICON_RELOAD, ICON_GRENADE, ICON_KNIFE, ICON_SWAP, ICON_SCOPE,
       ICON_SCORE, ICON_MENU, ICON_UP, ICON_DOWN, ICON_LEFT, ICON_RIGHT, ICON_OK, ICON_BACK };

struct tbutton
{
    const char *label, *key;
    int icon, mode;
    bool menu;          // shown in menu mode instead of gameplay mode
    bool look;          // dragging the finger on this button also aims
    float x, y, r;      // pixel position and radius, set by layout()
    SDL_FingerID finger;
    bool held, on, visible;
    int pressmillis;
};

static tbutton buttons[] =
{
    // gameplay
    { "FIRE",   "MOUSE1", ICON_FIRE,    BTN_HOLD,   false, true  },
    { "FIRE",   "MOUSE1", ICON_FIRE,    BTN_HOLD,   false, true  },   // left fire
    { "JUMP",   "SPACE",  ICON_JUMP,    BTN_HOLD,   false, false },
    { "CROUCH", "LSHIFT", ICON_CROUCH,  BTN_TOGGLE, false, false },
    { "RELOAD", "R",      ICON_RELOAD,  BTN_TAP,    false, false },
    { "NADE",   "G",      ICON_GRENADE, BTN_HOLD,   false, false },
    { "KNIFE",  "MOUSE3", ICON_KNIFE,   BTN_HOLD,   false, false },
    { "SWAP",   "Q",      ICON_SWAP,    BTN_TAP,    false, false },
    { "SCOPE",  "MOUSE2", ICON_SCOPE,   BTN_TOGGLE, false, true  },
    { "SCORE",  "TAB",    ICON_SCORE,   BTN_HOLD,   false, false },
    { "MENU",   "ESCAPE", ICON_MENU,    BTN_TAP,    false, false },
    // menus
    { "",       "UP",     ICON_UP,      BTN_TAP,    true,  false },
    { "",       "DOWN",   ICON_DOWN,    BTN_TAP,    true,  false },
    { "",       "LEFT",   ICON_LEFT,    BTN_TAP,    true,  false },
    { "",       "RIGHT",  ICON_RIGHT,   BTN_TAP,    true,  false },
    { "OK",     "RETURN", ICON_OK,      BTN_TAP,    true,  false },
    { "BACK",   "ESCAPE", ICON_BACK,    BTN_TAP,    true,  false },
};
enum { B_FIRE = 0, B_LFIRE, B_JUMP, B_CROUCH, B_RELOAD, B_NADE, B_KNIFE, B_SWAP, B_SCOPE, B_SCORE, B_MENU,
       B_UP, B_DOWN, B_LEFT, B_RIGHT, B_OK, B_BACK, NUMBUTTONS };

// joystick
static struct
{
    SDL_FingerID finger;
    bool active;
    float ox, oy, x, y, r;      // origin, knob, radius (pixels)
    bool keys[4];               // forward, backward, left, right
} joy = { 0, false };
static const char *joykeys[4] = { "W", "S", "A", "D" };

// free-look fingers on the right side
static struct { SDL_FingerID finger; bool active; } look = { 0, false };
static float lookremx = 0, lookremy = 0;

// menu interaction
struct touchmenuitem { int idx, x, y, w, h; };
static vector<touchmenuitem> menuitems;
static void *menuitemsowner = NULL;
static struct { SDL_FingerID finger; bool active; float sx, sy, ly; bool moved; } menudrag = { 0, false };

static int lastmode = -1;       // 0 = gameplay, 1 = menu, 2 = text entry / off
static float unit = 1;          // layout unit in pixels

void touch_menureset(void *m)
{
    menuitems.setsize(0);
    if(touchdebug && m != menuitemsowner) conoutf("menureset %p", m);
    menuitemsowner = m;
}

void touch_menuitem(int idx, int x, int y, int w, int h)
{
    touchmenuitem &t = menuitems.add();
    t.idx = idx; t.x = x; t.y = y; t.w = w; t.h = h;
}

static void sendkey(const char *name, bool down)
{
    keym *k = findbind(name);
    if(k) keypress(k->code, 0, down);
}

static void tapkey(const char *name)
{
    sendkey(name, true);
    sendkey(name, false);
}

static bool menuactive() { return curmenu && curmenu->allowinput; }

static int currentmode()
{
    if(!touchcontrols || saycommandon) return 2;
    return menuactive() ? 1 : 0;
}

static void releaseall()
{
    loopi(NUMBUTTONS)
    {
        tbutton &b = buttons[i];
        if((b.held || b.on) && b.mode != BTN_TAP) sendkey(b.key, false);
        b.held = b.on = false;
    }
    loopi(4) if(joy.keys[i]) { sendkey(joykeys[i], false); joy.keys[i] = false; }
    joy.active = false;
    look.active = false;
    menudrag.active = false;
}

static void layout()
{
    float w = screenw, h = screenh;
    unit = min(w, h) / 10.0f * touchsize / 100.0f;
    float u = unit;
    #define PLACE(b, px, py, pr) { buttons[b].x = px; buttons[b].y = py; buttons[b].r = pr; }
    PLACE(B_FIRE,   w - 2.7f*u, h - 3.0f*u, 1.25f*u);
    PLACE(B_LFIRE,  2.8f*u,     h - 6.6f*u, 0.75f*u);
    PLACE(B_JUMP,   w - 1.05f*u, h - 4.9f*u, 0.75f*u);
    PLACE(B_CROUCH, w - 1.05f*u, h - 1.3f*u, 0.75f*u);
    PLACE(B_RELOAD, w - 4.9f*u, h - 1.1f*u, 0.6f*u);
    PLACE(B_NADE,   w - 5.1f*u, h - 2.6f*u, 0.6f*u);
    PLACE(B_KNIFE,  w - 4.6f*u, h - 4.0f*u, 0.55f*u);
    PLACE(B_SWAP,   w - 3.2f*u, h - 5.4f*u, 0.6f*u);
    PLACE(B_SCOPE,  w - 1.4f*u, h - 6.6f*u, 0.7f*u);
    PLACE(B_SCORE,  1.95f*u,    0.75f*u,    0.5f*u);
    PLACE(B_MENU,   0.75f*u,    0.75f*u,    0.5f*u);
    float cx = w - 2.4f*u, cy = h*0.5f;
    PLACE(B_UP,     cx,         cy - 1.5f*u, 0.65f*u);
    PLACE(B_DOWN,   cx,         cy + 1.5f*u, 0.65f*u);
    PLACE(B_LEFT,   cx - 1.5f*u, cy,         0.65f*u);
    PLACE(B_RIGHT,  cx + 1.5f*u, cy,         0.65f*u);
    PLACE(B_OK,     w - 1.3f*u, h - 1.3f*u, 0.8f*u);
    PLACE(B_BACK,   w - 3.3f*u, h - 1.3f*u, 0.8f*u);
    #undef PLACE
    joy.r = 1.4f*u;

    bool sniper = player1->weaponsel && player1->weaponsel->type == GUN_SNIPER;
    loopi(NUMBUTTONS)
    {
        tbutton &b = buttons[i];
        b.visible = true;
    }
    if(!touchleftfire) buttons[B_LFIRE].visible = false;
    if(!sniper) buttons[B_SCOPE].visible = false;
    if(editmode) buttons[B_NADE].visible = buttons[B_KNIFE].visible = buttons[B_SWAP].visible = false;
}

static tbutton *hitbutton(float x, float y, bool menumode)
{
    tbutton *best = NULL;
    float bestd = 1e16f;
    loopi(NUMBUTTONS)
    {
        tbutton &b = buttons[i];
        if(!b.visible || b.menu != menumode) continue;
        float dx = x - b.x, dy = y - b.y, d = dx*dx + dy*dy, rr = b.r * 1.15f;
        if(d <= rr*rr && d < bestd) { best = &b; bestd = d; }
    }
    return best;
}

static void dolook(float dx, float dy)
{
    // normalise to a 1080p tall screen so the feel is the same on every phone
    float s = 1.25f * touchsens * 1080.0f / max(screenh, 1);
    lookremx += dx * s;
    lookremy += dy * s;
    int ix = int(lookremx), iy = int(lookremy);
    lookremx -= ix; lookremy -= iy;
    if(ix || iy) mousemove(ix, iy);
}

static void updatejoy()
{
    bool want[4] = { false, false, false, false };
    if(joy.active)
    {
        float dx = (joy.x - joy.ox) / joy.r, dy = (joy.y - joy.oy) / joy.r;
        const float t = 0.35f;
        want[0] = dy < -t; want[1] = dy > t; want[2] = dx < -t; want[3] = dx > t;
    }
    loopi(4) if(want[i] != joy.keys[i])
    {
        joy.keys[i] = want[i];
        sendkey(joykeys[i], want[i]);
    }
}

static void pressbutton(tbutton &b, SDL_FingerID f)
{
    b.finger = f;
    b.held = true;
    b.pressmillis = totalmillis;
    switch(b.mode)
    {
        case BTN_HOLD: sendkey(b.key, true); break;
        case BTN_TAP: tapkey(b.key); break;
        case BTN_TOGGLE:
            if(&b == &buttons[B_CROUCH] && !touchcrouchtoggle) { sendkey(b.key, true); break; }
            b.on = !b.on;
            sendkey(b.key, b.on);
            break;
    }
}

static void releasebutton(tbutton &b)
{
    if(!b.held) return;
    b.held = false;
    if(b.mode == BTN_HOLD || (&b == &buttons[B_CROUCH] && !touchcrouchtoggle)) sendkey(b.key, false);
}

static void menutap(float px, float py)
{
    if(touchdebug) conoutf("menutap: curmenu %s owner %p", curmenu ? curmenu->name : "-", menuitemsowner);
    if(!curmenu || curmenu != menuitemsowner) return;
    float vx = px * 2 * VIRTW / max(screenw, 1), vy = py * 2 * VIRTH / max(screenh, 1);
    if(touchdebug) { conoutf("menutap %.0f,%.0f items %d", vx, vy, menuitems.length()); loopv(menuitems) conoutf("  item %d: %d,%d %dx%d", menuitems[i].idx, menuitems[i].x, menuitems[i].y, menuitems[i].w, menuitems[i].h); }
    loopv(menuitems)
    {
        touchmenuitem &t = menuitems[i];
        if(vy < t.y || vy >= t.y + t.h || vx < t.x - FONTH || vx > t.x + t.w + FONTH) continue;
        if(!curmenu->items.inrange(t.idx)) return;
        mitem *m = curmenu->items[t.idx];
        if(m->greyedout) return;
        bool wasselected = curmenu->menusel == t.idx;
        menuselect(curmenu, t.idx);
        bool selectonly = m->mitemtype == mitem::TYPE_SLIDER || m->mitemtype == mitem::TYPE_TEXTINPUT || m->mitemtype == mitem::TYPE_KEYINPUT;
        if(!selectonly || wasselected) tapkey("RETURN");
        return;
    }
}

bool touch_event(SDL_Event &e)
{
    if(e.type != SDL_FINGERDOWN && e.type != SDL_FINGERUP && e.type != SDL_FINGERMOTION) return false;
    int mode = currentmode();
    if(mode != lastmode) { releaseall(); lastmode = mode; }
    if(mode == 2) return true;
    layout();

    float px = e.tfinger.x * screenw, py = e.tfinger.y * screenh;
    if(touchdebug) conoutf("touch: type %d finger %d at %.0f,%.0f mode %d", e.type, int(e.tfinger.fingerId), px, py, mode);
    float pdx = e.tfinger.dx * screenw, pdy = e.tfinger.dy * screenh;
    SDL_FingerID f = e.tfinger.fingerId;

    if(mode == 1) // menus
    {
        switch(e.type)
        {
            case SDL_FINGERDOWN:
            {
                tbutton *b = hitbutton(px, py, true);
                if(b) { pressbutton(*b, f); break; }
                if(!menudrag.active)
                {
                    menudrag.active = true; menudrag.finger = f;
                    menudrag.sx = px; menudrag.sy = py; menudrag.ly = py; menudrag.moved = false;
                }
                break;
            }
            case SDL_FINGERMOTION:
                if(menudrag.active && menudrag.finger == f)
                {
                    if(fabs(py - menudrag.sy) > unit*0.4f || fabs(px - menudrag.sx) > unit*0.4f) menudrag.moved = true;
                    float step = max(unit * 0.55f, 1.0f);
                    while(py - menudrag.ly > step) { tapkey("UP"); menudrag.ly += step; }
                    while(menudrag.ly - py > step) { tapkey("DOWN"); menudrag.ly -= step; }
                }
                break;
            case SDL_FINGERUP:
                loopi(NUMBUTTONS) if(buttons[i].held && buttons[i].finger == f) releasebutton(buttons[i]);
                if(touchdebug) conoutf("menuup: drag %d finger %d moved %d", menudrag.active, int(menudrag.finger), menudrag.moved);
                if(menudrag.active && menudrag.finger == f)
                {
                    menudrag.active = false;
                    if(!menudrag.moved) menutap(px, py);
                }
                break;
        }
        return true;
    }

    // gameplay
    switch(e.type)
    {
        case SDL_FINGERDOWN:
        {
            tbutton *b = hitbutton(px, py, false);
            if(b) { pressbutton(*b, f); break; }
            if(px < screenw * 0.42f && py > screenh * 0.3f)
            {
                if(!joy.active)
                {
                    joy.active = true; joy.finger = f;
                    joy.ox = joy.x = px; joy.oy = joy.y = py;
                    updatejoy();
                }
            }
            else if(!look.active) { look.active = true; look.finger = f; }
            break;
        }
        case SDL_FINGERMOTION:
            if(joy.active && joy.finger == f)
            {
                joy.x = px; joy.y = py;
                float dx = joy.x - joy.ox, dy = joy.y - joy.oy, d = sqrtf(dx*dx + dy*dy);
                if(d > joy.r * 1.6f) { joy.ox = px - dx / d * joy.r * 1.6f; joy.oy = py - dy / d * joy.r * 1.6f; } // drag the base along
                updatejoy();
                break;
            }
            if(look.active && look.finger == f) { dolook(pdx, pdy); break; }
            loopi(NUMBUTTONS)
            {
                tbutton &b = buttons[i];
                if(b.held && b.finger == f && b.look) { dolook(pdx, pdy); break; }
            }
            break;
        case SDL_FINGERUP:
            if(joy.active && joy.finger == f) { joy.active = false; updatejoy(); }
            if(look.active && look.finger == f) look.active = false;
            loopi(NUMBUTTONS) if(buttons[i].held && buttons[i].finger == f) releasebutton(buttons[i]);
            break;
    }
    return true;
}

// touchsim <0 down|1 move|2 up> <x 0..1> <y 0..1> [finger]: inject a touch event (for testing on desktop)
static void touchsim(int *type, float *x, float *y, int *finger)
{
    static float lastx[10], lasty[10];
    int fi = clamp(*finger, 0, 9);
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = *type == 0 ? SDL_FINGERDOWN : (*type == 1 ? SDL_FINGERMOTION : SDL_FINGERUP);
    e.tfinger.fingerId = fi + 1;
    e.tfinger.x = *x; e.tfinger.y = *y;
    e.tfinger.dx = *type == 0 ? 0 : *x - lastx[fi];
    e.tfinger.dy = *type == 0 ? 0 : *y - lasty[fi];
    lastx[fi] = *x; lasty[fi] = *y;
    SDL_PushEvent(&e);
}
COMMAND(touchsim, "iffi");

// ---------------------------------------------------------------- rendering

static void circle(float x, float y, float r, bool filled, int segs = 40)
{
    glBegin(filled ? GL_TRIANGLE_FAN : GL_LINE_LOOP);
    if(filled) glVertex2f(x, y);
    loopi(segs + (filled ? 1 : 0))
    {
        float a = 2 * PI * i / segs;
        glVertex2f(x + cosf(a) * r, y + sinf(a) * r);
    }
    glEnd();
}

static void line(float x1, float y1, float x2, float y2)
{
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

static void tri(float x1, float y1, float x2, float y2, float x3, float y3)
{
    glBegin(GL_TRIANGLES);
    glVertex2f(x1, y1); glVertex2f(x2, y2); glVertex2f(x3, y3);
    glEnd();
}

static void quad(float x1, float y1, float x2, float y2)
{
    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(x1, y1); glVertex2f(x2, y1); glVertex2f(x1, y2); glVertex2f(x2, y2);
    glEnd();
}

static void arc(float x, float y, float r, float a1, float a2, int segs = 24)
{
    glBegin(GL_LINE_STRIP);
    loopi(segs + 1)
    {
        float a = a1 + (a2 - a1) * i / segs;
        glVertex2f(x + cosf(a) * r, y + sinf(a) * r);
    }
    glEnd();
}

static void drawicon(int icon, float x, float y, float s)
{
    // s = icon half size
    switch(icon)
    {
        case ICON_FIRE:
            circle(x, y, s * 0.55f, false);
            line(x - s, y, x - s * 0.25f, y); line(x + s * 0.25f, y, x + s, y);
            line(x, y - s, x, y - s * 0.25f); line(x, y + s * 0.25f, x, y + s);
            circle(x, y, s * 0.1f, true, 12);
            break;
        case ICON_JUMP:
            tri(x, y - s * 0.8f, x - s * 0.7f, y, x + s * 0.7f, y);
            quad(x - s * 0.25f, y, x + s * 0.25f, y + s * 0.7f);
            break;
        case ICON_CROUCH:
            tri(x, y + s * 0.5f, x - s * 0.7f, y - s * 0.2f, x + s * 0.7f, y - s * 0.2f);
            quad(x - s * 0.25f, y - s * 0.8f, x + s * 0.25f, y - s * 0.2f);
            quad(x - s * 0.8f, y + s * 0.65f, x + s * 0.8f, y + s * 0.85f);
            break;
        case ICON_RELOAD:
            arc(x, y, s * 0.6f, -PI * 0.35f, PI * 1.35f);
            tri(x + s * 0.32f, y - s * 0.95f, x + s * 0.32f, y - s * 0.1f, x + s * 0.9f, y - s * 0.52f);
            break;
        case ICON_GRENADE:
            circle(x, y + s * 0.2f, s * 0.55f, true, 20);
            quad(x - s * 0.2f, y - s * 0.6f, x + s * 0.2f, y - s * 0.3f);
            arc(x + s * 0.35f, y - s * 0.6f, s * 0.25f, PI, PI * 2.2f, 10);
            break;
        case ICON_KNIFE:
            tri(x - s * 0.8f, y + s * 0.8f, x + s * 0.8f, y - s * 0.8f, x - s * 0.3f, y + s * 0.1f);
            quad(x - s * 0.95f, y + s * 0.55f, x - s * 0.55f, y + s * 0.95f);
            break;
        case ICON_SWAP:
            tri(x + s * 0.9f, y - s * 0.4f, x + s * 0.3f, y - s * 0.85f, x + s * 0.3f, y + s * 0.05f);
            quad(x - s * 0.8f, y - s * 0.55f, x + s * 0.3f, y - s * 0.25f);
            tri(x - s * 0.9f, y + s * 0.4f, x - s * 0.3f, y - s * 0.05f, x - s * 0.3f, y + s * 0.85f);
            quad(x - s * 0.3f, y + s * 0.25f, x + s * 0.8f, y + s * 0.55f);
            break;
        case ICON_SCOPE:
            circle(x, y, s * 0.8f, false);
            circle(x, y, s * 0.45f, false);
            line(x - s, y, x + s, y); line(x, y - s, x, y + s);
            break;
        case ICON_SCORE:
            loopi(3) quad(x - s * 0.7f, y - s * 0.65f + i * s * 0.5f, x + s * 0.7f, y - s * 0.4f + i * s * 0.5f);
            break;
        case ICON_MENU:
            quad(x - s * 0.5f, y - s * 0.6f, x - s * 0.15f, y + s * 0.6f);
            quad(x + s * 0.15f, y - s * 0.6f, x + s * 0.5f, y + s * 0.6f);
            break;
        case ICON_UP:    tri(x, y - s * 0.6f, x - s * 0.7f, y + s * 0.4f, x + s * 0.7f, y + s * 0.4f); break;
        case ICON_DOWN:  tri(x, y + s * 0.6f, x - s * 0.7f, y - s * 0.4f, x + s * 0.7f, y - s * 0.4f); break;
        case ICON_LEFT:  tri(x - s * 0.6f, y, x + s * 0.4f, y - s * 0.7f, x + s * 0.4f, y + s * 0.7f); break;
        case ICON_RIGHT: tri(x + s * 0.6f, y, x - s * 0.4f, y - s * 0.7f, x - s * 0.4f, y + s * 0.7f); break;
        case ICON_OK:
            glBegin(GL_LINE_STRIP);
            glVertex2f(x - s * 0.6f, y); glVertex2f(x - s * 0.15f, y + s * 0.5f); glVertex2f(x + s * 0.65f, y - s * 0.5f);
            glEnd();
            break;
        case ICON_BACK:
            line(x - s * 0.5f, y - s * 0.5f, x + s * 0.5f, y + s * 0.5f);
            line(x + s * 0.5f, y - s * 0.5f, x - s * 0.5f, y + s * 0.5f);
            break;
    }
}

static void drawlabel(const char *s, float x, float y, float height, float alpha)
{
    if(!s || !*s) return;
    float scale = height / FONTH;
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(scale, scale, 1);
    glEnable(GL_TEXTURE_2D);
    draw_text(s, -text_width(s) / 2, -FONTH / 2, 255, 255, 255, int(alpha * 255));
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void touch_render()
{
    int mode = currentmode();
    if(mode == 2) return;
    layout();
    float a = touchalpha / 100.0f;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, screenw, screenh, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glLineWidth(max(2.0f, unit * 0.06f));

    bool menumode = mode == 1;
    loopi(NUMBUTTONS)
    {
        tbutton &b = buttons[i];
        if(!b.visible || b.menu != menumode) continue;
        bool lit = b.held || b.on;
        bool fire = i == B_FIRE || i == B_LFIRE;
        if(fire) glColor4f(lit ? 0.95f : 0.75f, 0.2f, 0.15f, a * (lit ? 0.75f : 0.45f));
        else glColor4f(0.05f, 0.05f, 0.08f, a * (lit ? 0.8f : 0.45f));
        circle(b.x, b.y, b.r, true);
        glColor4f(1, 1, 1, a * (lit ? 1.0f : 0.7f));
        circle(b.x, b.y, b.r, false);
        bool haslabel = *b.label && b.r > unit * 0.55f;
        float iy = haslabel ? b.y - b.r * 0.15f : b.y;
        glColor4f(1, 1, 1, a * (lit ? 1.0f : 0.85f));
        drawicon(b.icon, b.x, iy, b.r * (haslabel ? 0.45f : 0.55f));
        if(haslabel) drawlabel(b.label, b.x, b.y + b.r * 0.55f, b.r * 0.28f, a);
        glDisable(GL_TEXTURE_2D);
    }

    if(!menumode)
    {
        float jx = joy.active ? joy.ox : 2.8f * unit, jy = joy.active ? joy.oy : screenh - 3.2f * unit;
        glColor4f(0.05f, 0.05f, 0.08f, a * 0.35f);
        circle(jx, jy, joy.r, true);
        glColor4f(1, 1, 1, a * 0.6f);
        circle(jx, jy, joy.r, false);
        float kx = jx, ky = jy;
        if(joy.active)
        {
            float dx = joy.x - joy.ox, dy = joy.y - joy.oy, d = sqrtf(dx*dx + dy*dy);
            if(d > joy.r) { dx *= joy.r / d; dy *= joy.r / d; }
            kx += dx; ky += dy;
        }
        glColor4f(1, 1, 1, a * (joy.active ? 0.8f : 0.5f));
        circle(kx, ky, joy.r * 0.42f, true);
    }

    glLineWidth(1);
    glColor4f(1, 1, 1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

#endif // AC_TOUCH
