/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

extern "C" {
#include "swe.h"
#include "utils/gl.h"
}

#if DEFINED(SWE_GUI)

#include "imgui.h"

static texture_t *g_font_tex = NULL;

namespace ImGui {
    bool VerticalTab(const char *text, bool *v);
};

static const char *VSHADER =
    "                                                               \n"
    "attribute vec3 a_pos;                                          \n"
    "attribute vec2 a_tex_pos;                                      \n"
    "attribute vec4 a_color;                                        \n"
    "                                                               \n"
    "uniform mat4 u_proj_mat;                                       \n"
    "                                                               \n"
    "varying vec2 v_tex_pos;                                        \n"
    "varying vec4 v_color;                                          \n"
    "                                                               \n"
    "void main()                                                    \n"
    "{                                                              \n"
    "    gl_Position = u_proj_mat * vec4(a_pos, 1.0);               \n"
    "    v_tex_pos = a_tex_pos;                                     \n"
    "    v_color = a_color;                                         \n"
    "}                                                              \n"
;

static const char *FSHADER =
    "                                                               \n"
    "#ifdef GL_ES                                                   \n"
    "precision mediump float;                                       \n"
    "#endif                                                         \n"
    "                                                               \n"
    "uniform sampler2D u_tex;                                       \n"
    "uniform float u_is_alpha_tex;                                  \n"
    "                                                               \n"
    "varying vec2 v_tex_pos;                                        \n"
    "varying vec4 v_color;                                          \n"
    "                                                               \n"
    "vec4 col;                                                      \n"
    "                                                               \n"
    "void main()                                                    \n"
    "{                                                              \n"
    "    col = texture2D(u_tex, v_tex_pos);                         \n"
    "    col = mix(col, vec4(1.0, 1.0, 1.0, col.r), u_is_alpha_tex);\n"
    "    gl_FragColor = col * v_color;                              \n"
    "}                                                              \n"
;

typedef struct {
    GLuint prog;

    GLuint a_pos_l;
    GLuint a_tex_pos_l;
    GLuint a_color_l;

    GLuint u_tex_l;
    GLuint u_is_alpha_tex_l; // Set to 1.0 if the tex is pure alpha.
    GLuint u_proj_mat_l;
} prog_t;

typedef struct gui {
    void    *user;
    fader_t more_info_opened;

    prog_t  prog;
    GLuint  array_buffer;
    GLuint  index_buffer;
} gui_t;

static void init_ImGui(gui_t *gui);

static void render_prepare_context(gui_t *gui)
{
    #define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
    // Setup render state: alpha-blending enabled, no face culling, no depth
    // testing, scissor enabled
    GL(glEnable(GL_BLEND));
    GL(glBlendEquation(GL_FUNC_ADD));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    GL(glDisable(GL_CULL_FACE));
    GL(glDisable(GL_DEPTH_TEST));
    GL(glEnable(GL_SCISSOR_TEST));
    GL(glActiveTexture(GL_TEXTURE0));

    // Setup orthographic projection matrix
    const float width = ImGui::GetIO().DisplaySize.x;
    const float height = ImGui::GetIO().DisplaySize.y;
    const float ortho_projection[4][4] =
    {
        { 2.0f / width, 0.0f,           0.0f, 0.0f },
        { 0.0f,         2.0f / -height, 0.0f, 0.0f },
        { 0.0f,         0.0f,          -1.0f, 0.0f },
        { -1.0f,        1.0f,           0.0f, 1.0f },
    };
    GL(glUseProgram(gui->prog.prog));
    GL(glUniformMatrix4fv(gui->prog.u_proj_mat_l, 1, 0,
                          &ortho_projection[0][0]));

    GL(glBindBuffer(GL_ARRAY_BUFFER, gui->array_buffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gui->index_buffer));
    // This could probably be done only at init time.
    GL(glEnableVertexAttribArray(gui->prog.a_pos_l));
    GL(glEnableVertexAttribArray(gui->prog.a_tex_pos_l));
    GL(glEnableVertexAttribArray(gui->prog.a_color_l));
    GL(glVertexAttribPointer(gui->prog.a_pos_l, 2, GL_FLOAT, false,
                             sizeof(ImDrawVert),
                             (void*)OFFSETOF(ImDrawVert, pos)));
    GL(glVertexAttribPointer(gui->prog.a_tex_pos_l, 2, GL_FLOAT, false,
                             sizeof(ImDrawVert),
                             (void*)OFFSETOF(ImDrawVert, uv)));
    GL(glVertexAttribPointer(gui->prog.a_color_l, 4, GL_UNSIGNED_BYTE,
                             true, sizeof(ImDrawVert),
                             (void*)OFFSETOF(ImDrawVert, col)));
    #undef OFFSETOF
}

