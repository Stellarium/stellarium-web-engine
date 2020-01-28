
Stellarium Web Engine internals
===============================

# Files structure

    ext_src/            All the sources from dependencies.
    src/algos/          General astro algos, only depends on erfa and C std.
    src/modules/        All the modules (stars, milkyway, etc...)
    src/projections/    All the projections.
    src/utils/          Utility code.

    tools/              Various python scripts.
    html/               Minimal html website for testing the js version.
    data/               Data (need to be filled with tools/makedata.py).


# Code structure

Even though the code is in C, we are using a small object oriented structure,
with a single hierarchy where every class inherits from `obj_t`.

    +-------+
    | obj_t |
    +-------+
        ^
        |
        +----------------+---------------+--------------+-------------
        |                |               |              |
    +---------+   +------------+   +---------+   +------------+   +---
    | core_t  |   | observer_t |   | stars_t |   | milkyway_t |   | ...
    +---------+   +------------+   +---------+   +------------+   +---

The inheritance mechanism works by putting an `obj_t` attribute as first
attribute of a struct:

    struct my_obj_t {
        obj_t obj;
        // Other attributes
    };

Methods and class attributes are defined in an instance of `obj_klass_t`
(defined in obj.h)

For example to define a class for an object that can be rendered we would
declare a global instance of `obj_klass_t` and set the render method pointer
to a function with the proper signature:

    static int my_render(const obj_t *obj, const painter_t *painter);

    static obj_klass_t my_obj_klass = {
        .render = my_render,
    };

In order to create an new instance of an object of a given class, we have
to assign the klass pointer of the obj attribute to the proper klass
instance.  We  also have specify the size of the object, giving it an
id and a parent.  All of this is done by the function `obj_create`:

    obj_t *obj;
    obj = obj_create(&my_obj_klass, (int)sizeof(my_obj_t), "MyID", parent)

As a convenience we can also use the `OBJ_CREATE` macro:

    obj_t *obj;
    obj = OBJ_CREATE(my_obj, "MyID", parent);


Currently the methods we can define are:

    create      Create a new instance of the object.
    init        Called after create if specified.
    del         Called if the ref counter of the object goes to zero.
    update      Called at each iteration of the program.
    render      Called at each render.
    get         Used to find a sub object from a query string.
    gui         Used for the gui.
    list        List the sky object defined in a module.

There are two special cases of object: sky object and modules.

Sky objects represent an astro source.  They must implement the update
method, and it should set the values of the obj.pos struct:

    // equ, J2000.0, AU geocentric pos and speed.
    double pvg[2][3];
    // Can be set to INFINITY for stars that are too far to have a
    // position un AU.
    double unit;
    double g_ra, g_dec;
    double ra, dec;
    double az, alt;

Module objects are automatically created at startup, and must at least
define the `create` method.  In order to register a class as a module,
we have to put a `MODULE_REGISTER` macro in the module source file.
(Internally it uses gcc constructor attribute).  See for example
`src/modules/cardinal.c` for a simple module code.

# Attributes

In addition to the object methods and struct attribute, a class can also
define dynamic attributes.  This has several advantages:

- It can be accessed from JavaScript.
- We can assign listener callbacks that get called when an attribute changes.
- We can assign meta data to attributes, like documentation, or list of
  possible values, allowing to simplify gui generation.
- The attribute access API provide automatic type conversion, for example
  a position attribute can be set with a sky object.

In order to add attributes to an object, we must define the `attributes`
attribute of the `obj_klass_t` instance.  For example here is an attribute
of the observer class:

    ATTR("longitude", .offset = offsetof(observer_t, elong), .base = "f"),

