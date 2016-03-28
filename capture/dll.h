/******************************************************************************/
/* dll.h  -- Double Linked List
 *
 * Simple macro based DLL with counts that supports having a single item in
 * multiple lists by using a prefix.  Every element in the list needs to 
 * have a [prefix]next and [prefix]prev elements.  The list head needs to have
 * a [prefix]count element.  
 *
 * WARNING: If using a seperate head structure AND a single item is in multiple 
 * lists the byte position in the structure of the next and prev elements MUST be
 * the same between the head structure and item structures.
 */

#ifndef _DLL_HEADER
#define _DLL_HEADER


#define DLL_INIT(name,head) \
    ((head)->name##count = 0, \
     (head)->name##next = (head)->name##prev = (void *)head \
    )

#define DLL_PUSH_TAIL(name,head,element) \
    ((element)->name##next          = (void *)(head), \
     (element)->name##prev          = (head)->name##prev, \
     (head)->name##prev->name##next = (element), \
     (head)->name##prev             = (element), \
     (head)->name##count++ \
    )

#define DLL_PUSH_HEAD(name,head,element) \
    ((element)->name##next          = (head)->name##next, \
     (element)->name##prev          = (void *)(head), \
     (head)->name##next->name##prev = (element), \
     (head)->name##next             = (element), \
     (head)->name##count++ \
    )

#define DLL_ADD_AFTER(name,head,after,element) \
    ((element)->name##next            = (after)->name##next, \
     (element)->name##prev            = (after), \
     (after)->name##next->name##prev  = (element), \
     (after)->name##next              = (element), \
     (head)->name##count++ \
    )

#define DLL_ADD_BEFORE(name,head,before,element) \
    ((element)->name##next            = (before), \
     (element)->name##prev            = (before)->name##prev, \
     (before)->name##prev             = (element), \
     (before)->name##prev->name##next = (element), \
     (head)->name##count++ \
    )


#define DLL_REMOVE(name,head,element) \
    ((element)->name##prev->name##next = (element)->name##next, \
     (element)->name##next->name##prev = (element)->name##prev, \
     (element)->name##prev             = 0, \
     (element)->name##next             = 0, \
     (head)->name##count-- \
    )

#define DLL_MOVE_TAIL(name,head,element) \
    ((element)->name##prev->name##next = (element)->name##next, \
     (element)->name##next->name##prev = (element)->name##prev, \
     (element)->name##next             = (void *)(head), \
     (element)->name##prev             = (head)->name##prev, \
     (head)->name##prev->name##next    = (element), \
     (head)->name##prev                = (element) \
    )

#define DLL_POP_HEAD(name, head, element) \
    ((head)->name##count == 0 ? ((element) = NULL, 0) : ((element) = (head)->name##next, DLL_REMOVE(name, (head), (element)), 1))

#define DLL_POP_TAIL(name, head, element) \
    ((head)->name##count == 0 ? ((element) = NULL, 0) : ((element) = (head)->name##prev, DLL_REMOVE(name, (head), (element)), 1))

#define DLL_PEEK_HEAD(name,head) \
    ((head)->name##count == 0?NULL:(head)->name##next)

#define DLL_PEEK_TAIL(name,head) \
    ((head)->name##count == 0?NULL:(head)->name##prev)

#define DLL_COUNT(name,head) \
    ((head)->name##count)

#define DLL_FOREACH(name,head,element) \
      for ((element) = (head)->name##next; \
           (element) != (void *)(head); \
           (element)=(element)->name##next)

#define DLL_FOREACH_REMOVABLE(name,head,element, temp) \
      for ((element) = (head)->name##next, (temp) = (element)->name##next; \
           (element) != (void *)(head); \
           (element) = (temp), (temp) = (temp)->name##next)

#define DLL_FOREACH_REVERSE(name,head,element) \
      for ((element) = (head)->name##prev; \
           (element) != (void *)(head); \
           (element)=(element)->name##prev)

#define DLL_FOREACH_REVERSE_REMOVABLE(name,head,element, temp) \
      for ((element) = (head)->name##prev, (temp) = (element)->name##prev; \
           (element) != (void *)(head); \
           (element) = (temp), (temp) = (temp)->name##prev)


#endif
