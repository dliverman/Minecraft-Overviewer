/* 
 * This file is part of the Minecraft Overviewer.
 *
 * Minecraft Overviewer is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * Minecraft Overviewer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Overviewer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "overviewer.h"

#include <numpy/arrayobject.h>

/* macro for getting blockID from a chunk of memory */
#define getBlock(blockThing, x,y,z) (*(unsigned char *)(PyArray_GETPTR3(blockThing, (x), (y), (z))))

static PyObject *textures = NULL;
static PyObject *chunk_mod = NULL;
static PyObject *blockmap = NULL;
static PyObject *special_blocks = NULL;
static PyObject *specialblockmap = NULL;
static PyObject *transparent_blocks = NULL;

int init_chunk_render(void) {
   
    /* if blockmap (or any of these) is not NULL, then that means that we've 
     * somehow called this function twice.  error out so we can notice this
     * */
    if (blockmap) return 1;

    textures = PyImport_ImportModule("textures");
    chunk_mod = PyImport_ImportModule("chunk");

    blockmap = PyObject_GetAttrString(textures, "blockmap");
    special_blocks = PyObject_GetAttrString(textures, "special_blocks");
    specialblockmap = PyObject_GetAttrString(textures, "specialblockmap");
    transparent_blocks = PyObject_GetAttrString(chunk_mod, "transparent_blocks");
    
    /* ensure none of these pointers are NULL */    
    if ((!transparent_blocks) || (!blockmap) || (!special_blocks) || (!specialblockmap)){
        fprintf(stderr, "\ninit_chunk_render failed\n");
        return 1;
    }

    Py_DECREF(textures);
    Py_DECREF(chunk_mod);
    return 0;

}

static inline int isTransparent(PyObject* tup, unsigned char b) {
    PyObject *block = PyInt_FromLong(b);
    int ret = PySequence_Contains(tup, block);
    Py_DECREF(block);
    return ret;

}

/* helper to handle alpha_over calls involving a texture tuple */
static inline PyObject *
texture_alpha_over(PyObject *dest, PyObject *t, int imgx, int imgy)
{
    PyObject* src, * mask;

    src = PyTuple_GET_ITEM(t, 0);
    mask = PyTuple_GET_ITEM(t, 1);
    if (mask == Py_None) {
        mask = src;
    }

    return alpha_over(dest, src, mask, imgx, imgy, 0, 0);
}

