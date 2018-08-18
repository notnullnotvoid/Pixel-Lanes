//TODO: fully fix ugly dark outlines on scaled textures (this is probably caused
//      by either gamma-incorrect blending or premultiplied alpha confusion)

//TODO: change OS cursor based on hover? or maybe draw our own cursor?
//TODO: draw icons for window controls
//TODO: draw icons for application launchers
//TODO: launch applications with function keys
//TODO: editor UI
//TODO: register listings
//TODO: text editing and selection
//TODO: compilation error handling
//TODO: code execution
//TODO: pickable desktop background
//TODO: 4 bit color post-processing shader
//  - setup offscreen rendering
//  - quantization shader
//  - dithering?

//editor feature list
//TODO: deleting text (via DELETE key)
//TODO: advanced arrow key navigation (w/modifiers)
//TODO: special key navigation (home/end/pageup/pagedown/etc.)
//TODO: cut+copy+paste
//TODO: auto-formatting of some kind???

//TODO: editor scroll buttons behaviors
//TODO: editor respond to clicks in empty scroll bar area

//(2*3*5*7*11)*12 = 27720 [1-12]
//(2*3*5*7)*12 = 2520 [1-10]
//(2*3*5*7)*4 = 840 [1-8]
//(2*3*5*7)*2 = 420 [1-7]
//(2*3*5)*2 = 60 [1-6]
//(2*3)*2 = 12 [1-4]

#include <stdio.h>

//NOTE(miles): I'm not sure if this is right! <SDL.h> alone doesn't compile for me on OSX,
//but I think that's what you're supposed to use, at least on other platforms.
//let me know if you have problems with this include
#ifdef __APPLE__
# include <SDL2/SDL.h>
#else
# include <SDL.h>
#endif

#include "types.hpp"
#include "system.hpp"
#include "Imm.hpp"
#include "ArrayList.hpp"
#include "parse.hpp"
#include "glutils.hpp"

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

u64 applicationStartupTimeValue;

