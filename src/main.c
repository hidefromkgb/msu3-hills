/**
  @mainpage
  This is a random terrain generator and viewer in pure Win32 API / OpenGL.\n
  It features "endless" maps, smooth Gouraud shading, exponential distance
  fog, facet textures, water reflections and serialization.

  The architecture is quite simple:
  1. When the program starts, it looks for the config file (conf.txt) and
     performs deserialization. If there is no file, a new map is generated.
  2. To serialize to binary format, specify an empty file (with any extension
     except .txt) in parameters, or drag this file onto the main window.
  3. By pressing [SPACE] you can regenerate the map, which is automatically
     serialized.
  4. The map is generated using the diamond-square algorithm.

  [ENTER] resets the orientation of the camera.\n
  Use [W]/[A]/[S]/[D] and mouse (with the left button pressed) to move.

  By pressing [Z]/[X]/[C]/[V]/[B]/[N], you can toggle various drawing modes:

  &nbsp;&nbsp;&nbsp;&nbsp;[Z]: Vertex arrays / VBO\n
  &nbsp;&nbsp;&nbsp;&nbsp;[X]: Wireframe / filled polygons\n
  &nbsp;&nbsp;&nbsp;&nbsp;[C]: Shading on / off\n
  &nbsp;&nbsp;&nbsp;&nbsp;[V]: Coloring on / off\n
  &nbsp;&nbsp;&nbsp;&nbsp;[B]: Texturing on / off\n
  &nbsp;&nbsp;&nbsp;&nbsp;[N]: Objects on / off\n
**/



#include <time.h>
#include <math.h>
#include <stdio.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <windows.h>
#include <mmsystem.h>



/**
  @brief tr: inline function, used mainly to truncate FLOAT into LONG.
  Is obviously faster than trunc().

  @param f - something that tr() shall convert into LONG.

  @return f converted into LONG. /Thank you, captain Obvious!/
**/
#define tr(f) ((LONG)(f))

/**
  @brief frand: inline function that generates a random value between -f and f.

  @param f - defines the range that the value generated shall fall into.

  @return random value from the range.
**/
#define frand(f) (2.0 * ((FLOAT)(rand() & 0x7FFF) / (FLOAT)0x7FFF) * (FLOAT)(f) - (FLOAT)(f))


/**
  GL_ARRAY_BUFFER_ARB
  - OpenGL constant not present in any header shipped with Code::Blocks.
**/
#define GL_ARRAY_BUFFER_ARB 0x8892
/**
  GL_INDEX_BUFFER_ARB
  - OpenGL constant not present in any header shipped with Code::Blocks.
**/
#define GL_INDEX_BUFFER_ARB 0x8893
/**
  GL_STATIC_DRAW_ARB
  - OpenGL constant not present in any header shipped with Code::Blocks.
**/
#define GL_STATIC_DRAW_ARB  0x88E4


/// USE_NONE - we don`t want our VBO to be capable of anything.
#define USE_NONE 0
/// USE_ARBV - let the VBO be drawn through ARB OpenGL extensions.
#define USE_ARBV (1 << 0)
/// USE_FILL - use filled polygons instead of wireframe mesh.
#define USE_FILL (1 << 1)
/// USE_NORM - enable normals for better shading computation.
#define USE_NORM (1 << 2)
/// USE_TEXC - cover the VBO with a microfacet texture.
#define USE_TEXC (1 << 3)
/// USE_CLRS - make the VBO colored.
#define USE_CLRS (1 << 4)
/// USE_OBJS - create some objects resting on the surface.
#define USE_OBJS (1 << 5)

/// DEG_CRAD - converts degrees into radians.
#define DEG_CRAD (M_PI / 180.0)
/// RAD_CDEG - converts radians into degrees.
#define RAD_CDEG (180.0 / M_PI)

/// DEF_TMRP - repainting timer delay.
#define DEF_TMRP 16
/// DEF_TMRC - recomputing timer delay.
#define DEF_TMRC 32

/// DEF_FANG - accuracy coefficient for rotation; accuracy * speed = const.
#define DEF_FANG 0.5
/// DEF_FTRN - accuracy coefficient for movement; accuracy * speed = const.
#define DEF_FTRN 15.0

/// DEF_FFOV - field of view (perspective coefficient).
#define DEF_FFOV 45.0
/// DEF_ZNEA - near clipping plane (perspective coefficient).
#define DEF_ZNEA 0.1
/// DEF_ZFAR - far clipping plane (perspective coefficient).
#define DEF_ZFAR 8000.0

/// DEF_LPWR - log2 of the total number of unique vertices in the landscape VBO.
#define DEF_LPWR 7
/// DEF_GRID - size of elemental squares that build our landscape.
#define DEF_GRID 16.0
/// DEF_FHEI - landscape height multiplier; peak height is DEF_FHEI / 2.
#define DEF_FHEI 600.0
/// DEF_WLVL - "sea level", i.e. minimal height; should be > -DEF_FHEI / 2.
#define DEF_WLVL (-0.25 * DEF_FHEI)

/// DEF_ANGU - default camera direction, U component
#define DEF_ANGU   0.0
/// DEF_ANGV - default camera direction, V component
#define DEF_ANGV -60.0

/// DEF_TRNX - default camera position, X component
#define DEF_TRNX   0.0
/// DEF_TRNY - default camera position, Y component
#define DEF_TRNY   0.0
/// DEF_TRNZ - default camera position, Z component
#define DEF_TRNZ (-0.5 * DEF_FHEI)

/// DEF_DIRX - default light direction, X component
#define DEF_DIRX   0.0
/// DEF_DIRY - default light direction, Y component
#define DEF_DIRY   0.0
/// DEF_DIRZ - default light direction, Z component
#define DEF_DIRZ  -1.0

/// DEF_POSX - default light position, X component
#define DEF_POSX   0.0
/// DEF_POSY - default light position, Y component
#define DEF_POSY   0.0
/// DEF_POSZ - default light position, Z component
#define DEF_POSZ (10.0 * DEF_FHEI)

/// DEF_FEXL - length of the default extension.
#define DEF_FEXL 4
/// DEF_FEXT - default extension for the file.
#define DEF_FEXT ".txt"
/// DEF_FILE - default file to serialize the map to and from.
#define DEF_FILE "conf"DEF_FEXT
/// DEF_FRMT - default serialization format.
#define DEF_FRMT "%u %u %f %f %f %f %f %f %f %f %f %f %f"

/// DEF_NOBJ - default number of objects.
#define DEF_NOBJ 50



#pragma pack(push, 1)
/**
  @struct FCLR
  The color of a specified vertex in RGBA8888 format.
**/
typedef union _FCLR {
    struct {
        BYTE R, G, B, A;
    };
    DWORD RGBA;
} FCLR;

/**
  @struct FTRI
  The layout of 4 triangles comprising an elemental square:

  @verbatim
    ...                                        ___                ___
  | /  \ | /             \ |                 i\   /h         [2n]\   /[2n+1]
  [2n]--[2n+1]-   ...  -[3n-1]             k   \ /   f     [2n]   \ /   [2n+1]
  | \  / | \          \  / |              | \   g   / |      | \  [n]  / |
  | [n]  | [n+1]  ...[2n-2]|  [2n-1]  ->  |  j     d  |  ->  | [n]   [n] |
  | /  \ | /  \ |     /  \ |              | /   a   \ |      | /  [n]  \ |
  0------1------2-...  --[n-1]             l   / \   e        0   / \   1
                                             b/___\c            0/___\1
  @endverbatim

  All FTRI members are indices into the vertex array (see leftmost picture).\n
  Used for VBO element array.
**/
typedef struct _FTRI {
    UINT a, b, c,
         d, e, f,
         g, h, i,
         j, k, l;
} FTRI;