static void ImImpl_RenderDrawLists(ImDrawData* draw_data)
{
    ImGuiIO& io = ImGui::GetIO();
    const float height = io.DisplaySize.y;
    texture_t *tex;
    gui_t *gui = (gui_t*)io.UserData;
    render_prepare_context(gui);
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawIdx* idx_buffer_offset = 0;

        GL(glBufferData(GL_ARRAY_BUFFER,
                (GLsizeiptr)cmd_list->VtxBuffer.size() * sizeof(ImDrawVert),
                (GLvoid*)&cmd_list->VtxBuffer.front(), GL_DYNAMIC_DRAW));

        GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                    (GLsizeiptr)cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx),
                    (GLvoid*)&cmd_list->IdxBuffer.front(), GL_DYNAMIC_DRAW));

        for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin();
             pcmd != cmd_list->CmdBuffer.end(); pcmd++)
        {
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
                render_prepare_context(gui); // Restore context.
            }
            else {
                tex = (texture_t*)pcmd->TextureId;
                assert(tex);
                GL(glBindTexture(GL_TEXTURE_2D, tex->id));
                GL(glScissor((int)pcmd->ClipRect.x,
                             (int)(height - pcmd->ClipRect.w),
                             (int)(pcmd->ClipRect.z - pcmd->ClipRect.x),
                             (int)(pcmd->ClipRect.w - pcmd->ClipRect.y)));
                GL(glUniform1f(gui->prog.u_is_alpha_tex_l,
                               pcmd->TextureId == io.Fonts->TexID ?
                               1.0 : 0.0));
                GL(glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount,
                                  GL_UNSIGNED_SHORT, idx_buffer_offset));
            }
            idx_buffer_offset += pcmd->ElemCount;
        }
    }
    GL(glDisable(GL_SCISSOR_TEST));
}

static void load_fonts_texture(void)
{
    const void *data;
    int data_size;
    unsigned char* pixels;
    int width, height;
    ImGuiIO& io = ImGui::GetIO();
    const ImWchar ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0370, 0x03FF, // Greek
        0x2600, 0x267F, // Symbols Misc
        0x2700, 0x27BF, // Dingbat
        0
    };
    ImFontConfig conf;

    conf.FontDataOwnedByAtlas = false;
    data = asset_get_data("asset://font/DejaVuSans-small.ttf",
                          &data_size, NULL);
    io.Fonts->AddFontFromMemoryTTF((void*)data, data_size, 28, &conf, ranges);
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
    g_font_tex = texture_create(width, height, 1);
    texture_set_data(g_font_tex, pixels, width, height, 1);
    io.Fonts->TexID = (void*)g_font_tex;
}

static void init_ImGui(gui_t *gui)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    io.UserData = gui;
    io.DeltaTime = 1.0f/60.0f;
    io.RenderDrawListsFn = ImImpl_RenderDrawLists;
    io.FontGlobalScale = 0.5;
    style.FramePadding.x = 6;
    style.FramePadding.y = 3;
    style.WindowRounding = 0;
    style.FrameRounding = 2;

    load_fonts_texture();

    io.KeyMap[ImGuiKey_Tab]         = KEY_TAB;
    io.KeyMap[ImGuiKey_LeftArrow]   = KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow]  = KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow]     = KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow]   = KEY_DOWN;
    io.KeyMap[ImGuiKey_PageUp]      = KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown]    = KEY_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home]        = KEY_HOME;
    io.KeyMap[ImGuiKey_End]         = KEY_END;
    io.KeyMap[ImGuiKey_Delete]      = KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace]   = KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter]       = KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape]      = KEY_ESCAPE;
    io.KeyMap[ImGuiKey_A]           = 'A';
    io.KeyMap[ImGuiKey_C]           = 'C';
    io.KeyMap[ImGuiKey_V]           = 'V';
    io.KeyMap[ImGuiKey_X]           = 'X';
    io.KeyMap[ImGuiKey_Y]           = 'Y';
    io.KeyMap[ImGuiKey_Z]           = 'Z';

}

