#ifndef _CACTUS_FACE_H_
#define _CACTUS_FACE_H_

#include "cactusGlobals.h"

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
//Basic face functions.
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////

/*
 * Constructs faces in net
 */
Face * face_construct();

/* 
 * Simple destructor
 */
void face_destruct(Face * face);

/*
 * Returns cardinal
 */
int32_t face_getCardinal(Face * face);

/*
 * Get selected top node
 */
Cap * face_getTopNode(Face * face, int32_t index);

/*
 * Get selected derived destination  of selected top node
 */
Cap * face_getDerivedDestination(Face * face, int32_t index);

/*
 * Get the number of bottom nodes for the selected top node
 */
int32_t face_getBottomNodeNumber(Face * face, int32_t topIndex);

/*
 * Get selected bottom node from selected top node in face
 */
Cap * face_getBottomNode(Face * face, int32_t topNodeIndex, int32_t bottomNodeIndex);

/*
 * Allocate arrays to allow for data 
 */
void face_allocateSpace(Face * face, int32_t cardinal); 

/*
 * Sets the selected top node
 */
void face_setTopNode(Face * face, int32_t topIndex, Cap * topNode);

/*
 * Set bottom node count and allocate space to store pointers
 */
void face_setBottomNodeNumber(Face * face, int32_t topIndex, int32_t number);

/*
 * Sets the derived edge destination for a given top node in face
 */
void face_setDerivedDestination(Face * face, int32_t topIndex, Cap * destination);

/*
 * Adds bottom node to selected top node in face
 */
void face_addBottomNode(Face * face, int32_t topIndex, Cap * bottomNode);

/*
 * Adds a top node with a single bottom node
 */
void face_engineerArtificialNodes(Face * face, Cap * topNode, Cap * bottomNode, int32_t nonDerived);

/*
 * Returns face name
 */
Name face_getName(Face * face);
#endif