/**
  @struct FVEC
  A vector (or simply a point) in 3D space.\n
  Used in vertex / normal arrays, and other places where vectors are needed.
**/
typedef struct _FVEC {
    FLOAT x, y, z;
} FVEC;

/**
  @struct FTEX
  A pair of planar texture coordinates.
  Used for texture coordinate array.\n
  Also used for holding the horizontal and vertical
  viewing angles, since they also are just two FLOATs.
**/
typedef struct _FTEX {
    FLOAT u, v;
} FTEX;
#pragma pack(pop)

/**
  @struct FVBO
  The big fat structure that holds the components of a VBO.
**/
typedef struct _FVBO {
    /// optional VBO that may contain additional objects or LODs.
    struct _FVBO *next;

    /** what shall be displayed:\n
        &nbsp;&nbsp;&nbsp;&nbsp;USE_NONE: just vertices and indices\n
        &nbsp;&nbsp;&nbsp;&nbsp;USE_ARBV: +ARB VBO\n
        &nbsp;&nbsp;&nbsp;&nbsp;USE_FILL: +filled polygons\n
        &nbsp;&nbsp;&nbsp;&nbsp;USE_NORM: +normals\n
        &nbsp;&nbsp;&nbsp;&nbsp;USE_TEXC: +texture coordinates\n
        &nbsp;&nbsp;&nbsp;&nbsp;USE_CLRS: +colors\n
        &nbsp;&nbsp;&nbsp;&nbsp;USE_OBJS: +objects
    **/
    UINT flgs;
    /// horizontal and vertical dimension of the landscape map.
    UINT ndim;
    /// number of separate control points defining the map.
    UINT ndot;
    /// number of polygons; shall be passed to glDrawElements().
    UINT npol;
    /// ID of the facet texture associated with the landscape.
    UINT ntex;
    /// PRNG seed that was used to create the map.
    UINT seed;

    /// VBO ID for indices.
    UINT iind;
    /// VBO ID for vertices.
    UINT ivec;
    /// VBO ID for normals.
    UINT inrm;
    /// VBO ID for colors.
    UINT iclr;
    /// VBO ID for texture coords.
    UINT itex;

    /// width and height of the whole map grid.
    FLOAT grid;
    /// lowest point on the map; "sea level".
    FLOAT wlvl;

    /// array with indices.
    FTRI *indx;
    /// array with vertices.
    FVEC *vect;
    /// array with normals.
    FVEC *norm;
    /// array with colors.
    FCLR *clrs;
    /// array with texture coords.
    FTEX *texc;
} FVBO;

/**
  @struct FHEI
  The special structure used for height-based coloring.

  FHEIs are to be stored in an array:\n
  (all heights except the last zero are given as an example)

  @verbatim
  - FHEI[N]: fhei = 0.0 (end-of-an-array mark), fclr = [color of the water]
  -
  |
    ...
  |
  -
  |
  | FHEI[2]: fhei = 3.0, fclr = [color of the second level]
  |
  -
  | FHEI[1]: fhei = 1.0, fclr = [color of the level just above the lowest]
  -
  |
  | FHEI[0]: fhei = 2.0, fclr = [color of the lowest non-water level]
  -
  @endverbatim

  During the map creation, all heights are (by default) summed
  and then the sum is interpolated upon the real map height.

  NOTICE: all colors, except that of water, ignore the alpha component.
**/
typedef struct _FHEI {
    /// relative height (see above).
    FLOAT fhei;
    /// associated color.
    FCLR fclr;
} FHEI;



/// Main GDI device context
HDC DC;
/// Main OpenGL rendering context
HGLRC RC;
/// Quasi-mutex to prohibit drawing
BOOL paint;
/// Path to the config file
LPSTR path;
/// ID of a timer that recomputes the scene
UINT tmrc;
/// ID of a timer that repaints the scene
UINT tmrp;
/// Prevoius mouse position
POINT angp;
/// Current mouse position
POINT movp;

/// Camera position
FVEC ftrn;
/// Camera direction
FTEX fang;
/// Light position
FLOAT lpos[4];
/// Light direction
FLOAT ldir[4];
/// Main landsape VBO
FVBO *land = NULL;
/// Array that holds keystrokes
BOOL keys[256] = {};
/// Previous frame timestamp
UINT tick = 0;
/// Number of frames drawn
UINT fram = 0;

/// This function is to be loaded manually, since its implementation may vastly depend on the pixel format.
void CALLBACK (*glGenBuffersARB)(GLsizei, GLuint*) = NULL;
/// This function is to be loaded manually, since its implementation may vastly depend on the pixel format.
void CALLBACK (*glBindBufferARB)(GLenum, GLuint);
/// This function is to be loaded manually, since its implementation may vastly depend on the pixel format.
void CALLBACK (*glBufferDataARB)(GLenum, GLsizei, const void*, GLenum);
/// This function is to be loaded manually, since its implementation may vastly depend on the pixel format.
void CALLBACK (*glDelBuffersARB)(GLsizei, const GLuint*);



/**
  @brief MakeFacetTex
  creates a microfacet texture containing a white noise pattern.

  @param rndc - absolute value defines the amplitude of white noise, (0; 256].
                sign shows if the texture needs to be transparent; < 0 == yes.

  @return texture ID on success, 0 on failure (either by OGL or if rndc == 0).
**/
UINT MakeFacetTex(LONG rndc) {
    UINT x, y, dpos, itex;
    BOOL trns = rndc < 0;
    FCLR *ctex;

    if (!(rndc = abs(rndc) % 257)) return 0;

    #define TEX_LPWR 8
    itex = pow(2.0, TEX_LPWR);
    ctex = (FCLR*)malloc(itex * itex * sizeof(FCLR));

    if (trns) {
        for (y = 0; y < itex; y++)
            for (dpos = itex + (x = y * itex); x < dpos; x++) {
                ctex[x].R = ctex[x].G = ctex[x].B = 255;
                ctex[x].A = (rand() % rndc) - rndc;
            }
    }
    else {
        for (y = 0; y < itex; y++)
            for (dpos = itex + (x = y * itex); x < dpos; x++) {
                ctex[x].R = ctex[x].G = ctex[x].B = (rand() % rndc) - rndc;
                ctex[x].A = 255;
            }
    }

    glGenTextures(1, &dpos);
    glBindTexture(GL_TEXTURE_2D, dpos);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gluBuild2DMipmaps(GL_TEXTURE_2D, sizeof(FCLR), itex, itex, GL_RGBA, GL_UNSIGNED_BYTE, ctex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NICEST);
    free(ctex);
    #undef TEX_LPWR

    return dpos;
}