void gui_init(void *user)
{
    gui_t *gui = (gui_t*)calloc(1, sizeof(*gui));
    prog_t *p;

    gui->user = user;
    gui->prog.prog = gl_create_program(VSHADER, FSHADER, NULL, NULL);
    p = &gui->prog;
    GL(glUseProgram(p->prog));
#define UNIFORM(x) p->x##_l = glGetUniformLocation(p->prog, #x);
#define ATTRIB(x) p->x##_l = glGetAttribLocation(p->prog, #x)
    UNIFORM(u_proj_mat);
    UNIFORM(u_tex);
    UNIFORM(u_is_alpha_tex);
    ATTRIB(a_pos);
    ATTRIB(a_tex_pos);
    ATTRIB(a_color);
#undef ATTRIB
#undef UNIFORM
    GL(glUniform1i(p->u_tex_l, 0));
    GL(glGenBuffers(1, &gui->array_buffer));
    GL(glGenBuffers(1, &gui->index_buffer));
    init_ImGui(gui);
}

void gui_release(void)
{
    ImGui::Shutdown();
}

// Function that create a base widget.
// +------------+-------+--------+
// | label      |   sub | widget |
// +------------+-------+--------+
static bool gui_base_widget(const char *label,
                            const char *sublabel, double ws,
                            bool button = false)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushID(label);
    double width, height, spacing;
    ImVec2 label_size, pos;
    bool ret = false;

    spacing = style.ItemSpacing.x;
    width = ImGui::GetContentRegionAvailWidth();
    label_size = ImGui::CalcTextSize(label, NULL, true);
    height = label_size.y + style.FramePadding.y * 2;

    if (button) {
        pos = ImGui::GetCursorPos();
        ret = ImGui::Button("", ImVec2(-1, 0));
        ImGui::SetItemAllowOverlap();
        ImGui::SetCursorPos(pos);
    }

    ImGui::Dummy(ImVec2(1, height));
    ImGui::SameLine();
    ImGui::AlignFirstTextHeightToWidgets();
    ImGui::Text("%s", label);
    ws = ws * label_size.y + style.FramePadding.y * 2;

    if (sublabel) {
        ImGui::SameLine(width - ws - spacing * 2
                            - ImGui::CalcTextSize(sublabel, NULL, true).x);
        ImGui::Text("%s", sublabel);
    }

    ImGui::SameLine(width - ws - spacing);
    ImGui::PushItemWidth(ws);
    return ret;
}

static void gui_base_widget_end(void)
{
    ImGui::PopItemWidth();
    ImGui::PopID();
}

void gui_tabs(char *current)
{
    ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("tabs-current"),
                                         current);
    ImGui::GetStateStorage()->SetFloat(ImGui::GetID("tabs-base-y"),
                ImGui::GetCursorPosY());
}

void gui_tabs_end(void)
{
}

bool gui_tab(const char *label)
{
    bool v;
    char *current = (char *)ImGui::GetStateStorage()->GetVoidPtr(
                                            ImGui::GetID("tabs-current"));
    v = strcmp(current, label) == 0;
    if (ImGui::VerticalTab(label, &v)) strcpy(current, label);
    if (v) {
        ImGui::GetStateStorage()->SetFloat(ImGui::GetID("tabs-next-y"),
                ImGui::GetCursorPosY());
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetStateStorage()->GetFloat(
                    ImGui::GetID("tabs-base-y")));
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(0, 0));
    }
    return v;
}

void gui_tab_end(void)
{
    ImGui::EndGroup();
    ImGui::SetCursorPosY(ImGui::GetStateStorage()->GetFloat(
                    ImGui::GetID("tabs-next-y")));
}

// Set the next tab as open at startup.
void gui_set_next_tab_open(void)
{
    ImGui::SetNextTreeNodeOpen(true, ImGuiSetCond_Once);
}

typedef struct gui_stack gui_stack_t;
struct gui_stack {
    void (*f)(void *user);
    void *user;
    gui_stack_t *next;
};
static ImGuiID g_current_gui_stack = 0;

bool gui_stack(const char *id_, void *user)
{
    gui_stack_t *stack;
    ImGuiID id;
    void (*f)(void *user);
    // We don't support stack inside stack for the moment.
    assert(!g_current_gui_stack);
    id = ImGui::GetID(id_);
    g_current_gui_stack =  id;
    stack = (gui_stack_t*)ImGui::GetStateStorage()->GetVoidPtr(id);
    if (!stack) return true;
    f = stack->f;
    user = stack->user ?: user;
    if (gui_button("Back", -1)) gui_stack_pop();
    f(user);
    g_current_gui_stack = 0;
    return false;
}