In this simple case, we define an attribute 'longitude' that is a double
(base = "f') and is actually just an alias for the object structure attribute
`elong`.

We can now access this attribute like this:

    double elong;
    obj_call(obj, "longitude", ">f", &elong);

And set it like that:

    obj_call(obj, "longitude", "f", 1.4);

The syntax of `obj_call` is:

    int obj_call(obj_t *obj, const char *func, const char *sig, ...);

# Stack

Internally an attribute is actually a function operating on a stack.  In
the case of a simple attribute, the function first set the value if the
passed stack is not empty, and then push the value in the stack.

`obj_call` works by first creating the stack, then calling the function,
and finally getting the output values from the stack.

The signature argument has the form:

    INPUTS ['>' OUTPUTS]

Where inputs and outputs are a list of variables declaration:

    TYPE[size]

Types are:

    'i': int
    'f': double
    'b': boolean
    's': string
    'p': pointer

Instead of using `obj_call`, we could also manually manipulate the stack:

    const attribute_t *attr = obj_get_attr(obj, "longitude");
    stack_t *s;

    // Set the longitude:
    s = stack_create();
    stack_push_f(s, 1.4);
    attr->fn(obj, attr, s);
    stack_delete(s);

    // Get the longitude:
    s = stack_create();
    attr->fn(obj, attr, s);
    elong = stack_get_f(s, -1); // Last value on the stack.
    stack_delete(s);

The `stack_push` and `stack_get` are convenience functions that allow to
push or get several values from a stack in a single call using a signature
string.

In addition to the base type (int, double, string, boolean), we can also
assign an type name to a stack value to give it some semantic meaning.

All the types are defined in src/types:

    - `d_angle`
    - `h_angle`
    - `mjd`
    - `color`
    - `obj`
    - `v3`
    - `v3radec`
    - `v3altaz`
    - `dist`
    - `mag`

This is mostly used so that we can convert those types to string values.


# Assets manager

All the assets are compiled into the binary.  The tool makedata.py generates
the file src/assets.inl, that is included by assets.c.  The function
`asset_get_data` is used to retrieve any asset by url:

    asset_get_data("asset://some_asset", &size, &code)

As a convenience, if we use a normal url, the asset manager will try to load
the data using an http request or direct file access.

# Ephemeris algorithm

I try to use the erfa (open source version of the sofa library) as much as
possible.  All the code is in `ext_src/erfa/erfa.c`.

Erfa is used to compute:

    - Earth rotation and position.
    - Star positions.
    - Major planets positions.
    - Referential conversion matrices.
    - Time conversion

Some additional code not covered by erfa is in src/algos.  The rule is that
all the code in this dir should only depend on erfa and nothing else.

    - deltat computation (very basic for the moment).
    - moon position.
    - saturn ring.
    - some more time manipulation functions.
    - some convenience string format functions.

# Projections

A projection represents any function that goes from the 3D space to the
2D and/or reverse.  Some projections also support projecting to OpenGL clipping
space.

All the projections are represented by the type `projection_t`.
Currently we support the following projections:

    - Perspective
    - Stereographic
    - Mercator
    - Toast     (only backward)
    - Healpix   (only backward)

The signature of the projection function is:

    // Projection flags.
    enum {
        PROJ_NO_CLIP            = 1 << 0,
        PROJ_BACKWARD           = 1 << 1,
        PROJ_TO_NDC_SPACE       = 1 << 2,
        PROJ_ALREADY_NORMALIZED = 1 << 3,
    };

    bool project(const projection_t *proj, int flags,
                 int out_dim, const double *v, double *out);

Example of usages:

    // Project a 3d pos to clipping space:
    double pos[3] = {1, 0, 0};
    double out[4];
    project(proj, 0, 4, pos, out);

    // Optimization if we know the input is normalized:
    double pos[3] = {1, 0, 0};
    double out[4];
    project(proj, PROJ_ALREADY_NORMALIZED, 4, pos, out);

    // Project a 3d pos to NDC space (xyz divided by w):
    double pos[3] = {1, 0, 0};
    double out[3];
    project(proj, PROJ_TO_NDC_SPACE, 3, pos, out);

    // Project from a 2d pos back to the sphere.
    double pos[2] = {0.5, 0.5};
    double out[3];
    project(proj, PROJ_BACKWARD, 3, pos, out);


In addition we can check if a projecting a line or quad would intersects a
discontinuity:

    int projection_intersect_discontinuity(const projection_t *proj,
                                           double (*p)[3], int n);


In case of discontinuity some projections can implement a split method,
that returns two new projections that are similar to the original one but
with a different discontinuity position.


    P                         P1                     P2
    +---------------+         +---------------+      +---------------+
    |               |         |               |      |               |
    |--A         B--|      B--|--A            |      |            B--|--A
    |               |         |               |      |               |
    |               |         |               |      |               |
    +---------------+         +---------------+      +---------------+

    For example here we try to project a segment using a cylindrical
    projection P.  The projection returns the points A and B, but also tells
    us that we are facing an discontinuty.  We split the projection into
    P1 and P2, each of them projecting the segment A-B properly without
    discontinuty.


# Quad rendering

The function used to render a quad mapped into the sky sphere is `paint_quad`:

    int paint_quad(const painter_t *painter,
                   texture_t *tex,
                   double uv[4][2],
                   const projection_t *proj,
                   int grid_size);

We don't pass the actual 3d coordinates of the quad, but instead the quad
projected into the UV space and the projection function used to go from
the 3d space to the UV space.  That way there is no ambiguity as to the
exact position of each pixel in the texture.

The chain of coordinates transformations is as follow:

    Texture UV coordinates (2d, 0 to 1)
            |
            | Inverse projection `*proj` (for example healpix)
            |
            v
    3d model sphere coordinates (3d vector)
            |
            | Multiplication by 3x3 mat `painter->rm2h`
            | (default to identity)
            |
            v
    Horizontal cartesian coordinates (without refraction)
            |
            | Optional refraction computation.
            |
            v
    Observed cartesian coordinates
            |
            | Multiplication by 3x3 mat `painter->ro2v`.
            | default to the rotation for observer altitude and azimuth
            | plus Z up to Y up switch.
            |
            v
    OpenGL view coordinates
            |
            | Render projection `painter->proj`.
            | Usually set by the core depending on user preferences.
            | If there is any effects like refraction it is also applied.
            |
            v
    OpenGL clipping coordinates

Note that there are two projections used: the first one defines how the texture
itself projects into the sphere (argument `proj`), it depends on the texture
used.  The second one defines how to project the sphere into the screen.

Internally the `paint_quad`function will try to subdivide the original UV
surface into smaller parts so that we don't have projection errors, and
it also take care of discontinuities.  We can specify the number of
subdivisions we want with the `grid_size` argument.

# `traverse_surface` function

This high level function can be used to split an UV coordinate quad into
smaller tiles.  It is useful for hierarchical data structure used for example
in the stars module or healpix surveys.

        level 0          level 1          level 2
       +-----------+    +-----+-----+    +--+--+--+--+
       |           |    |     |     |    |  |  |  |  |
       |           |    |     |     |    +--+--+--+--+
       |           |    |     |     |    |  |  |  |  |
       |           |    +-----+-----+    +--+--+--+--+
       |           |    |     |     |    |  |  |  |  |
       |           |    |     |     |    +--+--+--+--+
       |           |    |     |     |    |  |  |  |  |
       +-----------+    +-----+-----+    +--+--+--+--+

The function takes a callback, and call it for each tile at every level,
starting from 0.  We can control how deep we go, if we filter non visible
tiles, and if we handle discontinuities split using the return value of the
callback.

The way it works is that for each tile the function will first check if
it is visible, then check if its projection intersects a discontinuity,
in which case it attempts to split the projection.

The callback get called at every stage of the algorithm, and can decide to:

- stop the tests of this tile and not go to deeper tiles (return 0).
- stop the tests, but go to deeper tiles (return 1).
- keep the normal test process, and go to deeper tiles (return 2).
- totally abort the traversal of all tiles (return 3).


Example of usages:

To visit all the tiles up to a certain level, no matter if they are visible
or not:

    if (node->level > max_level) return 0;
    ...
    return 1;

To visit all the visible tiles, up to a certain level, but ignore the
discontinuities (for example used to render the stars tiles).

    if (step == 0) return 2; // Check for visibiliy and call again if so.
    if (node->level > max_level) return 0; // Stop here.
    ...
    return 1;

To visit all the visible tiles, and also take care of the discontinuities
(used in the hips module):

    if (step < 2) return 2; // Check both visible and discontinuities.
    if (node->level > max_level) return 0; // Stop here.
    ...
    return 1;