/**
  @brief MakeHeightmap
  generates a random NxN heightmap, where N shall be a power of 2.
  The method used is the so-called "diamond-square" algorithm.

  @param size - N (discussed above).
  @param dmpf - the "sharpness" of the surface; shan`t be zero.

  @return 1D array on success, NULL on failure (N != 2**K, where K is natural).
**/
FLOAT *MakeHeightmap(UINT size, FLOAT dmpf) {
    if ((size & (size - 1)) || (size == 1)) return NULL;

    LONG x, y, xbgn, xend, ybgn, yend, oddc, step, sinc = size + 1;
    FLOAT hdef, *farr = (FLOAT*)calloc(sinc * sinc, sizeof(FLOAT));
    hdef = dmpf = pow(2.0, -fabs(dmpf));

    for (step = size >> 1; step; step >>= 1, hdef *= dmpf) {
        for (y = step; y < size; y += step << 1)
            for (x = step; x < size; x += step << 1)
                farr[x + y * sinc] = hdef * frand(0.500)
                           + 0.250 *(farr[(x - step) + (y - step) * sinc]
                                   + farr[(x + step) + (y - step) * sinc]
                                   + farr[(x - step) + (y + step) * sinc]
                                   + farr[(x + step) + (y + step) * sinc]);

        for (ybgn = yend = oddc = y = 0; y < size; ybgn = yend = y += step, oddc = ~oddc) {
            if (y == 0) yend = size; else if (y == size) ybgn = 0;
            for (xbgn = xend = x = (oddc)? 0 : step; x < size; xbgn = xend = x += step << 1) {
                if (x == 0) xend = size; else if (x == size) xbgn = 0;
                farr[x + y * sinc] = hdef * frand(0.500)
                           + 0.250 *(farr[(xend - step) + y * sinc]
                                   + farr[(xbgn + step) + y * sinc]
                                   + farr[x + (yend - step) * sinc]
                                   + farr[x + (ybgn + step) * sinc]);
                if (x == 0) farr[size + y * sinc] = farr[0 + y * sinc];
                if (y == 0) farr[x + size * sinc] = farr[x + 0 * sinc];
            }
        }
    }
    return farr;
}



/**
  @brief BlurHeightmap
  makes the heightmap look less edgy, by smoothing it with Gaussian blur.

  @param farr - the heightmap to be blurred.
  @param size - width and height of the heightmap.
  @param fsig - strength of smoothing; [0, size).
**/
void BlurHeightmap(FLOAT *farr, UINT size, FLOAT fsig) {
    FLOAT *blur, *ftmp, fsum;
    LONG x, y, z, dpos;

    if (!(fsig = fabs(fsig) * 3.0)) return;
    if (size <= 0 || fsig >= size) return;

    fsum = 9.0 / (2.0 * fsig * fsig);
    blur = (FLOAT*)calloc(tr(fsig) + 1, sizeof(FLOAT));

    for (x = tr(fsig); x > 0; x--)
        blur[0] += blur[x] = exp(-x * x * fsum);

    blur[0] = 0.5 / (blur[0] + 0.5);
    for (x = tr(fsig); x > 0; x--)
        blur[x] *= blur[0];

    ftmp = (FLOAT*)malloc((size + 1) * (size + 1) * sizeof(FLOAT));
    for (y = 0; y <= size; y++)
        for (dpos = y * (size + 1), x = 0; x <= size; x++) {
            for (fsum = 0.0, z = tr(fsig); z > 0; z--)
                fsum += (farr[dpos + ((x - z < 0)?    x - z + size : x - z)]  +
                         farr[dpos + ((x + z > size)? x + z - size : x + z)]) * blur[z];
            ftmp[dpos + x] = ftmp[dpos + x] * blur[0] + fsum;
        }
    for (x = 0; x <= size; x++)
        for (y = 0; y <= size; y++) {
            for (fsum = 0.0, z = tr(fsig); z > 0; z--)
                fsum += (ftmp[x + ((y - z < 0)?    (y - z + size) * (size + 1) : (y - z) * (size + 1))]  +
                         ftmp[x + ((y + z > size)? (y + z - size) * (size + 1) : (y + z) * (size + 1))]) * blur[z];
            farr[x + y * (size + 1)] = farr[x + y * (size + 1)] * blur[0] + fsum;
        }
    free(ftmp);
    free(blur);
}



/**
  @brief MakeVBO
  creates an empty VBO placeholder.

  @param ndot - number of separate vertices in the VBO; must be > 0.

  @return FVBO struct on success, NULL on failure (ndot == 0).
**/
FVBO *MakeVBO(UINT ndot) {
    if (!ndot) return NULL;
    FVBO *retn = (FVBO*)calloc(1, sizeof(FVBO));

    retn->ndot = ndot;
    retn->indx = (FTRI*)calloc(1 + (ndot >> 1), sizeof(FTRI));
    retn->vect = (FVEC*)calloc(ndot, sizeof(FVEC));
    retn->norm = (FVEC*)calloc(ndot, sizeof(FVEC));
    retn->texc = (FTEX*)calloc(ndot, sizeof(FTEX));
    retn->clrs = (FCLR*)calloc(ndot, sizeof(FCLR));

    if (glGenBuffersARB) {
        glGenBuffersARB(1, &retn->iind);
        glBindBufferARB(GL_INDEX_BUFFER_ARB, retn->iind);
        glGenBuffersARB(1, &retn->ivec);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, retn->ivec);
        glGenBuffersARB(1, &retn->inrm);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, retn->inrm);
        glGenBuffersARB(1, &retn->itex);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, retn->itex);
        glGenBuffersARB(1, &retn->iclr);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, retn->iclr);
    }
    return retn;
}



/**
  @brief DrawVBO
  renders the VBO using OpenGL commands.

  @param vobj - VBO to be rendered; must be a valid FVBO pointer.
**/
void DrawVBO(FVBO *vobj) {
    if (!vobj) return;

    if (vobj->flgs & USE_FILL)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    if (!(vobj->flgs & USE_CLRS))
        glColor4ub(255, 255, 255, 255);

    glEnableClientState(GL_VERTEX_ARRAY);
    if (vobj->flgs & USE_ARBV) {
        glBindBufferARB(GL_INDEX_BUFFER_ARB, vobj->iind);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, vobj->ivec);
        glVertexPointer(3, GL_FLOAT, 0, 0);
        if (vobj->flgs & USE_NORM) {
            glEnableClientState(GL_NORMAL_ARRAY);
            glBindBufferARB(GL_ARRAY_BUFFER_ARB, vobj->inrm);
            glNormalPointer(GL_FLOAT, 0, 0);
        }
        if (vobj->flgs & USE_TEXC) {
            glEnable(GL_TEXTURE_2D);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glBindTexture(GL_TEXTURE_2D, vobj->ntex);
            glBindBufferARB(GL_ARRAY_BUFFER_ARB, vobj->itex);
            glTexCoordPointer(2, GL_FLOAT, 0, 0);
        }
        if (vobj->flgs & USE_CLRS) {
            glEnableClientState(GL_COLOR_ARRAY);
            glBindBufferARB(GL_ARRAY_BUFFER_ARB, vobj->iclr);
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);
        }
        glDrawElements(GL_TRIANGLES, vobj->npol, GL_UNSIGNED_INT, 0);
        glBindBufferARB(GL_INDEX_BUFFER_ARB, 0);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    }
    else {
        glVertexPointer(3, GL_FLOAT, 0, vobj->vect);
        if (vobj->flgs & USE_NORM) {
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT, 0, vobj->norm);
        }
        if (vobj->flgs & USE_TEXC) {
            glEnable(GL_TEXTURE_2D);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glBindTexture(GL_TEXTURE_2D, vobj->ntex);
            glTexCoordPointer(2, GL_FLOAT, 0, vobj->texc);
        }
        if (vobj->flgs & USE_CLRS) {
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, vobj->clrs);
        }
        glDrawElements(GL_TRIANGLES, vobj->npol, GL_UNSIGNED_INT, vobj->indx);
    }
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_TEXTURE_2D);

    if (!vobj->next)
        vobj->flgs &= ~USE_OBJS;
    else if (vobj->flgs & USE_OBJS) {
        vobj->next->flgs = (vobj->next->flgs & USE_OBJS)? vobj->flgs : vobj->flgs & ~USE_OBJS;
        DrawVBO(vobj->next);
    }
}