void gui_stack_end(void)
{
    g_current_gui_stack = 0;
}

void gui_stack_push(void (*f)(void *user), void *user)
{
    gui_stack_t *stack;
    ImGuiID id = g_current_gui_stack;
    assert(id);
    stack = (gui_stack_t*)calloc(1, sizeof(*stack));
    stack->f = f;
    stack->user = user;
    stack->next = (gui_stack_t*)ImGui::GetStateStorage()->GetVoidPtr(id);
    ImGui::GetStateStorage()->SetVoidPtr(id, (void*)stack);
}

void gui_stack_pop(void)
{
    gui_stack_t *stack;
    ImGuiID id = g_current_gui_stack;
    assert(id);
    stack = (gui_stack_t*)ImGui::GetStateStorage()->GetVoidPtr(id);
    ImGui::GetStateStorage()->SetVoidPtr(id, (void*)stack->next);
    free(stack);
}

void gui_text_unformatted(const char *txt)
{
    ImGui::TextUnformatted(txt);
}

bool gui_button(const char *label, double size)
{
    return ImGui::Button(label, ImVec2(size, 0));
}

void gui_image(const char *url, int w, int h)
{
    texture_t *tex;
    tex = texture_from_url(url, 0);
    if (tex) {
        ImGui::Image((void*)tex, ImVec2(w, h),
                ImVec2(0, 0),
                ImVec2(tex->w / (float)tex->tex_w,
                       tex->h / (float)tex->tex_h));
    }
}

void gui_label(const char *label, const char *value)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    double x;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(style.FramePadding.x, 0));
    x = ImGui::GetCursorPosX();
    ImGui::Text("%s", label);
    ImGui::SameLine(x + 70); // XXX: to define.
    ImGui::Text("%s", value);
    ImGui::PopStyleVar();
}

bool gui_toggle(const char *label, bool *v)
{
    bool ret;
    gui_base_widget(label, NULL, 1);
    ret = ImGui::Checkbox("", v);
    gui_base_widget_end();
    return ret;
}

bool gui_link(const char *label, const char *sublabel)
{
    bool ret;
    ret = gui_base_widget(label, sublabel, 0.1, true);
    ImGui::Text("❯");
    gui_base_widget_end();
    return ret;
}

bool gui_int(const char *label, int *v)
{
    bool ret;
    gui_base_widget(label, NULL, 8);
    ret = ImGui::InputInt("##l", v);
    gui_base_widget_end();
    return ret;
}

bool gui_double(const char *label, double *v, double default_value)
{
    bool ret, b;
    double height;
    float f = *v;
    gui_base_widget(label, NULL, 10);

    if (!isnan(default_value) && isnan(f)) {
        f = ImGui::GetStateStorage()->GetFloat(
                ImGui::GetID("last-value"), default_value);
    }

    height = ImGui::CalcTextSize(label, NULL, true).y;
    ImGui::PushItemWidth(8.5 * height);
    ret = ImGui::InputFloat("##l", &f, 0.1, 1.0, 1);
    ImGui::PopItemWidth();

    if (!isnan(default_value)) {
        b = !isnan(*v);
        ImGui::SameLine();
        if (ImGui::Checkbox("", &b)) {
            if (b) {
                f = ImGui::GetStateStorage()->GetFloat(
                            ImGui::GetID("last-value"), default_value);
            } else {
                ImGui::GetStateStorage()->SetFloat(
                            ImGui::GetID("last-value"), f);
                f = NAN;
            }
            ret = true;
        }
    }

    if (ret) *v = f;
    gui_base_widget_end();
    return ret;
}

bool gui_input(const char *label, char *buffer, int len,
               const char **suggestions)
{
    bool ret;
    int flags = ImGuiInputTextFlags_EnterReturnsTrue;
    ImGui::PushID(label);
    ret = ImGui::InputText(label, buffer, len, flags);
    while (suggestions && *suggestions) {
        if (ImGui::Button(*suggestions)) {
            strncpy(buffer, *suggestions, len);
            ret = true;
            break;
        }
        suggestions++;
    }
    ImGui::PopID();
    return ret;
}