PyObject*
chunk_render_lighting(PyObject *self, PyObject *args) {

    PyObject *chunk;
    PyObject *blockdata_expanded; 
    int xoff, yoff;
    PyObject *img;
    
    PyObject *imgsize, *imgsize0_py, *imgsize1_py;
    int imgsize0, imgsize1;
    
    PyObject *blocks_py;
    
    PyObject *textures, *blockmap, *special_blocks, *specialblockmap, *chunk_mod, *transparent_blocks;
    
    int imgx, imgy;
    int x, y, z;

    if (!PyArg_ParseTuple(args, "OOiiO",  &chunk, &img, &xoff, &yoff, &blockdata_expanded))
        return Py_BuildValue("i", "-1");

    /* tuple */
    imgsize = PyObject_GetAttrString(img, "size");

    imgsize0_py = PySequence_GetItem(imgsize, 0);
    imgsize1_py = PySequence_GetItem(imgsize, 1);
    Py_DECREF(imgsize);

    imgsize0 = PyInt_AsLong(imgsize0_py);
    imgsize1 = PyInt_AsLong(imgsize1_py);
    Py_DECREF(imgsize0_py);
    Py_DECREF(imgsize1_py);


    /* get the block data directly from numpy: */
    blocks_py = PyObject_GetAttrString(chunk, "blocks");

    /*
    PyObject *left_blocks = PyObject_GetAttrString(chunk, "left_blocks");
    PyObject *right_blocks = PyObject_GetAttrString(chunk, "right_blocks");
    */
    
    textures = PyImport_ImportModule("textures");
    chunk_mod = PyImport_ImportModule("chunk");

    /* TODO can these be global static?  these don't change during program execution */
    blockmap = PyObject_GetAttrString(textures, "blockmap");
    special_blocks = PyObject_GetAttrString(textures, "special_blocks");
    specialblockmap = PyObject_GetAttrString(textures, "specialblockmap");
    transparent_blocks = PyObject_GetAttrString(chunk_mod, "transparent_blocks");
    if (transparent_blocks == NULL) {
        PyErr_SetString(PyExc_ValueError,
                        "transparent_blocks is NULL");
        return NULL;
    }
    PyObject *black_color = PyObject_GetAttrString(chunk_mod, "black_color");
    PyObject *facemasks_py = PyObject_GetAttrString(chunk_mod, "facemasks");
    PyObject *facemasks[3];
    //assert(PyTuple_Check(facemasks_py));
    facemasks[0] = PyTuple_GetItem(facemasks_py, 0);
    facemasks[1] = PyTuple_GetItem(facemasks_py, 1);
    facemasks[2] = PyTuple_GetItem(facemasks_py, 2);
        

    Py_DECREF(textures);
    Py_DECREF(chunk_mod);


    
    for (x = 15; x > -1; x--) {
        for (y = 0; y < 16; y++) {
            imgx = xoff + x*12 + y*12;
            /* 128*12 -- offset for z direction, 15*6 -- offset for x */
            imgy = yoff - x*6 + y*6 + 128*12 + 15*6;
            for (z = 0; z < 128; z++) {
                unsigned char block;
                PyObject *blockid;
                
                imgy -= 12;
                
                if ((imgx >= imgsize0 + 24) || (imgx <= -24)) {
                    continue;
                }
                if ((imgy >= imgsize1 + 24) || (imgy <= -24)) {
                    continue;
                }

                /* get blockid
                   note the order: x, z, y */
                block = getBlock(blocks_py, x, y, z);
                if (block == 0) {
                    continue;
                }
                
                /* TODO figure out how to DECREF this easily, instead of at
                   every continue */
                blockid = PyInt_FromLong(block);


                if ( (x != 0) && (y != 15) && (z != 127) &&
                     !isTransparent(transparent_blocks, getBlock(blocks_py, x-1, y, z)) &&
                     !isTransparent(transparent_blocks, getBlock(blocks_py, x, y, z+1)) &&
                     !isTransparent(transparent_blocks, getBlock(blocks_py, x, y+1, z))) {
                    continue;
                }

                //# top face
                //black_coeff, face_occlude = self.get_lighting_coefficient(x, y, z + 1)


                PyObject *t = NULL;
                if (!PySequence_Contains(special_blocks, blockid)) {
                    /* t = textures.blockmap[blockid] */
                    PyObject *t = PyList_GetItem(blockmap, block);
                    /* PyList_GetItem returns borrowed ref */

                    /* note that this version of alpha_over has a different signature than the 
                       version in _composite.c */
                    texture_alpha_over(img, t, imgx, imgy );
                } else {
                    PyObject *tmp;
                    
                    /* this should be a pointer to a unsigned char */
                    void* ancilData_p = PyArray_GETPTR3(blockdata_expanded, x, y, z); 
                    unsigned char ancilData = *((unsigned char*)ancilData_p);
                    if (block == 85) {
                        /* fence.  skip the generate_pseudo_ancildata for now */
                        continue;
                    }
                    
                    tmp = PyTuple_New(2);

                    Py_INCREF(blockid); /* because SetItem steals */
                    PyTuple_SetItem(tmp, 0, blockid);
                    PyTuple_SetItem(tmp, 1, PyInt_FromLong(ancilData));
                    
                    /* this is a borrowed reference. no need to decref */
                    t = PyDict_GetItem(specialblockmap, tmp);
                    Py_DECREF(tmp);

                }
                if ((t != NULL) && (t != Py_None)) 
                    texture_alpha_over(img, t, imgx, imgy );

                {
                    // returns new references
                    PyObject* light_tup = PyObject_CallMethod(chunk, "get_lighting_coefficient", "iii", x, y, z+1);
                    PyObject *black_coeff_py = PySequence_GetItem(light_tup, 0);
                    double black_coeff = PyFloat_AsDouble(black_coeff_py);
                    Py_DECREF(black_coeff_py);

                    PyObject *face_occlude_py = PySequence_GetItem(light_tup, 1);
                    int face_occlude = PyInt_AsLong(face_occlude_py);
                    Py_DECREF(face_occlude_py);


                    if (!face_occlude) {
                        //#composite.alpha_over(img, over_color, (imgx, imgy), ImageEnhance.Brightness(facemasks[0]).enhance(black_coeff))

                        PyObject *mask = PyObject_CallMethod(facemasks[0], "copy", NULL); // new ref
                        //printf("black_coeff: %f\n", black_coeff);
                        brightness(mask, black_coeff);
                        //printf("done with brightness\n");
                        alpha_over(img, black_color, mask, imgx, imgy, 0, 0);
                        //printf("done with alpha_over\n");
                        Py_DECREF(mask);

                    }
                }


                {                
                    // returns new references
                    PyObject* light_tup = PyObject_CallMethod(chunk, "get_lighting_coefficient", "iii", x-1, y, z);
                    PyObject *black_coeff_py = PySequence_GetItem(light_tup, 0);
                    double black_coeff = PyFloat_AsDouble(black_coeff_py);
                    Py_DECREF(black_coeff_py);

                    PyObject *face_occlude_py = PySequence_GetItem(light_tup, 1);
                    int face_occlude = PyInt_AsLong(face_occlude_py);
                    Py_DECREF(face_occlude_py);


                    if (!face_occlude) {
                        //#composite.alpha_over(img, over_color, (imgx, imgy), ImageEnhance.Brightness(facemasks[0]).enhance(black_coeff))
                        /* for now, we are lazy and call Image.blend to change the brightness of facemask
                         *  TODO find some faster way?
                         */

                        PyObject *mask = PyObject_CallMethod(facemasks[1], "copy", NULL); // new ref
                        //printf("black_coeff: %f\n", black_coeff);
                        brightness(mask, black_coeff);
                        //printf("done with brightness\n");
                        alpha_over(img, black_color, mask, imgx, imgy, 0, 0);
                        //printf("done with alpha_over\n");
                        Py_DECREF(mask);

                    }
                }


                { 
                    // returns new references
                    PyObject* light_tup = PyObject_CallMethod(chunk, "get_lighting_coefficient", "iii", x, y+1, z);
                    PyObject *black_coeff_py = PySequence_GetItem(light_tup, 0);
                    double black_coeff = PyFloat_AsDouble(black_coeff_py);
                    Py_DECREF(black_coeff_py);

                    PyObject *face_occlude_py = PySequence_GetItem(light_tup, 1);
                    int face_occlude = PyInt_AsLong(face_occlude_py);
                    Py_DECREF(face_occlude_py);


                    if (!face_occlude) {
                        //#composite.alpha_over(img, over_color, (imgx, imgy), ImageEnhance.Brightness(facemasks[0]).enhance(black_coeff))

                        PyObject *mask = PyObject_CallMethod(facemasks[2], "copy", NULL); // new ref
                        //printf("black_coeff: %f\n", black_coeff);
                        brightness(mask, black_coeff);
                        //printf("done with brightness\n");
                        alpha_over(img, black_color, mask, imgx, imgy, 0, 0);
                        //printf("done with alpha_over\n");
                        Py_DECREF(mask);

                    }
                }



            }
        }
    } 

    Py_DECREF(facemasks_py);
    Py_DECREF(blocks_py);
    Py_DECREF(blockmap);
    Py_DECREF(special_blocks);
    Py_DECREF(specialblockmap);

    return Py_BuildValue("i",2);

}

