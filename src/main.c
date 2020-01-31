/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"

#ifndef __EMSCRIPTEN__ // No need for main in emscripten

#ifndef NDEBUG
#   include <fenv.h>
#endif

#ifdef GLES2
#   define GLFW_INCLUDE_ES2
#endif
#include <GLFW/glfw3.h>

#if DEBUG
#   define DEBUG_ONLY(x) x
#else
#   define DEBUG_ONLY(x)
#endif

static GLFWwindow   *g_window = NULL;

typedef struct
{
    bool run_tests;
    char *tests_filter;
    bool calendar;
    bool gen_doc;
    char *args[3];
} args_t;

#if !DEFINED(NO_ARGP)
#include <argp.h>

const char *argp_program_version = "swe " SWE_VERSION_STR;
static char doc[] = "A virtual planetarium";
static char args_doc[] = "";
#define OPT_RUN_TESTS 1
#define OPT_GEN_DOC 2
static struct argp_option options[] = {

#if COMPILE_TESTS
    {"run-tests", OPT_RUN_TESTS, "filter", OPTION_ARG_OPTIONAL,
                                                    "Run the unit tests" },
#endif
    {"calendar", 'c', NULL, 0, "print events calendar"},
    {"gen-doc", OPT_GEN_DOC, NULL, 0, "print doc for the defined classes"},
    { 0 }
};

/* Parse a single option. */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    args_t *args = state->input;
    switch (key)
    {
    case OPT_RUN_TESTS:
        args->run_tests = true;
        args->tests_filter = arg;
        break;
    case OPT_GEN_DOC:
        args->gen_doc = true;
        break;
    case 'c':
        args->calendar = true;
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= 3)
            argp_usage (state);
        args->args[state->arg_num] = arg;
        break;
    case ARGP_KEY_END:
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };
#endif

static void run_main_loop(void (*func)(void));
static void loop_function(void);

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    core_on_mouse(0, state == GLFW_PRESS ? 1 : 0, xpos, ypos);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action,
                           int mods)
{
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    core_on_mouse(0, action == GLFW_PRESS ? 1 : 0, xpos, ypos);
}

static void key_callback(GLFWwindow* window, int key, int scancode,
                  int action, int mods)
{
    core_on_key(key, action);
}

void char_callback(GLFWwindow *win, unsigned int c)
{
    core_on_char(c);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    const double ZOOM_FACTOR = 1.1;
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    core_on_zoom(pow(ZOOM_FACTOR, yoffset), xpos, ypos);
}


static void add_source(const char *module, const char *url, const char *key)
{
    obj_t *m = NULL;
    if (module) {
        m = core_get_module(module);
        assert(m);
    }
    module_add_data_source(m, url, key);
}

void add_default_sources(void) __attribute__ ((weak));
void add_default_sources(void)
{
    #define BASE "data/skydata/"

    add_source("landscapes", BASE "landscapes/guereins", "guereins");

    // Bundled star survey.
    add_source("stars", BASE "stars", NULL);
    // DSO survey.
    add_source("dsos", BASE "dso", NULL);
    add_source("skycultures", BASE "skycultures/western", "western");
    add_source("milkyway", BASE "surveys/milkyway", "hips");

    // All the planets.
    add_source("planets", BASE "surveys/sso/moon", "default");
    add_source("planets", BASE "surveys/sso/moon", "moon");
    add_source("planets", BASE "surveys/sso/sun", "sun");

    // MPC data.
    add_source("minor_planets", "asset://mpcorb.dat", "mpc_asteroids");
    add_source("comets", BASE "CometEls.txt", "mpc_comets");

    // Artificial satellites files.
    add_source("satellites", BASE "tle_satellite.jsonl.gz", "jsonl/sat");

    #undef BASE
}

int main(int argc, char **argv)
{
    int w = 800, h = 600;
    const char *title =
                "Stellarium Web Engine "
                SWE_VERSION_STR DEBUG_ONLY(" (debug)");

#if DEBUG
    // Make sure that we don't rely on NAN or INFINITY in our computing
    feenableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW);
#endif

    args_t args = {};
#if !DEFINED(NO_ARGP)
    argp_parse (&argp, argc, argv, 0, 0, &args);
#endif
    if (args.calendar) {
        core_init(0, 0, 1);
        add_default_sources();
        calendar_print();
        return 0;
    }
    if (args.gen_doc) {
        swe_gen_doc();
        return 0;
    }

    if (args.run_tests) {
        tests_run(args.tests_filter);
        return 0;
    }

    glfwInit();
    glfwWindowHint(GLFW_SAMPLES, 2);
    g_window = glfwCreateWindow(w, h, title, NULL, NULL);
    glfwMakeContextCurrent(g_window);

    glfwSetKeyCallback(g_window, key_callback);
    glfwSetCharCallback(g_window, char_callback);
    glfwSetCursorPosCallback(g_window, cursor_pos_callback);
    glfwSetMouseButtonCallback(g_window, mouse_button_callback);
    glfwSetScrollCallback(g_window, scroll_callback);
    glfwSetInputMode(g_window, GLFW_STICKY_MOUSE_BUTTONS, false);

    int fb_size[2];
    glfwGetFramebufferSize(g_window, &fb_size[0], &fb_size[1]);

    core_init(fb_size[0], fb_size[1], 1.0);
    add_default_sources();

    if (DEFINED(COMPILE_TESTS)) {
        tests_run("auto"); // Run all the automatic tests.
        // Reinit the core to default.
        core_init(fb_size[0], fb_size[1], 1.0);
    }

    run_main_loop(loop_function);
    core_release();

    return 0;
}

static void loop_function(void)
{
    int fb_size[2];
    double dt = 1.0 / 60.0; // Assume fixed 60fps.
    glfwGetFramebufferSize(g_window, &fb_size[0], &fb_size[1]);

    core_update(dt);
    core_render(fb_size[0], fb_size[1], 1.0);
    glfwSwapBuffers(g_window);
    glfwPollEvents();
}

static void run_main_loop(void (*func)(void))
{
    while (!glfwWindowShouldClose(g_window)) {
        func();
    }
    glfwTerminate();
}

#endif