bool gui_input_multilines(const char *label, char *buf, int len)
{
    int flags = ImGuiInputTextFlags_CtrlEnterForNewLine |
                ImGuiInputTextFlags_EnterReturnsTrue;
    return ImGui::InputTextMultiline(label, buf, len, ImVec2(0, 0), flags);
}

void gui_separator(void)
{
    ImGui::Separator();
}

void gui_header(const char *label)
{
    ImGui::Separator();
    ImGui::Text("%s", label);
    ImGui::Separator();
}

void gui_same_line(void)
{
    ImGui::SameLine();
}

bool gui_date(double *v)
{
    int iy, im, id, ihmsf[4], r = 0, iy_new, im_new, id_new;
    double utc, djm0;

    utc = *v;
    eraD2dtf("UTC", 0, DJM0, utc, &iy, &im, &id, ihmsf);
    iy_new = iy;
    im_new = im;
    id_new = id;
    if (gui_int("Year", &iy_new)) r = 1;
    if (gui_int("Month", &im_new)) r = 1;
    if (gui_int("Day", &id_new)) r = 1;
    if (r) {
        r = eraDtf2d("UTC", iy_new, im_new, id_new,
                     ihmsf[0], ihmsf[1], ihmsf[2],
                     &djm0, &utc);
        if (r == 0) {
            *v = djm0 - DJM0 + utc;
            return true;
        } else {
            // We cannot convert to MJD.  This can happen if for example we set
            // the day to 0.  In that case we use the delta to the
            // previous value.
            *v += (iy_new - iy) * 365 +
                   (im_new - im) * 30 +
                   (id_new - id) * 1;
            return true;
        }
    }
    return false;
}

static ImVec2 make_size(const double size[2], const double parent[2])
{
    double parent_size[2];
    ImGuiIO& io = ImGui::GetIO();
    if (parent) vec2_copy(parent, parent_size);
    else vec2_set(parent_size, io.DisplaySize.x, io.DisplaySize.y);
    return ImVec2(size[0] >= 0 ? size[0] : parent_size[0] + size[0],
                  size[1] >= 0 ? size[1] : parent_size[1] + size[1]);
}

double gui_panel_begin(const char *name, const double pos[2],
                                         const double size_[2])
{
    ImGuiWindowFlags window_flags;
    double size[2];
    ImGuiIO& io = ImGui::GetIO();

    size[0] = size_[0];
    size[1] = size_[1] ?: io.DisplaySize.y;

    ImGui::SetNextWindowPos(make_size(pos, NULL));
    ImGui::SetNextWindowSize(make_size(size, NULL));
    window_flags = ImGuiWindowFlags_NoTitleBar |
                   ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoCollapse;
    ImGui::Begin(name, NULL, window_flags);
    ImGui::Columns(1);
    return make_size(size, NULL).x;
}

void gui_panel_end(void)
{
    ImGui::End();
}

void gui_card(const char *label, const char *img_url)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ChildWindowRounding, 5.0f);
    ImGui::BeginChild(label, ImVec2(0, 64), true);
    ImGui::BeginGroup();
}

bool gui_card_end(void)
{
    ImGui::EndGroup();
    // How to propery aligh right?
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x - 60, 0));
    ImGui::SameLine();
    gui_image("http://i.imgur.com/T5nYOAAs.jpg", 48, 48);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    return ImGui::IsItemClicked();
}

void gui_render_prepare(void)
{
    int i;
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(core->win_size[0], core->win_size[1]);
    io.DeltaTime = 1.0 / 60;
    io.MousePos = ImVec2(core->inputs.touches[0].pos[0],
                         core->inputs.touches[0].pos[1]);
    io.MouseDown[0] = core->inputs.touches[0].down[0];
    io.MouseDown[1] = core->inputs.touches[0].down[1];
    for (i = 0; i < ARRAY_SIZE(core->inputs.keys); i++)
        io.KeysDown[i] = core->inputs.keys[i];
    for (i = 0; i < ARRAY_SIZE(core->inputs.chars); i++) {
        if (!core->inputs.chars[i]) break;
        io.AddInputCharacter(core->inputs.chars[i]);
    }
    memset(core->inputs.chars, 0, sizeof(core->inputs.chars));

    ImGui::NewFrame();
}

void gui_render_finish(void)
{
    ImGuiIO& io = ImGui::GetIO();
    core->gui_want_capture_mouse = io.WantCaptureMouse;
    ImGui::Render();
}

#endif // __EMSCRIPTEN__