double get_time() {
    u64 currentTimeValue = SDL_GetPerformanceCounter();
    u64 diffTimeValue = currentTimeValue - applicationStartupTimeValue;
    double elapsedSeconds = (double)diffTimeValue / (double)SDL_GetPerformanceFrequency();
    return elapsedSeconds;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

inline Color color16(u8 r, u8 g, u8 b, u8 a) {
    return { (u8)(r * 17), (u8)(g * 17), (u8)(b * 17), (u8)(a * 17) };
}

inline Color color16(u8 r, u8 g, u8 b) {
    return color16(r, g, b, 15);
}

inline Color color16(u8 v) {
    return color16(v, v, v, 15);
}

const int outputWidth = 32;
const int outputHeight = 32;

const float uiRadius = 2;
const float windowBorder = 12;
const float windowButton = 24;
const float windowCorner = windowBorder + windowButton;
const float windowMenu = 48;

const Color uiBlack = color16(0);
const Color uiDark = color16(3);
const Color uiGrey = color16(7);
const Color uiLight = color16(11);
const Color uiWhite = color16(15);

const float visWidth = outputWidth;
const float visHeight = outputHeight;
const float visScale = 4;

struct Rect {
    float x, y, w, h;
};

//encode offset of interactive window areas relative to window edges
//positive = from top/left, negative = from bottom/right
const Rect windowTop = { windowCorner, 0, -windowCorner, windowBorder };
const Rect windowLeft = { 0, windowCorner, windowBorder, -windowCorner };
const Rect windowRight = { -windowBorder, windowCorner, -0., -windowCorner };
const Rect windowBottom = { windowCorner, -windowBorder, -windowCorner, -0. };

const Rect windowTopLeft = { 0, 0, windowCorner, windowCorner };
const Rect windowTopRight = { -windowCorner, 0, -0., windowCorner };
const Rect windowBottomLeft = { 0, -windowCorner, windowCorner, -0. };
const Rect windowBottomRight = { -windowCorner, -windowCorner, -0., -0. };

const Rect windowClose =
    { windowBorder, windowBorder, windowCorner, windowCorner };
const Rect windowMinimize =
    { windowCorner, windowBorder, windowCorner + windowButton, windowCorner };
const Rect windowMaximize =
    { windowCorner + windowButton, windowBorder, windowCorner + windowButton * 2, windowCorner };
const Rect windowTitle =
    { windowCorner + windowButton * 2, windowBorder, -windowBorder, windowCorner };

const Rect windowBar =
    { windowBorder, windowCorner, -windowBorder, windowCorner + windowMenu };
const Rect windowContent =
    { windowBorder, windowCorner + windowMenu, -windowBorder, -windowBorder };
const Rect windowArea =
    { windowBorder, windowCorner, -windowBorder, -windowBorder };
const Rect windowInside =
    { windowBorder, windowBorder, -windowBorder, -windowBorder };
const Rect windowEverything =
    { 0, 0, -0., -0. };

struct GameState {
    Imm imm;
    Font prop, mono;
};

enum WindowTag {
    WINDOW_NONE, WINDOW_EDITOR, WINDOW_MAIL, WINDOW_OPTIONS,
};

struct Window {
    void (*draw) (GameState * game, Window * window);
    void (*text) (GameState * game, Window * window, uint codepoint);
    void (*key) (GameState * game, Window * window, int keycode, int mods, bool down, bool repeat);
    void (*mouse) (GameState * game, Window * window,
        float mx, float my, bool down, bool lb, bool rb);
    void (*drag) (GameState * game, Window * window,
        float mx, float my, float dmx, float dmy, int buttonMask);
    void (*scroll) (GameState * game, Window * window, float mx, float my, float dx, float dy);
    void * appData; //data specific to the application this window represents
    const char * title;
    WindowTag tag;
    float x, y, w, h;
    bool open, dragging, closing;
    bool top, left, right, bottom;
};

Rect get_absolute_bounds(Window * window, Rect relativeBounds) {
    Window * w = window;
    Rect r = relativeBounds;
    float x1 = signbit(r.x)? w->x + w->w + r.x : w->x + r.x;
    float y1 = signbit(r.y)? w->y + w->h + r.y : w->y + r.y;
    float x2 = signbit(r.w)? w->x + w->w + r.w : w->x + r.w;
    float y2 = signbit(r.h)? w->y + w->h + r.h : w->y + r.h;
    return { x1, y1, x2 - x1, y2 - y1 };
}

bool window_hover(Window * w, const Rect r, float mx, float my) {
    Rect b = get_absolute_bounds(w, r);
    return mx > b.x && mx < b.x + b.w && my > b.y && my < b.y + b.h;
}

void ui_colors(Color * light, Color * mid, Color * dark, bool active, bool inverted) {
    if (active) {
        *light = color16(11);
        *mid   = color16( 7);
        *dark  = color16( 3);
    } else {
        *light = color16(11);
        *mid   = color16( 9);
        *dark  = color16( 7);
    }

    if (inverted) {
        Color temp = *light;
        *light = *dark;
        *dark = temp;
    }
}

void ui_rect(Imm * imm, float wx, float wy, float ww, float wh, float r,
        bool active, bool inverted)
{
    Color light, mid, dark;
    ui_colors(&light, &mid, &dark, active, inverted);

    imm->rect(wx, wy, ww, wh, mid);
    imm->quad(wx, wy, wx + r, wy + r, wx + r, wy + wh - r, wx, wy + wh, light);
    imm->quad(wx, wy, wx + r, wy + r, wx + ww - r, wy + r, wx + ww, wy, light);
    imm->quad(wx, wy + wh, wx + r, wy + wh - r, wx + ww - r, wy + wh - r, wx + ww, wy + wh, dark);
    imm->quad(wx + ww, wy, wx + ww - r, wy + r, wx + ww - r, wy + wh - r, wx + ww, wy + wh, dark);
}

void ui_corner(Imm * imm,
        float x1, float y1, float x2, float y2, float x3, float y3,
        float rx, float ry, bool active, bool inverted)
{
    Color light, mid, dark;
    ui_colors(&light, &mid, &dark, active, inverted);

    Color d1 = x2 > x1? light : dark;
    Color d2 = x2 < x1? light : dark;
    Color d3 = y2 > y1? light : dark;
    Color d4 = y2 < y1? light : dark;

    imm->rect(x1, y1, x2 - x1, y3 - y1, mid);
    imm->rect(x2, y1, x3 - x2, y2 - y1, mid);

    imm->quad(x1, y1, x1 + rx, y1 + ry, x3 - rx, y1 + ry, x3, y1, d3);
    imm->quad(x1, y1, x1 + rx, y1 + ry, x1 + rx, y3 - ry, x1, y3, d1);
    imm->quad(x1, y3, x1 + rx, y3 - ry, x2 - rx, y3 - ry, x2, y3, d4);
    imm->quad(x3, y1, x3 - rx, y1 + ry, x3 - rx, y2 - ry, x3, y2, d2);
    imm->quad(x2, y3, x2 - rx, y3 - ry, x2 - rx, y2 - ry, x2, y2, d2);
    imm->quad(x3, y2, x3 - rx, y2 - ry, x2 - rx, y2 - ry, x2, y2, d4);
}

void ui_window(Imm * imm, Font prop, Font mono, Window * w, bool active) {
    //TODO: refactor this to use the window region Rects for drawing (less duplicate code)
    //TODO: hover interaction of some sort?
    //TODO: allow windows to be non-resizeable
    //TODO: draw non-resizeable windows differently (one solid outline for the border)
    float r = uiRadius;
    float border = windowBorder;
    float button = windowButton;
    float corner = windowCorner;
    // float menu = windowMenu;

    float wx = w->x;
    float wy = w->y;
    float ww = w->w;
    float wh = w->h;

    //backplate
    ui_rect(imm, wx, wy, ww, wh, r, false, false);

    float x1 = wx;
    float y1 = wy;
    float x2 = wx + ww;
    float y2 = wy + wh;

    //corners
    ui_corner(imm, x1, y1, x1 + border, y1 + border, x1 + corner, y1 + corner,  r,  r, active,
        w->top && w->left);
    ui_corner(imm, x2, y1, x2 - border, y1 + border, x2 - corner, y1 + corner, -r,  r, active,
        w->top && w->right);
    ui_corner(imm, x2, y2, x2 - border, y2 - border, x2 - corner, y2 - corner, -r, -r, active,
        w->bottom && w->right);
    ui_corner(imm, x1, y2, x1 + border, y2 - border, x1 + corner, y2 - corner,  r, -r, active,
        w->bottom && w->left);

    //edges
    // bool horiz = w->left || w->right;
    // bool vert = w->top || w->bottom;
    ui_rect(imm, wx + corner, wy, ww - corner * 2, border, r, active,
        w->top && !w->left && !w->right);
    ui_rect(imm, wx + corner, wy + wh - border, ww - corner * 2, border, r, active,
        w->bottom && !w->left && !w->right);
    ui_rect(imm, wx, wy + corner, border, wh - corner * 2, r, active,
        w->left && !w->top && !w->bottom);
    ui_rect(imm, wx + ww - border, wy + corner, border, wh - corner * 2, r, active,
        w->right && !w->top && !w->bottom);

    //buttons
    ui_rect(imm, wx + border, wy + border, button, button, r, active, w->closing);
    ui_rect(imm, wx + corner, wy + border, button, button, r, active, false);
    ui_rect(imm, wx + corner + button, wy + border, button, button, r, active, false);

    //title bar
    ui_rect(imm, wx + corner + button * 2, wy + border, ww - border * 2 - button * 3, button,
        r, active, w->dragging);

    //menu bar
    // ui_rect(wx + border, wy + corner, ww - border * 2, menu, r, false, false);

    //title
    imm->useFont(prop);
    imm->drawText(w->title, wx + corner + button * 2 + r * 2, wy + border + 1,
        0.5f, active? uiBlack : uiDark);
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

struct EditorState {
    ArrayList<TextArea> nodes;
    int activeNode;
    int curx, cury;
    int selx, sely;
    int targetx;
    bool selecting;
    bool scrolling;

    //how far down the top of the node list viewport is scrolled, in pixels
    float nodeListScrollAmount;
    //y coordinate of the mouse relative to the top of the scroll bar when it was grabbed
    float relativeScrollGrab;
};

struct EditorLayout {
    Rect drawableArea;
        Rect outerNodeList;
            Rect innerNodeList;
                //other nodes are calculated by simply offsetting this whole layout in y
                Rect firstNode;
                    Rect outerTextArea;
                        Rect innerTextArea;
                            Rect text;
                float nodeSpacing; //the full height of a node
                float nodeListHeight;
            Rect scrollUp;
            Rect scrollDown;
            Rect scrollArea;
                Rect scrollBar;

    void init(Window * window);
};

void layout_scrollable_viewport(Rect outer, Rect * inner, Rect * up, Rect * down, Rect * area) {
    float scrollWidth = 16;
    Rect inside = { outer.x + uiRadius, outer.y + uiRadius,
                    outer.w - uiRadius * 2, outer.h - uiRadius * 2 };
    *inner = { inside.x, inside.y, inside.w - scrollWidth, inside.h };
    Rect scroll = { inner->x + inner->w, inner->y, scrollWidth, inner->h };
    *up = { scroll.x, scroll.y, scrollWidth, scrollWidth };
    *down = { scroll.x, scroll.y + scroll.h - scrollWidth, scrollWidth, scrollWidth };
    *area = { scroll.x, scroll.y + scrollWidth, scrollWidth, scroll.h - scrollWidth * 2 };
}

void EditorLayout::init(Window * window) {
    drawableArea = get_absolute_bounds(window, windowArea);
    EditorState * editor = (EditorState *) window->appData;

    float margin = 8;
    outerNodeList = {
        drawableArea.x + margin,
        drawableArea.y + margin,
        700, //TODO: make this based on something real maybe?
        drawableArea.h - margin * 2,
    };

    layout_scrollable_viewport(outerNodeList, &innerNodeList, &scrollUp, &scrollDown, &scrollArea);

    //TODO: calculate these dimensions from something more reasonable (font metrics?);
    float nodeWidth = 620;
    float nodeHeight = 432;
    firstNode = {
        innerNodeList.x + innerNodeList.w / 2 - nodeWidth / 2,
        innerNodeList.y + margin,
        nodeWidth, nodeHeight,
    };
    nodeSpacing = nodeHeight + margin;
    nodeListHeight = margin + editor->nodes.len * nodeSpacing;
    // nodeListHeight = margin + 1 * nodeSpacing;

    float barHeight = fmin(1, innerNodeList.h / nodeListHeight) * scrollArea.h;
    float scrollPercent = editor->nodeListScrollAmount / nodeListHeight;
    scrollBar = {
        scrollArea.x,
        scrollArea.y + scrollPercent * scrollArea.h,
        scrollArea.w,
        barHeight,
    };

    //TODO: calculate text area width from font metrics
    outerTextArea = { firstNode.x + margin, firstNode.y + margin, 275, firstNode.h - margin * 2 };
    innerTextArea = { outerTextArea.x + uiRadius, outerTextArea.y + uiRadius,
                      outerTextArea.w - uiRadius * 2, outerTextArea.h - uiRadius * 2 };
    text = { outerTextArea.x + margin, outerTextArea.y + margin,
             outerTextArea.w - margin * 2, outerTextArea.h - margin * 2 };
}

EditorLayout layout_editor(Window * window) {
    EditorLayout layout = {};
    layout.init(window);
    return layout;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

void ui_rect(Imm * imm, Rect rect, float radius, bool active, bool inverted) {
    ui_rect(imm, rect.x, rect.y, rect.w, rect.h, radius, active, inverted);
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

void editor_draw(GameState * game, Window * window) {
    //TODO: add overloads to Imm for passing Rect struct directly

    Imm * imm = &game->imm;
    EditorState * editor = (EditorState *) window->appData;
    EditorLayout layout = layout_editor(window);

    ui_rect(imm, layout.outerNodeList, uiRadius, false, true);
    //draw scroll controls as inactive when the list fits entirely within the viewport
    bool scrollAvailable = layout.nodeListHeight > layout.innerNodeList.h;
    ui_rect(imm, layout.scrollUp, uiRadius, scrollAvailable, false);
    ui_rect(imm, layout.scrollDown, uiRadius, scrollAvailable, false);
    imm->rect(layout.scrollArea.x, layout.scrollArea.y, layout.scrollArea.w, layout.scrollArea.h,
        color16(10));
    ui_rect(imm, layout.scrollBar, uiRadius, scrollAvailable, editor->scrolling);

    imm->clip(layout.innerNodeList.x, layout.innerNodeList.y,
        layout.innerNodeList.w, layout.innerNodeList.h);
    for (int i = 0; i < editor->nodes.len; ++i) {
        TextArea * area = &editor->nodes[i];

        //account for scroll offset
        imm->offy = i * layout.nodeSpacing - editor->nodeListScrollAmount;
        ui_rect(imm, layout.firstNode, uiRadius, true, false);
        ui_rect(imm, layout.outerTextArea, uiRadius, true, true);
        imm->rect(layout.innerTextArea.x, layout.innerTextArea.y,
            layout.innerTextArea.w, layout.innerTextArea.h, color16(13));

        //draw selection and cursor behind the text
        float ts = 0.5f;
        float cw = text_width(game->mono, ts, "a"); //'a' is arbitrary (any visible char will work)
        float ch = 20;
        if (editor->activeNode == i) {
            int x1 = editor->curx;
            int y1 = editor->cury;
            int x2 = editor->selx;
            int y2 = editor->sely;

            if (y1 > y2) {
                swap(x1, x2);
                swap(y1, y2);
            }

            if (y1 != y2) {
                imm->rect(layout.text.x + x1 * cw, layout.text.y + y1 * ch,
                    cw * (strlen(area->text[y1]) - x1) + 2, ch, color16(15));
                for (int l = y1 + 1; l < y2; ++l) {
                    imm->rect(layout.text.x, layout.text.y + l * ch,
                        cw * strlen(area->text[l]) + 2, ch, color16(15));
                }
                imm->rect(layout.text.x, layout.text.y + y2 * ch,
                    cw * x2 + 2, ch, color16(15));
            } else {
                int begin = imin(x1, x2);
                int end = imax(x1, x2);
                imm->rect(layout.text.x + begin * cw, layout.text.y + y1 * ch,
                    cw * (end - begin) + 2, ch, color16(15));
            }

            float cx = layout.text.x + editor->curx * cw;
            float cy = layout.text.y + editor->cury * ch;
            imm->rect(cx, cy, 2, ch, color16(4));
        }

        const Color textColors[] = {
            color16( 3,  3,  3), //TEXT_DEFAULT
            color16(15,  0, 15), //TEXT_INVALID
            color16( 4,  7,  4), //TEXT_COMMENT
            color16(11,  2,  2), //TEXT_COLON
            color16( 9,  5,  9), //TEXT_CONST
            color16( 0,  0,  0), //TEXT_IDENTIFIER
        };

        //draw source text
        imm->useFont(game->mono);
        for (int l = 0; l < MAX_LINES; ++l) {
            imm->drawTextWithColors(area->text[l], layout.text.x, layout.text.y + l * ch, ts,
                area->colors[l], textColors);
        }

        //draw error info
        if (area->foundError) {
            //draw error underline
            imm->rect(layout.text.x + area->errorStart * cw,
                      layout.text.y + area->errorLine * ch + ch - 2,
                      (area->errorEnd - area->errorStart) * cw, 4, color16(15, 0, 0));

            //draw error message
            //TODO: proper positioning and text wrapping
            imm->useFont(game->prop);
            imm->drawText(area->errorMessage,
                layout.outerTextArea.x + layout.outerTextArea.w +  10,
                layout.outerTextArea.y + layout.outerTextArea.h - 400,
                0.333f, color16(15, 3, 0));
        }
    }
    imm->offy = 0;
    imm->noClip();
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

//TODO: remove selected text when typing BACKSPACE, DELETE, RETURN, or a char

void editor_text(GameState * game, Window * window, uint codepoint) {
    if (codepoint < 128 && codepoint >= ' ') {
        //DEBUG PRINT
        printf("EDITOR RECIEVED KEY: %c\n", (char) codepoint);

        EditorState * editor = (EditorState *) window->appData;
        TextArea * area = &editor->nodes[editor->activeNode];

        if (strlen(area->text[editor->cury]) < MAX_LINE_LEN) {
            int len = strlen(area->text[editor->cury]);
            for (int i = len; i >= editor->curx; --i) {
                area->text[editor->cury][i + 1] = area->text[editor->cury][i];
            }
            area->text[editor->cury][editor->curx] = toupper(codepoint);

            editor->curx += 1;
            editor->selx = editor->curx;

            area->shouldRecompile = true;
        }
    }
}

void editor_key(GameState * game, Window * window, int keycode, int mods, bool down, bool repeat) {
    //DEBUG PRINT
    printf("KEY: %d, %d, %d, %d\n", keycode, mods, (int) down, (int) repeat);

    EditorState * editor = (EditorState *) window->appData;
    TextArea * area = &editor->nodes[editor->activeNode];

    if (down) {
        if (keycode == SDLK_LEFT) {
            if (editor->curx > 0) {
                editor->curx -= 1;
            } else if (editor->cury > 0) { //wrap
                editor->cury -= 1;
                editor->curx = strlen(area->text[editor->cury]);
            }
            editor->targetx = editor->curx;
        } else if (keycode == SDLK_RIGHT) {
            if (editor->curx < strlen(area->text[editor->cury])) {
                editor->curx += 1;
            } else if (editor->cury < MAX_LINES - 1) { //wrap
                editor->cury += 1;
                editor->curx = 0;
            }
            editor->targetx = editor->curx;
        } else if (keycode == SDLK_UP) {
            if (editor->cury > 0) {
                editor->cury -= 1;
                editor->curx = imin(editor->targetx, strlen(area->text[editor->cury]));
            }
        } else if (keycode == SDLK_DOWN) {
            if (editor->cury < MAX_LINES - 1) {
                editor->cury += 1;
                editor->curx = imin(editor->targetx, strlen(area->text[editor->cury]));
            }
        } else if (keycode == SDLK_BACKSPACE) {
            if (editor->curx > 0) {
                //remove char from line
                editor->curx -= 1;
                int len = strlen(area->text[editor->cury]);
                for (int i = editor->curx; i < len; ++i) {
                    area->text[editor->cury][i] = area->text[editor->cury][i + 1];
                }

                area->shouldRecompile = true;
            } else if (editor->cury > 0) {
                int len1 = strlen(area->text[editor->cury - 1]);
                int len2 = strlen(area->text[editor->cury]);
                if (len1 + len2 <= MAX_LINE_LEN) {
                    strcat(area->text[editor->cury - 1], area->text[editor->cury]);
                    //move all following lines up by 1
                    for (int i = editor->cury; i < MAX_LINES - 1; ++i) {
                        strcpy(area->text[i], area->text[i + 1]);
                    }
                    area->text[MAX_LINES - 1][0] = '\0';

                    editor->cury -= 1;
                    editor->curx = len1;

                    area->shouldRecompile = true;
                }
            }
        } else if (keycode == SDLK_DELETE) {
            //TODO: handle delete key
        } else if (keycode == SDLK_RETURN) {
            if (editor->cury < MAX_LINES - 1 && strlen(area->text[MAX_LINES - 1]) == 0) {
                for (int i = MAX_LINES - 2; i > editor->cury; --i) {
                    strcpy(area->text[i + 1], area->text[i]);
                }
                strcpy(area->text[editor->cury + 1], area->text[editor->cury] + editor->curx);
                area->text[editor->cury][editor->curx] = '\0';

                editor->cury += 1;
                editor->curx = 0;

                area->shouldRecompile = true;
            }
        }

        bool shift = mods & KMOD_LSHIFT || mods & KMOD_RSHIFT;
        if (!shift) {
            editor->selx = editor->curx;
            editor->sely = editor->cury;
        }
    }
}

bool contains(Rect rect, float x, float y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.w && y < rect.y + rect.h;
}

void editor_mouse(GameState * game, Window * window,
        float mx, float my, bool down, bool lb, bool rb)
{
    //DEBUG PRINT
    printf("MOUSE: %f, %f, %d, %d, %d\n", mx, my, (int) down, (int) lb, (int) rb);

    EditorState * editor = (EditorState *) window->appData;
    EditorLayout layout = layout_editor(window);

    if (!down && lb) {
        editor->scrolling = false;
    }

    bool scrollAvailable = layout.nodeListHeight > layout.innerNodeList.h;
    if (scrollAvailable && down && lb && contains(layout.scrollBar, mx, my)) {
        editor->scrolling = true;
        editor->relativeScrollGrab = my - layout.scrollBar.y;
    }

    //capture mouse activity in editor
    if (down && lb) {
        for (int i = 0; i < editor->nodes.len; ++i) {
            float rmx = mx;
            float rmy = my + editor->nodeListScrollAmount - i * layout.nodeSpacing;

            if (contains(layout.text, rmx, rmy)) {
                //'a' is arbitrary (any visible char will work)
                float cw = text_width(game->mono, 0.5f, "a");
                float ch = 20;
                int curx = (rmx - layout.text.x) / cw + 0.5f;
                int cury = (rmy - layout.text.y) / ch;
                cury = imax(0, imin(MAX_LINES - 1, cury));
                curx = imax(0, imin(curx, strlen(editor->nodes[i].text[cury])));

                editor->activeNode = i;
                editor->curx = curx;
                editor->cury = cury;
                editor->selx = curx;
                editor->sely = cury;
                editor->targetx = curx;
                editor->selecting = true;
            }
        }
    } else if (!down && lb) {
        editor->selecting = false;
    }
}

void editor_drag(GameState * game, Window * window,
        float mx, float my, float dx, float dy, int buttonMask)
{
    bool lb = buttonMask & SDL_BUTTON_LMASK;

    EditorState * editor = (EditorState *) window->appData;
    EditorLayout layout = layout_editor(window);

    if (lb && editor->scrolling) {
        float maxScrollAmount = layout.scrollArea.h - layout.scrollBar.h;
        float relativeBarPos = fmax(0, fmin(maxScrollAmount,
            my - editor->relativeScrollGrab - layout.scrollArea.y));
        editor->nodeListScrollAmount = relativeBarPos/layout.scrollArea.h * layout.nodeListHeight;
    }

    if (editor->selecting) {
        //'a' is arbitrary (any visible char will work)
        float rmx = mx;
        float rmy = my + editor->nodeListScrollAmount - editor->activeNode * layout.nodeSpacing;

        //TODO: de-dup logic with that in editor_mouse()
        float cw = text_width(game->mono, 0.5f, "a");
        float ch = 20;
        int curx = (rmx - layout.text.x) / cw + 0.5f;
        int cury = (rmy - layout.text.y) / ch;
        cury = imax(0, imin(MAX_LINES - 1, cury));
        curx = imax(0, imin(curx, strlen(editor->nodes[editor->activeNode].text[cury])));

        editor->curx = curx;
        editor->cury = cury;
        editor->targetx = curx;
    }
}

void editor_scroll(GameState * game, Window * window,
        float mx, float my, float dx, float dy)
{
    //DEBUG PRINT
    printf("EDITOR SCROLL   %.0f   %.0f   %.0f   %.0f\n", mx, my, dx, dy);

    EditorState * editor = (EditorState *) window->appData;
    EditorLayout layout = layout_editor(window);

    if (contains(layout.innerNodeList, mx, my)) {
        float maxScrollAmount = fmax(0, layout.nodeListHeight - layout.innerNodeList.h);
        editor->nodeListScrollAmount =
            fmax(0, fmin(maxScrollAmount, editor->nodeListScrollAmount + dy * 10));
    }
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

struct Launcher {
    WindowTag window;
    const char * title;
    bool launching;
};

void bring_to_front(Window ** windows, int windowCount, int windex) {
    Window * win = windows[windex];
    for (int i = windex; i > 0; --i) {
        windows[i] = windows[i - 1];
    }
    windows[0] = win;
}

void send_to_back(Window ** windows, int windowCount, int windex) {
    Window * win = windows[windex];
    //TODO: only move the window back until it's behind all open windows?
    for (int i = windex + 1; i < windowCount; ++i) {
        windows[i - 1] = windows[i];
    }
    windows[windowCount - 1] = win;
}

//TODO: fold this into GameState?
bool mouseDraggingInWindow = false;

void window_mouse_button_event(GameState * game, Window ** windows, int windowCount, int windex,
        float x, float y, bool down, bool lb, bool rb)
{
    Window * w = windows[windex];
    bool inside = window_hover(w, windowInside, x, y);
    bool title = window_hover(w, windowTitle, x, y);
    bool topLeft = window_hover(w, windowTopLeft, x, y);
    bool topRight = window_hover(w, windowTopRight, x, y);
    bool bottomLeft = window_hover(w, windowBottomLeft, x, y);
    bool bottomRight = window_hover(w, windowBottomRight, x, y);
    bool top = window_hover(w, windowTop, x, y) || topLeft || topRight;
    bool left = window_hover(w, windowLeft, x, y) || topLeft || bottomLeft;
    bool right = window_hover(w, windowRight, x, y) || topRight || bottomRight;
    bool bottom = window_hover(w, windowBottom, x, y) || bottomLeft || bottomRight;
    bool close = window_hover(w, windowClose, x, y);

    if (down) {
        if (window_hover(w, windowArea, x, y)) {
            if (w->mouse != nullptr) {
                w->mouse(game, w, x, y, down, lb, rb);
                mouseDraggingInWindow = true;
            }
        }

        if (lb && close) {
            w->closing = true;
        }

        if (lb && title) {
            w->dragging = true;
        }

        if (lb && !inside && (top || left || right || bottom)) {
            w->top = top;
            w->left = left;
            w->right = right;
            w->bottom = bottom;
        }
    } else {
        //pass mouse-up events to the active window if they correspond to a mouse-down event that
        //was recieved by that window, even if the mouse is not longer hovering it
        if (mouseDraggingInWindow) {
            mouseDraggingInWindow = false;
            if (windows[0]->mouse != nullptr) {
                windows[0]->mouse(game, windows[0], x, y, down, lb, rb);
            }
        }

        if (lb && w->closing) {
            w->closing = false;
            if (close) {
                w->open = false;
                send_to_back(windows, windowCount, windex);
            }
        }

        if (lb) {
            w->dragging = false;
            w->top = w->left = w->right = w->bottom = false;
        }
    }
}

//this is broken out into a function because it makes control flow easier
void mouse_button_event(GameState * game, float ww, float wh,
        Launcher * launchers, int launcherCount,
        Window ** windows, int windowCount,
        float x, float y, bool down, bool lb, bool rb)
{
    if (down) {
        //check windows from front to back so that windows in front get priority
        for (int i = 0; i < windowCount && windows[i]->open; ++i) {
            if (window_hover(windows[i], windowEverything, x, y)) {
                if (lb) {
                    bring_to_front(windows, windowCount, i);
                }

                window_mouse_button_event(game, windows, windowCount, 0, x, y, down, lb, rb);
                return;
            }
        }

        for (int i = 0; i < launcherCount; ++i) {
            //TODO: find a unified way to retrieve the area of the n'th launcher
            //      in order to reduce code duplication
            float button = 80;
            float label = 20;
            float margin = 10;

            float lx = ww - button - margin;
            float ly = wh - (button + label + margin) * (i + 1);
            bool hovered = x > lx && x < lx + button &&
                           y > ly && y < ly + button + label;

            if (lb && hovered) {
                launchers[i].launching = true;
                return;
            }
        }
    } else {
        //if we area launching a window, then open it and give it focus
        if (lb) {
            for (int i = 0; i < launcherCount; ++i) {
                if (launchers[i].launching) {
                    launchers[i].launching = false;

                    //TODO: find a unified way to retrieve the area of the n'th launcher
                    //      in order to reduce code duplication
                    float button = 80;
                    float label = 20;
                    float margin = 10;

                    float lx = ww - button - margin;
                    float ly = wh - (button + label + margin) * (i + 1);
                    bool hovered = x > lx && x < lx + button &&
                                   y > ly && y < ly + button + label;

                    if (hovered) {
                        //find window with this launcher's tag
                        int windex = -1;
                        for (int j = 0; j < windowCount; ++j) {
                            if (windows[j]->tag == launchers[i].window) {
                                windex = j;
                                break;
                            }
                        }

                        if (windex >= 0) {
                            windows[windex]->open = true;
                            bring_to_front(windows, windowCount, windex);
                        }
                    }

                    return;
                }
            }
        }

        window_mouse_button_event(game, windows, windowCount, 0, x, y, down, lb, rb);
    }
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

int main() {
    //initialize timer
    applicationStartupTimeValue = SDL_GetPerformanceCounter();

    GameState gameState = {};
    GameState * game = &gameState;
    Imm * imm = &game->imm;

    EditorState editorState = {};
    EditorState * editor = &editorState;
    ArrayList<TextArea> * areas = &editor->nodes;

    *areas = load_save_from_file("res/test-program.S");

    for (int i = 0; i < areas->len; ++i) {
        compile_text_area(&areas->data[i], i);
    }

    printf("Imm size: %lu\n", sizeof(Imm));
    printf("Instruction size: %lu\n", sizeof(Instruction));
    printf("TextArea size: %lu\n", sizeof(TextArea));
    printf("areas size: %lu\n", areas->len * sizeof(TextArea));
    printf("GameState size: %lu\n", sizeof(GameState));
    printf("Window size: %lu\n", sizeof(Window));
    printf("EditorState size: %lu\n", sizeof(EditorState));
    printf("EditorLayout size: %lu\n", sizeof(EditorLayout));

    printf("pre-init: %f seconds\n", get_time());

    // return 0;

    //haptic subsystem takes a long time to init for some reason, but we don't use it
    //so to decrease startup time, we manually specify which systems to init
    if (SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL FAILED TO INIT: %s\n", SDL_GetError());
        return 1;
    }

    printf("SDL init: %f seconds\n", get_time());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_Window * window = SDL_CreateWindow("Soup Window",
        SDL_WINDOWPOS_CENTERED_DISPLAY(0), SDL_WINDOWPOS_CENTERED_DISPLAY(0),
        1280, 800,
        SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == nullptr) {
        printf("SDL FAILED TO CREATE WINDOW: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);

    glEnable(GL_FRAMEBUFFER_SRGB);

    printf("SDL create window: %f seconds\n", get_time());

    imm->init(5000);
    game->prop = load_font("res/test.fnt");
    game->mono = load_font("res/mono.fnt");
    imm->font = game->prop;

    Texture bliss = load_texture("res/windows_xp_bliss-wide.jpg");

    printf("init finished: %f seconds\n", get_time());

    Launcher launchers[] = {
        { WINDOW_NONE, "Manual" },
        { WINDOW_MAIL, "Mail" },
        { WINDOW_OPTIONS, "Options" },
        { WINDOW_EDITOR, "Editor" },
    };

    Window windows[] = {
        { editor_draw, editor_text, editor_key, editor_mouse, editor_drag, editor_scroll,
            editor, "Editor", WINDOW_EDITOR, 100, 100, 800, 600, true },
        { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, "Mail", WINDOW_MAIL, 200, 200, 400, 300, false },
        { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, "Options", WINDOW_OPTIONS, 300, 300, 400, 300, false },
    };

    Window * orderedWindows[ARR_SIZE(windows)] {
        &windows[0], &windows[1], &windows[2],
    };



    int frameCount = 0;
    bool shouldExit = false;
    while (!shouldExit) {
        float dmx = 0;
        float dmy = 0;

        //TODO: test intra-frame event handling robustness by introducing artificial lag

        int ww, wh;
        SDL_GetWindowSize(window, &ww, &wh);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                shouldExit = true;
            } else if (event.type == SDL_MOUSEMOTION) {
                dmx += event.motion.xrel;
                dmy += event.motion.yrel;

                //pass events to active window if mouseDraggingInWindow
                Window * w = orderedWindows[0];
                if (w->open && event.motion.state != 0 && w->drag != nullptr) {
                    w->drag(game, w, event.motion.x, event.motion.y,
                        event.motion.xrel, event.motion.yrel, event.motion.state);
                }
            } else if (event.type == SDL_TEXTINPUT) {
                Window * w = orderedWindows[0];
                if (w->open && w->text != nullptr) {
                    //TODO: proper utf-8 support here?
                    w->text(game, w, event.text.text[0]);
                }
            } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                Window * w = orderedWindows[0];
                if (w->open && w->key != nullptr) {
                    w->key(game, w, event.key.keysym.sym, event.key.keysym.mod,
                        event.key.state == SDL_PRESSED, event.key.repeat);
                }
            } else if (event.type == SDL_MOUSEWHEEL) {
                //DEBUG PRINT
                printf("MOUSE WHEEL   x %d   y %d   flipped %d   touch %d\n",
                    event.wheel.x, event.wheel.y,
                    event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED,
                    event.wheel.which == SDL_TOUCH_MOUSEID);

                int imx, imy;
                u32 mouseMask = SDL_GetMouseState(&imx, &imy);

                //TODO: fix the weird abhorent scroll behavior

                //we want inactive windows to be able to respont to scroll events,
                //so we pass them to the foremost hovered window instead of the active window
                for (int i = 0; i < ARR_SIZE(orderedWindows); ++i) {
                    Window * w = orderedWindows[i];
                    if (w->open && window_hover(w, windowEverything, imx, imy)) {
                        if (window_hover(w, windowArea, imx, imy)) {
                            int flip = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED? -1 : 1;
                            w->scroll(game, w, imx, imy,
                                event.wheel.x * flip, event.wheel.y * flip);
                        }

                        break;
                    }
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                mouse_button_event(game, ww, wh,
                    launchers, ARR_SIZE(launchers),
                    orderedWindows, ARR_SIZE(orderedWindows),
                    event.button.x, event.button.y,
                    event.button.state == SDL_PRESSED,
                    event.button.button == SDL_BUTTON_LEFT,
                    event.button.button == SDL_BUTTON_RIGHT);
            }
        }

        //recompile any text areas that need it
        for (int i = 0; i < areas->len; ++i) {
            if (areas->data[i].shouldRecompile) {
                printf("Compiling area #%d\n", i);
                compile_text_area(&areas->data[i], i);
                areas->data[i].shouldRecompile = false;
            }
        }

        //get mouse info
        int imx, imy;
        u32 mouseMask = SDL_GetMouseState(&imx, &imy);
        float mx = imx;
        float my = imy;
        // bool lb = mouseMask & SDL_BUTTON(SDL_BUTTON_LEFT);
        // bool rb = mouseMask & SDL_BUTTON(SDL_BUTTON_RIGHT);

        //handle window controls
        {
            //TODO: this messes up if the mouse leaves the bounds of the window and then
            //      comes back in while dragging. fix this!
            Window * w = orderedWindows[0];
            if (w->dragging) {
                w->x += dmx;
                w->y += dmy;
            }
            if (w->top) {
                w->y += dmy;
                w->h -= dmy;
            }
            if (w->left) {
                w->x += dmx;
                w->w -= dmx;
            }
            if (w->right) {
                w->w += dmx;
            }
            if (w->bottom) {
                w->h += dmy;
            }
        }

        //begin drawing
        int dw, dh;
        SDL_GL_GetDrawableSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        imm->tex = bliss;
        imm->begin(ww, wh, dw, dh, true);

        //draw launchers
        for (int i = 0; i < ARR_SIZE(launchers); ++i) {
            float button = 80;
            float label = 20;
            float margin = 10;

            float x = ww - button - margin;
            float y = wh - (button + label + margin) * (i + 1);
            bool hovered = mx > x && mx < x + button && my > y && my < y + button + label;

            //draw button + label
            ui_rect(imm, x, y, button, button, 2, hovered, launchers[i].launching);
            ui_rect(imm, x, y + button, button, label, 2, hovered, launchers[i].launching);
            float xpos = x + button / 2 - text_width(game->prop, 0.33f, launchers[i].title) / 2;
            imm->useFont(game->prop);
            imm->drawText(launchers[i].title, xpos, y + button + uiRadius,
                0.33f, hovered? uiBlack : uiDark);

            //draw hotkey tooltip
            char buffer[3] = "F0";
            buffer[1] = i + '1';
            imm->drawText(buffer, x + button - 20, y + 4, 0.33f, { 255, 255, 255, 255 });
        }

        //draw windows
        for (int i = ARR_SIZE(orderedWindows) - 1; i >= 0; --i) {
            Window * w = orderedWindows[i];
            if (w->open) {
                ui_window(imm, game->prop, game->mono, w, i == 0);

                if (w->draw != nullptr) {
                    w->draw(game, w);
                }
            }
        }

        imm->end(0); //TODO: properly calculate dt for frame

        SDL_GL_SwapWindow(window);

        if (frameCount == 0) {
            printf("first frame: %f seconds\n", get_time());
        } else if (frameCount == 1) {
            printf("second frame: %f seconds\n", get_time());
        }

        fflush(stdout);
        fflush(stderr);

        ++frameCount;

        //uncomment this line to exit immediately
        // if (frameCount > 1) shouldExit = true;
    }







    printf("time alive: %f seconds\n", get_time());

    //optional cleanup
    free_font(game->prop);
    free_font(game->mono);
    imm->finalize();
    glDeleteTextures(1, &bliss.handle);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("shutdown: %f seconds\n", get_time());

    return 0;
}