/* TODO triple check this to make sure reference counting is correct */
PyObject*
chunk_render(PyObject *self, PyObject *args) {

    PyObject *chunk;
    PyObject *blockdata_expanded; 
    int xoff, yoff;
    PyObject *img;
    
    PyObject *imgsize, *imgsize0_py, *imgsize1_py;
    int imgsize0, imgsize1;
    
    PyObject *blocks_py;
    
    int imgx, imgy;
    int x, y, z;

    if (!PyArg_ParseTuple(args, "OOiiO",  &chunk, &img, &xoff, &yoff, &blockdata_expanded))
        return Py_BuildValue("i", "-1");

    /* tuple */
    imgsize = PyObject_GetAttrString(img, "size");

    imgsize0_py = PySequence_GetItem(imgsize, 0);
    imgsize1_py = PySequence_GetItem(imgsize, 1);
    Py_DECREF(imgsize);

    imgsize0 = PyInt_AsLong(imgsize0_py);
    imgsize1 = PyInt_AsLong(imgsize1_py);
    Py_DECREF(imgsize0_py);
    Py_DECREF(imgsize1_py);


    /* get the block data directly from numpy: */
    blocks_py = PyObject_GetAttrString(chunk, "blocks");

    /*
    PyObject *left_blocks = PyObject_GetAttrString(chunk, "left_blocks");
    PyObject *right_blocks = PyObject_GetAttrString(chunk, "right_blocks");
    */
    
    
    for (x = 15; x > -1; x--) {
        for (y = 0; y < 16; y++) {
            imgx = xoff + x*12 + y*12;
            /* 128*12 -- offset for z direction, 15*6 -- offset for x */
            imgy = yoff - x*6 + y*6 + 128*12 + 15*6;
            for (z = 0; z < 128; z++) {
                unsigned char block;
                PyObject *blockid;
                
                imgy -= 12;
                
                if ((imgx >= imgsize0 + 24) || (imgx <= -24)) {
                    continue;
                }
                if ((imgy >= imgsize1 + 24) || (imgy <= -24)) {
                    continue;
                }

                /* get blockid
                   note the order: x, z, y */
                block = getBlock(blocks_py, x, y, z);
                if (block == 0) {
                    continue;
                }
                
                /* TODO figure out how to DECREF this easily, instead of at
                   every continue */
                blockid = PyInt_FromLong(block);


                if ( (x != 0) && (y != 15) && (z != 127) &&
                     !isTransparent(transparent_blocks, getBlock(blocks_py, x-1, y, z)) &&
                     !isTransparent(transparent_blocks, getBlock(blocks_py, x, y, z+1)) &&
                     !isTransparent(transparent_blocks, getBlock(blocks_py, x, y+1, z))) {
                    continue;
                }


                if (!PySequence_Contains(special_blocks, blockid)) {
                    /* t = textures.blockmap[blockid] */
                    PyObject *t = PyList_GetItem(blockmap, block);
                    /* PyList_GetItem returns borrowed ref */
                    if (t == Py_None) {
                        continue;
                    }

                    /* note that this version of alpha_over has a different signature than the 
                       version in _composite.c */
                    texture_alpha_over(img, t, imgx, imgy );
                } else {
                    PyObject *tmp, *t;
                    
                    /* this should be a pointer to a unsigned char */
                    void* ancilData_p = PyArray_GETPTR3(blockdata_expanded, x, y, z); 
                    unsigned char ancilData = *((unsigned char*)ancilData_p);
                    if (block == 85) {
                        /* fence.  skip the generate_pseudo_ancildata for now */
                        continue;
                    }
                    
                    tmp = PyTuple_New(2);

                    Py_INCREF(blockid); /* because SetItem steals */
                    PyTuple_SetItem(tmp, 0, blockid);
                    PyTuple_SetItem(tmp, 1, PyInt_FromLong(ancilData));
                    
                    /* this is a borrowed reference. no need to decref */
                    t = PyDict_GetItem(specialblockmap, tmp);
                    Py_DECREF(tmp);
                    if (t != NULL) 
                        texture_alpha_over(img, t, imgx, imgy );
                    continue;
                }
            }
        }
    } 

    Py_DECREF(blocks_py);

    return Py_BuildValue("i",2);
}