/**
  @brief FreeVBO
  frees a VBO. /Captain, is it you again? ^_^/

  @param vobj - pointer to the location where the target FVBO is stored.
**/
void FreeVBO(FVBO **vobj) {
    if (vobj && *vobj) {
        FreeVBO(&(*vobj)->next);
        if (glGenBuffersARB) {
            glDelBuffersARB(1, &(*vobj)->iind);
            glDelBuffersARB(1, &(*vobj)->ivec);
            glDelBuffersARB(1, &(*vobj)->inrm);
            glDelBuffersARB(1, &(*vobj)->itex);
            glDelBuffersARB(1, &(*vobj)->iclr);
        }
        glDeleteTextures(1, &(*vobj)->ntex);
        free((*vobj)->indx);
        free((*vobj)->vect);
        free((*vobj)->norm);
        free((*vobj)->texc);
        free((*vobj)->clrs);
        free(*vobj);
        vobj = NULL;
    }
}



/**
  @brief CamLightReset
  resets camera and light source orientation to their default values.
**/
void CamLightReset() {
    fang.u = DEF_ANGU;
    fang.v = DEF_ANGV;

    ftrn.x = DEF_TRNX;
    ftrn.y = DEF_TRNY;
    ftrn.z = DEF_TRNZ;

    ldir[0] = DEF_DIRX;
    ldir[1] = DEF_DIRY;
    ldir[2] = DEF_DIRZ;
    ldir[3] = 1.0;

    lpos[0] = DEF_POSX;
    lpos[1] = DEF_POSY;
    lpos[2] = DEF_POSZ;
    lpos[3] = 1.0;
}



/**
  @brief Serialize
  saves VBO creation parameters to a file.

  @param file - file into which the VBO shall be serialized.
  @param vobj - location where the target FVBO is stored.
**/
void Serialize(LPSTR file, FVBO *vobj) {
    FILE *filp;

    if ((filp = fopen(file, "wb"))) {
        if ((file = strstr(file, DEF_FEXT)) && (strlen(file) == DEF_FEXL)) {
            fprintf(filp, DEF_FRMT, vobj->seed, vobj->flgs,
                    fang.u,  fang.v,
                    ftrn.x,  ftrn.y,  ftrn.z,
                    ldir[0], ldir[1], ldir[2],
                    lpos[0], lpos[1], lpos[2]);
        }
        else {
            fwrite(&vobj->seed, sizeof(UINT), 1, filp);
            fwrite(&vobj->flgs, sizeof(UINT), 1, filp);
            fwrite(&fang, sizeof(FTEX), 1, filp);
            fwrite(&ftrn, sizeof(FVEC), 1, filp);
            fwrite(ldir, sizeof(FVEC), 1, filp);
            fwrite(lpos, sizeof(FVEC), 1, filp);
        }
        fclose(filp);
    }
}



/**
  @brief ObjectVBO
  generates a VBO filled with additional objects for a "parent" landscape VBO.

  @param vobj - Parent VBO; the objects shall be situated on its surface.
  @param inum - number of objects to create; may be overridden in case it
                exceeds the count of spots that can actually hold an object.

  @return FVBO on success, NULL on failure (vobj == NULL or inum == 0).
**/
FVBO *ObjectVBO(FVBO *vobj, UINT inum) {
    FVEC fbgn, fend, fv00, fv01, fv10, fv11;
    LONG x, y, dpos, xl, xh, yh;
    UINT *fobj, *farr;
    FVBO *retn;

    if (!vobj || !inum) return NULL;

    for (xh = y = 0; y < vobj->ndim; y++)
        for (dpos = vobj->ndim + (x = (vobj->ndim + 1) + (y * (vobj->ndim + 1) << 1)); x < dpos; x++)
            if (vobj->vect[x].z > vobj->wlvl) xh++;

    fobj = (UINT*)malloc((1 + (xl = min(xh, inum))) * sizeof(UINT));
    fobj[0] = xl;
    farr = (UINT*)malloc(xh * sizeof(UINT));

    yh = xh;
    for (y = 0; y < vobj->ndim; y++)
        for (dpos = vobj->ndim + (x = (vobj->ndim + 1) + (y * (vobj->ndim + 1) << 1)); x < dpos; x++)
            if (vobj->vect[x].z > vobj->wlvl)
                farr[--yh] = x;

    for (; xl > 0; xl--) {
        x = rand() % xh;
        fobj[xl] = farr[x];
        farr[x] = farr[--xh];
    }
    free(farr);

    retn = MakeVBO(3 * 5 * fobj[0]);
    x = 64;
//    x = -256;
    retn->ntex = MakeFacetTex(x);
    retn->grid = vobj->grid / (FLOAT)vobj->ndim;

    #define FIR_TTEX  0.25
    #define FIR_SIZE  0.75
    #define FIR_FADE (0.25 * FIR_SIZE)
    for (x = fobj[0] - 1; x >= 0; x--) {
        fv00 = vobj->vect[fobj[x + 1] - (vobj->ndim + 1) + 0];
        fv01 = vobj->vect[fobj[x + 1] - (vobj->ndim + 1) + 1];
        fv11 = vobj->vect[fobj[x + 1] + (vobj->ndim + 1) + 1];
        fv10 = vobj->vect[fobj[x + 1] + (vobj->ndim + 1) + 0];

        fbgn = vobj->vect[fobj[x + 1]];
        fend = vobj->norm[fobj[x + 1]];

        fend.x *= 0.5 * retn->grid;
        fend.y *= 0.5 * retn->grid;
        fend.z *= 0.5 * retn->grid;

        for (y = 0; y < 3; y++) {
            xl = x * 3 + y;
            xh = xl * 5;

            retn->indx[xl].a = xh + 0;
            retn->indx[xl].b = xh + 4;
            retn->indx[xl].c = xh + 1;

            retn->indx[xl].d = xh + 0;
            retn->indx[xl].e = xh + 1;
            retn->indx[xl].f = xh + 2;

            retn->indx[xl].g = xh + 0;
            retn->indx[xl].h = xh + 2;
            retn->indx[xl].i = xh + 3;

            retn->indx[xl].j = xh + 0;
            retn->indx[xl].k = xh + 3;
            retn->indx[xl].l = xh + 4;

            retn->clrs[xh + 4].RGBA =
            retn->clrs[xh + 3].RGBA =
            retn->clrs[xh + 2].RGBA =
            retn->clrs[xh + 1].RGBA =
            retn->clrs[xh + 0].RGBA = 0xFF00B000;

            retn->norm[xh + 0] = vobj->norm[fobj[x + 1]];

            retn->vect[xh + 0].x = fbgn.x + fend.x * (FLOAT)(y + 2);
            retn->vect[xh + 0].y = fbgn.y + fend.y * (FLOAT)(y + 2);
            retn->vect[xh + 0].z = fbgn.z + fend.z * (FLOAT)(y + 2);

            retn->vect[xh + 1].x = fbgn.x + (fv00.x - fbgn.x) * FIR_SIZE * (1.0 - FIR_FADE * (FLOAT)y) + fend.x * (FLOAT)y;
            retn->vect[xh + 1].y = fbgn.y + (fv00.y - fbgn.y) * FIR_SIZE * (1.0 - FIR_FADE * (FLOAT)y) + fend.y * (FLOAT)y;
            retn->vect[xh + 1].z = fbgn.z + (fv00.z - fbgn.z) * FIR_SIZE * (1.0 - FIR_FADE * (FLOAT)y) + fend.z * (FLOAT)y;

            retn->vect[xh + 2].x = fbgn.x + (fv01.x - fbgn.x) * FIR_SIZE * (1.0 - FIR_FADE * (FLOAT)y) + fend.x * (FLOAT)y;
            retn->vect[xh + 2].y = fbgn.y + (fv01.y - fbgn.y) * FIR_SIZE * (1.0 - FIR_FADE * (FLOAT)y) + fend.y * (FLOAT)y;
            retn->vect[xh + 2].z = fbgn.z + (fv01.z - fbgn.z) * FIR_SIZE * (1.0 - FIR_FADE * (FLOAT)y) + fend.z * (FLOAT)y;

            retn->vect[xh + 3].x = fbgn.x + (fv11.x - fbgn.x) * FIR_SIZE * (1.0 - FIR_FADE * (FLOAT)y) + fend.x * (FLOAT)y;
            retn->vect[xh + 3].y = fbgn.y + (fv11.y - fbgn.y) * FIR_SIZE * (1.0 - FIR_FADE * (FLOAT)y) + fend.y * (FLOAT)y;
            retn->vect[xh + 3].z = fbgn.z + (fv11.z - fbgn.z) * FIR_SIZE * (1.0 - FIR_FADE * (FLOAT)y) + fend.z * (FLOAT)y;

            retn->vect[xh + 4].x = fbgn.x + (fv10.x - fbgn.x) * FIR_SIZE * (1.0 - FIR_FADE * (FLOAT)y) + fend.x * (FLOAT)y;
            retn->vect[xh + 4].y = fbgn.y + (fv10.y - fbgn.y) * FIR_SIZE * (1.0 - FIR_FADE * (FLOAT)y) + fend.y * (FLOAT)y;
            retn->vect[xh + 4].z = fbgn.z + (fv10.z - fbgn.z) * FIR_SIZE * (1.0 - FIR_FADE * (FLOAT)y) + fend.z * (FLOAT)y;

            retn->texc[xh + 0].u = 0.5 * FIR_TTEX;
            retn->texc[xh + 0].v = 0.5 * FIR_TTEX;
            retn->texc[xh + 2].u = FIR_TTEX;
            retn->texc[xh + 4].u = FIR_TTEX;
        }
    }
    #undef FIR_FADE
    #undef FIR_SIZE
    #undef FIR_TTEX

    if (glGenBuffersARB) {
        glBindBufferARB(GL_INDEX_BUFFER_ARB, retn->iind);
        glBufferDataARB(GL_INDEX_BUFFER_ARB, 3 * fobj[0] * sizeof(FTRI), retn->indx, GL_STATIC_DRAW_ARB);

        glBindBufferARB(GL_ARRAY_BUFFER_ARB, retn->ivec);
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, retn->ndot * sizeof(FVEC), retn->vect, GL_STATIC_DRAW_ARB);

        glBindBufferARB(GL_ARRAY_BUFFER_ARB, retn->iclr);
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, retn->ndot * sizeof(FCLR), retn->clrs, GL_STATIC_DRAW_ARB);

        glBindBufferARB(GL_ARRAY_BUFFER_ARB, retn->inrm);
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, retn->ndot * sizeof(FVEC), retn->norm, GL_STATIC_DRAW_ARB);

        glBindBufferARB(GL_ARRAY_BUFFER_ARB, retn->itex);
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, retn->ndot * sizeof(FTEX), retn->texc, GL_STATIC_DRAW_ARB);

        glBindBufferARB(GL_INDEX_BUFFER_ARB, 0);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    }

    retn->flgs = USE_ARBV;
    retn->npol = 3 * 3 * 4 * fobj[0];
    free(fobj);

    return retn;
}



