#include "sprite.h"
#include "list.h"
#include "sys.h"


/*
 * ----------------------------------------------------------------------------
 *
 * VmListInsert --
 *
 *	Insert the list element pointed to by itemPtr into a List after 
 *	destPtr.  Perform a primitive test for self-looping by returning
 *	failure if the list element is being inserted next to itself.
 *
 * Results:
 *	If neither List_Links structure is NIL, they are assumed to be valid
 *	and SUCCESS is returned.  If either one is NIL then FAILURE is
 *	returned.  
 *
 * Side effects:
 *	The list containing destPtr is modified to contain itemPtr.
 *
 * ----------------------------------------------------------------------------
 */
ReturnStatus
VmListInsert(itemPtr, destPtr)
    register	List_Links *itemPtr;	/* structure to insert */
    register	List_Links *destPtr;	/* structure after which to insert it */
{
    if (itemPtr->nextPtr != (List_Links *) NIL ||
	itemPtr->prevPtr != (List_Links *) NIL) {
	Sys_Panic(SYS_FATAL, "VmListInsert: Inserting element twice.\n");
    }

    if (itemPtr == (List_Links *) NIL || destPtr == (List_Links *) NIL
	    || !itemPtr || !destPtr || (itemPtr == destPtr)) {
	return(FAILURE);
    }
    itemPtr->nextPtr = destPtr->nextPtr;
    itemPtr->prevPtr = destPtr;
    destPtr->nextPtr->prevPtr = itemPtr;
    destPtr->nextPtr = itemPtr;
    return(SUCCESS);
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmListRemove --
 *
 *	Remove a list element from the list in which it is contained.
 *
 * Results:
 *	If the list element is invalid or is the list header, FAILURE is 
 *	returned.  Otherwise SUCCESS is returned.
 *
 * Side effects:
 *	The given structure is removed from its containing list.
 *
 * ----------------------------------------------------------------------------
 */
ReturnStatus
VmListRemove(itemPtr)
    register	List_Links *itemPtr;	/* list element to remove */
{
    if (itemPtr->nextPtr == (List_Links *) NIL ||
	itemPtr->prevPtr == (List_Links *) NIL) {
	Sys_Panic(SYS_FATAL, "VmListRemove: Item not on list.\n");
    }
    if (itemPtr == (List_Links *) NIL || itemPtr == itemPtr->nextPtr
	    || !itemPtr) {
	return(FAILURE);
    }
    itemPtr->prevPtr->nextPtr = itemPtr->nextPtr;
    itemPtr->nextPtr->prevPtr = itemPtr->prevPtr;
    itemPtr->prevPtr = (List_Links *) NIL;
    itemPtr->nextPtr = (List_Links *) NIL;
    return(SUCCESS);
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmListMove --
 *
 *	Move the list element referenced by itemPtr to follow destPtr.
 *
 * Results:
 *	If either list element is invalid, FAILURE is returned.
 *	Otherwise SUCCESS is returned.
 *
 * Side effects:
 *	List ordering is modified.
 *
 * ----------------------------------------------------------------------------
 */
ReturnStatus
VmListMove(itemPtr, destPtr)
    register List_Links *itemPtr; /* list element to be moved */
    register List_Links *destPtr; /* element after which it is to be placed */
{
    ReturnStatus status;

    if (itemPtr == (List_Links *) NIL || destPtr == (List_Links *) NIL
	    || !itemPtr || !destPtr) {
	return(FAILURE);
    }
    /*
     * It is conceivable that someone will try to move a list element to
     * be after itself.
     */
    if (itemPtr != destPtr) {
	status = VmListRemove(itemPtr);
	if (status != SUCCESS) {
	    return(status);
	}
	status = VmListInsert(itemPtr, destPtr);
	if (status != SUCCESS) {
	    return(status);
	}
    }    
    return(SUCCESS);
}