/**
  @brief LandscapeVBO
  generates a landscape VBO.

  @param ndim - log2 of the map size.
  @param flgs - display flags (see FVBO::flgs).
  @param seed - random number generator seed.
  @param grid - width and height of the elementary square.
  @param fhei - height range.
  @param wlvl - "sea level" within the range.
  @param lscp - array of FHEIs for mapping colors to heights.
  @param file - file into which the VBO shall be serialized.

  @return FVBO on success, NULL on failure (ndim == 0, lscp == NULL, grid <= 0).
**/
FVBO *LandscapeVBO(UINT ndim, UINT flgs, UINT seed, FLOAT grid, FLOAT fhei, FLOAT wlvl, FHEI *lscp, LPSTR file) {
    if (!ndim || !lscp || grid <= 0.0) return NULL;
    if (!glGenBuffersARB) flgs &= ~USE_ARBV;
    ndim = pow(2.0, ndim);
    wlvl = max(wlvl, -0.5 * (fhei = fabs(fhei)));

    FVBO *retn = MakeVBO((ndim + 1) * (ndim + ndim + 2));
    LONG i, x, y, xl, xh, yl, yh, dpos;
    FLOAT fmin, fmax, *farr;
    DWORD wclr;
    BYTE wtrn;

    for (i = y = 0; y < ndim; y++) {
        dpos = y * (ndim + 1) << 1;
        for (x = 0; x < ndim; x++) {
            retn->indx[i].a = dpos + (ndim + 1);
            retn->indx[i].b = dpos;
            retn->indx[i].c = dpos + 1;

            retn->indx[i].d = dpos + (ndim + 1);
            retn->indx[i].e = dpos + 1;
            retn->indx[i].f = dpos + (ndim + ndim + 2) + 1;

            retn->indx[i].g = dpos + (ndim + 1);
            retn->indx[i].h = dpos + (ndim + ndim + 2) + 1;
            retn->indx[i].i = dpos + (ndim + ndim + 2);

            retn->indx[i].j = dpos + (ndim + 1);
            retn->indx[i].k = dpos + (ndim + ndim + 2);
            retn->indx[i].l = dpos;

            dpos++;
            i++;
        }
    }

    srand(seed);
    BlurHeightmap(farr = MakeHeightmap(ndim, 1.0), ndim, 1.5);
    fmin = fmax = farr[0];
    for (x = (ndim + 1) * (ndim + 1); x >= 0; x--) {
        fmin = min(fmin, farr[x]);
        fmax = max(fmax, farr[x]);
    }
    fmax = fhei / (fmax - fmin);
    for (y = 0; y <= ndim; y++) {
        dpos = y * (ndim + 1) << 1;
        for (x = 0; x <= ndim; x++) {
            retn->vect[dpos].x = grid * (FLOAT)x - 0.5 * grid * (FLOAT)ndim;
            retn->vect[dpos].y = grid * (FLOAT)y - 0.5 * grid * (FLOAT)ndim;
            retn->vect[dpos].z = (farr[x + y*(ndim+1)] - fmin) * fmax - 0.5 * fhei;
            if (retn->vect[dpos].z < wlvl) retn->vect[dpos].z = wlvl;
            dpos++;
        }
    }
    free(farr);

    for (y = 0; y < ndim; y++) {
        dpos = (ndim + 1) + (y * (ndim + 1) << 1);
        for (x = 0; x < ndim; x++) {
            retn->vect[dpos].x = grid * (FLOAT)(x + 0.5) - 0.5 * grid * (FLOAT)ndim;
            retn->vect[dpos].y = grid * (FLOAT)(y + 0.5) - 0.5 * grid * (FLOAT)ndim;
            retn->vect[dpos].z = 0.25 * (retn->vect[dpos - ndim - 1].z
                                      +  retn->vect[dpos - ndim - 0].z
                                      +  retn->vect[dpos + ndim + 1].z
                                      +  retn->vect[dpos + ndim + 2].z);
            dpos++;
        }
    }

    for (fmax = i = 0; lscp[i].fhei > 0.0; i++)
        fmax += lscp[i].fhei;
    wclr = lscp[i].fclr.RGBA & 0xFFFFFF;
    wtrn = lscp[i].fclr.A;

    for (y = 0; y <= ndim; y++) {
        dpos = y * (ndim + 1) << 1;
        for (x = 0; x <= ndim; x++) {

            fmin = fmax * (retn->vect[dpos + x].z - wlvl) / (0.5 * fhei - wlvl);
            i = 0;
            while (lscp[i].fhei > 0.0 && (fmin -= lscp[i].fhei) > 0.0) i++;
            if (lscp[i].fhei <= 0.0) i--;

            retn->clrs[dpos + x].RGBA = lscp[i].fclr.RGBA | 0xFF000000;
            if (retn->vect[dpos + x].z == wlvl) {
                xl = (x > 0)?    x - 1 : ndim - 1;
                xh = (x < ndim)? x     : 0;
                yl = (y > 0)?    dpos - (ndim + 1) : (ndim + 1) + ((ndim - 1) * (ndim + 1) << 1);
                yh = (y < ndim)? dpos + (ndim + 1) : (ndim + 1);
                if ((retn->vect[xl + yl].z == wlvl) &&
                    (retn->vect[xl + yh].z == wlvl) &&
                    (retn->vect[xh + yl].z == wlvl) &&
                    (retn->vect[xh + yh].z == wlvl))
                     retn->clrs[dpos + x].RGBA = wclr | (wtrn * 0x1000000);
            }
        }
    }
    for (y = 0; y <= ndim; y++) {
        dpos = y * (ndim + 1) << 1;
        for (x = 0; x <= ndim; x++) {
            xl = x;
            xh = (x < ndim)? x + 1 : 0;
            yl = dpos;
            yh = (y < ndim)? dpos + ((ndim + 1) << 1) : 0;
            retn->clrs[dpos + x + (ndim + 1)].R = (retn->clrs[xl + yl].R
                                                +  retn->clrs[xl + yh].R
                                                +  retn->clrs[xh + yl].R
                                                +  retn->clrs[xh + yh].R) >> 2;
            retn->clrs[dpos + x + (ndim + 1)].G = (retn->clrs[xl + yl].G
                                                +  retn->clrs[xl + yh].G
                                                +  retn->clrs[xh + yl].G
                                                +  retn->clrs[xh + yh].G) >> 2;
            retn->clrs[dpos + x + (ndim + 1)].B = (retn->clrs[xl + yl].B
                                                +  retn->clrs[xl + yh].B
                                                +  retn->clrs[xh + yl].B
                                                +  retn->clrs[xh + yh].B) >> 2;
            retn->clrs[dpos + x + (ndim + 1)].A = 255;
            if (retn->vect[dpos + x + (ndim + 1)].z == wlvl) {
                i = (((retn->clrs[xl + yl].A == wtrn)? 0 : 1)  +
                     ((retn->clrs[xl + yh].A == wtrn)? 0 : 1)  +
                     ((retn->clrs[xh + yl].A == wtrn)? 0 : 1)  +
                     ((retn->clrs[xh + yh].A == wtrn)? 0 : 1)) * (255 - wtrn);
                if (!i) retn->clrs[dpos + x + (ndim + 1)].RGBA = wclr;
                retn->clrs[dpos + x + (ndim + 1)].A = wtrn + (i >> 2);
            }
        }
    }

    for (y = 0; y <= ndim; y++) {
        dpos = y * (ndim + 1) << 1;
        for (x = 0; x <= ndim; x++) {
            retn->norm[dpos + x].x = retn->vect[dpos + ((x > 0)?    x - 1 : ndim - 1)].z
                                   - retn->vect[dpos + ((x < ndim)? x + 1 : 1)].z;
            retn->norm[dpos + x].y = retn->vect[   x + ((y > 0)?    dpos - ((ndim + 1) << 1) : (ndim - 1) * (ndim + 1) << 1)].z
                                   - retn->vect[   x + ((y < ndim)? dpos + ((ndim + 1) << 1) : (ndim + 1) << 1)].z;
        }
    }
    for (y = 0; y < ndim; y++) {
        dpos = (ndim + 1) + (y * (ndim + 1) << 1);
        for (x = 0; x < ndim; x++) {
            retn->norm[dpos + x].x = retn->vect[dpos + ((x > 0)?        x - 1 : ndim - 1)].z
                                   - retn->vect[dpos + ((x < ndim - 1)? x + 1 : 0)].z;
            retn->norm[dpos + x].y = retn->vect[   x + ((y > 0)?        dpos - ((ndim + 1) << 1) : (ndim + 1) + ((ndim - 1) * (ndim + 1) << 1))].z
                                   - retn->vect[   x + ((y < ndim - 1)? dpos + ((ndim + 1) << 1) : (ndim + 1))].z;
        }
    }
    for (y = ndim << 1; y >= 0; y--)
        for (dpos = ndim + (x = y * (ndim + 1)); x <= dpos; x++) {
            retn->norm[x].z = 2.0 * grid;
            fmax = 1.0 / sqrt(retn->norm[x].x * retn->norm[x].x
                            + retn->norm[x].y * retn->norm[x].y
                            + retn->norm[x].z * retn->norm[x].z);
            retn->norm[x].x *= fmax;
            retn->norm[x].y *= fmax;
            retn->norm[x].z *= fmax;
        }

    retn->ntex = MakeFacetTex(64);
    for (y = ndim; y >= 0; y--) {
        dpos = y * (ndim + 1) << 1;
        for (x = 0; x <= ndim; x++) {
            retn->texc[dpos].u = (FLOAT)x;
            retn->texc[dpos].v = (FLOAT)y;
            retn->texc[dpos + (ndim + 1)].u = (FLOAT)x + 0.5;
            retn->texc[dpos + (ndim + 1)].v = (FLOAT)y + 0.5;
            dpos++;
        }
    }
    if (glGenBuffersARB) {
        glBindBufferARB(GL_INDEX_BUFFER_ARB, retn->iind);
        glBufferDataARB(GL_INDEX_BUFFER_ARB, (1 + (retn->ndot >> 1)) * sizeof(FTRI), retn->indx, GL_STATIC_DRAW_ARB);

        glBindBufferARB(GL_ARRAY_BUFFER_ARB, retn->ivec);
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, retn->ndot * sizeof(FVEC), retn->vect, GL_STATIC_DRAW_ARB);

        glBindBufferARB(GL_ARRAY_BUFFER_ARB, retn->iclr);
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, retn->ndot * sizeof(FCLR), retn->clrs, GL_STATIC_DRAW_ARB);

        glBindBufferARB(GL_ARRAY_BUFFER_ARB, retn->inrm);
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, retn->ndot * sizeof(FVEC), retn->norm, GL_STATIC_DRAW_ARB);

        glBindBufferARB(GL_ARRAY_BUFFER_ARB, retn->itex);
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, retn->ndot * sizeof(FTEX), retn->texc, GL_STATIC_DRAW_ARB);

        glBindBufferARB(GL_INDEX_BUFFER_ARB, 0);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    }

    retn->wlvl = wlvl;
    retn->seed = seed;
    retn->flgs = flgs;
    retn->ndim = ndim;
    retn->grid = (FLOAT)ndim * grid;
    retn->npol = 3 * 4 * ndim * ndim;
    retn->next = ObjectVBO(retn, DEF_NOBJ);

    if (file) Serialize(file, retn);
    return retn;
}



/**
  @brief Deserialize
  reads VBO creation parameters from a file, then creates the VBO.
  May skip the reading part and use preloaded values instead.
  Both flgs and seed are overridden by the values read from the file.

  @param file - file from which the VBO shall be deserialized.
  @param open - defines if reading is necessary.
  @param flgs - flags (see FVBO::flgs).
  @param seed - PRNG seed; 0 means "not specified, needs to be generated now".

  @return FVBO on success, NULL on failure.
**/
FVBO *Deserialize(LPSTR file, BOOL open, UINT flgs, UINT seed) {
    FILE *filp;
    FHEI lscp[] = {{.fhei = 0.1, .fclr.RGBA = 0xFF76DDFC},
                   {.fhei = 8.0, .fclr.RGBA = 0xFF30A15D},
                   {.fhei = 6.5, .fclr.RGBA = 0xFF808080},
                   {.fhei = 5.0, .fclr.RGBA = 0xFFFFFFFF},
                   {.fhei = 0.0, .fclr.RGBA = 0x80AC630D}};

    CamLightReset();
    if (!seed) seed = rand() * time(0);
    if (open && (filp = fopen(file, "rb"))) {
        if ((file = strstr(file, DEF_FEXT)) && (strlen(file) == DEF_FEXL)) {
            fscanf(filp, DEF_FRMT,
                  &seed,    &flgs,
                  &fang.u,  &fang.v,
                  &ftrn.x,  &ftrn.y,  &ftrn.z,
                  &ldir[0], &ldir[1], &ldir[2],
                  &lpos[0], &lpos[1], &lpos[2]);
        }
        else {
            fread(&seed, sizeof(UINT), 1, filp);
            fread(&flgs, sizeof(UINT), 1, filp);
            fread(&fang, sizeof(FTEX), 1, filp);
            fread(&ftrn, sizeof(FVEC), 1, filp);
            fread(ldir, sizeof(FVEC), 1, filp);
            fread(lpos, sizeof(FVEC), 1, filp);
        }
        fclose(filp);
        file = NULL;
    }
    return LandscapeVBO(DEF_LPWR, flgs, seed, DEF_GRID, DEF_FHEI, DEF_WLVL, lscp, file);
}



/**
  @brief tmrcount
  handles the timer that recomputes the scene.
  No parameters except pUsr are used.

  @param pUsr - handle of the main drawing surface.
**/
void CALLBACK tmrcount(UINT uTmr, UINT uMsg, DWORD pUsr, DWORD pdR1, DWORD pdR2) {
    UINT tnew = GetTickCount();

    if (keys['S'] | keys['W']) {
        ftrn.x += ((keys['W'])? DEF_FTRN : -DEF_FTRN) * sin(fang.u * DEG_CRAD) * sin(fang.v * DEG_CRAD);
        ftrn.y += ((keys['W'])? DEF_FTRN : -DEF_FTRN) * cos(fang.u * DEG_CRAD) * sin(fang.v * DEG_CRAD);
        ftrn.z += ((keys['W'])? DEF_FTRN : -DEF_FTRN) * cos(fang.v * DEG_CRAD);
    }
    if (keys['D'] | keys['A']) {
        ftrn.x += ((keys['A'])? DEF_FTRN : -DEF_FTRN) * cos(fang.u * DEG_CRAD);
        ftrn.y -= ((keys['A'])? DEF_FTRN : -DEF_FTRN) * sin(fang.u * DEG_CRAD);
    }
    if (ftrn.x >  0.5 * land->grid) {
        ftrn.x -= land->grid;
        lpos[0]+= land->grid;
    } else
    if (ftrn.x < -0.5 * land->grid) {
        ftrn.x += land->grid;
        lpos[0]-= land->grid;
    }
    if (ftrn.y >  0.5 * land->grid) {
        ftrn.y -= land->grid;
        lpos[1]+= land->grid;
    } else
    if (ftrn.y < -0.5 * land->grid) {
        ftrn.y += land->grid;
        lpos[1]-= land->grid;
    }
    if (tnew - tick > 1000) {
        CHAR fstr[MAX_PATH];

        sprintf(fstr, "VBO %s, %0.0f FPS",
               (land->flgs & USE_ARBV)? "enabled" : "disabled",
                1000.0 * (FLOAT)fram / (FLOAT)(tnew - tick));
        tick = tnew;
        fram = 0;
        SendMessage((HWND)pUsr, WM_SETTEXT, 0, (LPARAM)fstr);
    }
}



/**
  @brief tmrpaint
  handles the timer that repaints the scene.
  No parameters except pUsr are used.

  @param pUsr - handle of the main drawing surface.
**/
void CALLBACK tmrpaint(UINT uTmr, UINT uMsg, DWORD pUsr, DWORD pdR1, DWORD pdR2) {
    if (paint) InvalidateRect((HWND)pUsr, NULL, FALSE);
}



/**
  @brief DialogProc:
  window function of the main drawing surface.

  @param hDlg - handle of the surface.
  @param uMsg - message to process.
  @param wPrm - first parameter.
  @param lPrm - second parameter.

  @return FALSE (anywhere except WM_INITDIALOG branch, where TRUE is returned).
**/
BOOL CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wPrm, LPARAM lPrm) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            PIXELFORMATDESCRIPTOR pfd = {sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32};
            FLOAT fogc[] = {0.75, 0.75, 1.0, 1.0};

            paint = FALSE;
            DC = GetDC(hDlg);
            SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon((HINSTANCE)lPrm, MAKEINTRESOURCE(1)));
            pfd.iLayerType = PFD_MAIN_PLANE;
            SetPixelFormat(DC, ChoosePixelFormat(DC, &pfd), &pfd);
            wglMakeCurrent(DC, RC = wglCreateContext(DC));

            glClearColor(fogc[0], fogc[1], fogc[2], fogc[3]);
            glPointSize(5.0);

            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glEnable(GL_COLOR_MATERIAL);

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);

            glCullFace(GL_BACK);
            glEnable(GL_CULL_FACE);

            glDepthFunc(GL_LESS);
            glEnable(GL_DEPTH_TEST);

            glShadeModel(GL_SMOOTH);
            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);

            glFogi(GL_FOG_MODE, GL_EXP);
            glFogf(GL_FOG_DENSITY, 0.0015);
            glFogfv(GL_FOG_COLOR, fogc);
            glHint(GL_FOG_HINT, GL_NICEST);
            glEnable(GL_FOG);

            if (strstr((LPSTR)glGetString(GL_EXTENSIONS), "GL_ARB_vertex_buffer_object ")) {
                glGenBuffersARB = wglGetProcAddress("glGenBuffersARB");
                glBindBufferARB = wglGetProcAddress("glBindBufferARB");
                glBufferDataARB = wglGetProcAddress("glBufferDataARB");
                glDelBuffersARB = wglGetProcAddress("glDeleteBuffersARB");
            }
            land = Deserialize(path, TRUE, USE_ARBV | USE_FILL | USE_NORM | USE_TEXC | USE_CLRS | USE_OBJS, 0);

            tmrc = timeSetEvent(DEF_TMRC, 0, tmrcount, (DWORD)hDlg, TIME_PERIODIC);
            tmrp = timeSetEvent(DEF_TMRP, 0, tmrpaint, (DWORD)hDlg, TIME_PERIODIC);
            return paint = TRUE;
        }


        case WM_CLOSE:
            paint = FALSE;
            timeKillEvent(tmrp);
            timeKillEvent(tmrc);
            if (land) {
                Serialize(path, land);
                FreeVBO(&land);
            }
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(RC);
            ReleaseDC(hDlg, DC);
            DeleteDC(DC);
            PostQuitMessage(0);
            return FALSE;


        case WM_LBUTTONDOWN:
            SetCapture(hDlg);
            GetCursorPos(&angp);
            return FALSE;


        case WM_LBUTTONUP:
            ReleaseCapture();
            return FALSE;


        case WM_KEYDOWN:
            keys[wPrm & 0xFF] = TRUE;
            switch (wPrm & 0xFF) {
                case VK_RETURN:
                    CamLightReset();
                    break;

                case 'Z':
                    if (glGenBuffersARB) land->flgs ^= USE_ARBV;
                    break;

                case 'X':
                    land->flgs ^= USE_FILL;
                    break;

                case 'C':
                    land->flgs ^= USE_NORM;
                    break;

                case 'V':
                    land->flgs ^= USE_CLRS;
                    break;

                case 'B':
                    land->flgs ^= USE_TEXC;
                    break;

                case 'N':
                    land->flgs ^= USE_OBJS;
                    break;
            }
            return FALSE;


        case WM_KEYUP:
            keys[wPrm & 0xFF] = FALSE;
            return FALSE;


        case WM_MOUSEMOVE:
            if (wPrm & MK_LBUTTON) {
                GetCursorPos(&movp);
                fang.v += DEF_FANG * (FLOAT)(movp.y - angp.y);
                if (fang.v < -180.0) fang.v += 360.0;
                else if (fang.v > 180.0) fang.v -= 360.0;
                fang.u += ((fang.v <= 0.0)? DEF_FANG : -DEF_FANG) * (FLOAT)(movp.x - angp.x);
                if (fang.u < -180.0) fang.u += 360.0;
                else if (fang.u > 180.0) fang.u -= 360.0;
                angp = movp;
            }
            return FALSE;


        case WM_PAINT: {
            PAINTSTRUCT pstr;
            FVEC ftmp;
            LONG x, y;

            if (keys[VK_SPACE]) {
                keys[VK_SPACE] = FALSE;
                x = land->flgs;
                FreeVBO(&land);
                land = Deserialize(path, keys[0], x, 0);
                keys[0] = FALSE;
            }
            BeginPaint(hDlg, &pstr);
            if (paint) {
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                glPushMatrix();
                glPushMatrix();
                glRotatef(fang.v, 1, 0, 0);
                glRotatef(fang.u, 0, 0, 1);
                glTranslatef(ftrn.x, ftrn.y, ftrn.z);

                glLightfv(GL_LIGHT0, GL_POSITION, lpos);
                glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, ldir);

                #define DEF_DRAW 4
                glTranslatef(-0.5 * (FLOAT)(DEF_DRAW >> 1) * land->grid, -0.5 * (FLOAT)(DEF_DRAW >> 1) * land->grid, 0.0);
                glCullFace(GL_FRONT);
                glPushMatrix();
                glTranslatef(0.0, 0.0, 2.0 * land->wlvl);
                glScalef(1.0, 1.0, -1.0);
                for (y = 0; y < DEF_DRAW; y++) {
                    for (x = 0; x < DEF_DRAW; x++) {
                        DrawVBO(land);
                        glTranslatef(land->grid, 0.0, 0.0);
                    }
                    glTranslatef(-(FLOAT)DEF_DRAW * land->grid, land->grid, 0.0);
                }
                glPopMatrix();
                glCullFace(GL_BACK);
                for (y = 0; y < DEF_DRAW; y++) {
                    for (x = 0; x < DEF_DRAW; x++) {
                        DrawVBO(land);
                        glTranslatef(land->grid, 0.0, 0.0);
                    }
                    glTranslatef(-(FLOAT)DEF_DRAW * land->grid, land->grid, 0.0);
                }
                #undef DEF_DRAW

                glPopMatrix();

                glDisable(GL_FOG);
                glDisable(GL_LIGHTING);
                glClear(GL_DEPTH_BUFFER_BIT);

                #define DEF_QUAD 5.0
                ftmp.x = -ftrn.x * DEF_QUAD / land->grid;
                ftmp.y = -ftrn.y * DEF_QUAD / land->grid;
                ftmp.z = -(ftrn.z + land->wlvl) * DEF_QUAD / land->grid;

                glTranslatef(0.0, -10.0, -50.0);
                glRotatef(fang.v, 1, 0, 0);
                glRotatef(fang.u, 0, 0, 1);
                glTranslatef(-ftmp.x, -ftmp.y, 0.0);

                glBegin(GL_LINES);
                    glColor4ub(255, 255, 255, 255);
                    glVertex3f(-DEF_QUAD, -DEF_QUAD, 0.0);
                    glVertex3f(-DEF_QUAD, DEF_QUAD, 0.0);

                    glVertex3f(DEF_QUAD, -DEF_QUAD, 0.0);
                    glVertex3f(DEF_QUAD, DEF_QUAD, 0.0);

                    glVertex3f(-DEF_QUAD, -DEF_QUAD, 0.0);
                    glVertex3f(DEF_QUAD,  -DEF_QUAD, 0.0);

                    glVertex3f(-DEF_QUAD, DEF_QUAD, 0.0);
                    glVertex3f(DEF_QUAD,  DEF_QUAD, 0.0);

                    glVertex3f(-DEF_QUAD, 0.0, 0.0);
                    glVertex3f(DEF_QUAD,  0.0, 0.0);

                    glVertex3f(0.0, -DEF_QUAD, 0.0);
                    glVertex3f(0.0, DEF_QUAD,  0.0);

                    glVertex3f(0.0, 0.0, 0.0);
                    glVertex3f(0.0, 0.0, 0.5 * DEF_QUAD);

                    glColor4ub(255, 0, 0, 255);
                    glVertex3f(ftmp.x, ftmp.y, 0.0);
                    glVertex3f(ftmp.x, ftmp.y, ftmp.z);
                glEnd();

                glBegin(GL_POINTS);
                    glVertex3f(ftmp.x, ftmp.y, ftmp.z);
                glEnd();
                #undef DEF_QUAD

                glEnable(GL_LIGHTING);
                glEnable(GL_FOG);

                glPopMatrix();

                SwapBuffers(DC);
                fram++;
            }
            EndPaint(hDlg, &pstr);
            return FALSE;
        }


        case WM_SIZE:
            if (wPrm != SIZE_MINIMIZED) {
                BOOL ptmp = paint;
                paint = FALSE;
                FLOAT y = DEF_ZNEA * tan(0.5 * DEF_FFOV * DEG_CRAD), x = y * (FLOAT)LOWORD(lPrm)/(FLOAT)HIWORD(lPrm);

                glViewport(0, 0, LOWORD(lPrm), HIWORD(lPrm));
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glFrustum(-x, x, -y, y, DEF_ZNEA, DEF_ZFAR);

                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
                paint = ptmp;

                InvalidateRect(hDlg, NULL, FALSE);
            }
            return FALSE;


        case WM_DROPFILES: {
            free(path);
            DragQueryFile((HDROP)wPrm, 0, path = (LPSTR)malloc(MAX_PATH + 1), MAX_PATH);
            DragFinish((HDROP)wPrm);
            keys[VK_SPACE] = keys[0] = TRUE;
        }


        default:
            return FALSE;
    }
}



/**
  @brief WinMain:
  analogue of main() for a Windows application.
  All parameters except inst and cmdl are ignored.

  @param inst - base address of the process.
  @param cmdl - command line; may substitute the default config file.

  @return 0 (default process exit code).
**/
int CALLBACK WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdl, int show) {
    MSG pmsg;

    srand(time(0));
    if (strlen(cmdl)) {
        while (*cmdl == ' ' || *cmdl == '"') cmdl++;
        path = strdup(cmdl);
        if ((cmdl = strchr(path, '"'))) *cmdl = 0;
    }
    else
        path = strdup(DEF_FILE);

    CreateDialogParam(inst, MAKEINTRESOURCE(1), NULL, (DLGPROC)DialogProc, (LPARAM)inst);
    while (GetMessage(&pmsg, NULL, 0, 0)) {
        TranslateMessage(&pmsg);
        DispatchMessage(&pmsg);
    }
    free(path);
    return 0;
}
