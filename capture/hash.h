/******************************************************************************/
/* hash.h  -- Hashtable
 *
 * Simple macro based hashtable using DLL buckets with counts that supports 
 * having a single item in multiple lists or hash tables by using a prefix.  
 * Every element in the hash table needs to follow the DLL rules.
 *
 * To Use:
 * Create item structure and optional head structure for use with a DLL
 * Create the key function and cmp function.
 * Use HASH_VAR to declare the actual variable.  Can be global or in a structure
 * Use HASH_INIT to initialze the hashtable
 *
 * A key can also just be the element
 *
 * The same WARNING in dll.h applies since hash.h just uses DLL
 */

#ifndef _HASH_HEADER
#define _HASH_HEADER

/* Convert a key to a u32 hash value */
typedef uint32_t (* HASH_KEY_FUNC)(const void *key);

/* Given a key does it match the element */
typedef int (* HASH_CMP_FUNC)(const void *key, const void *element);

/* buckets is last on purpose so functions can be written */
#define HASH_VAR(name, varname, elementtype, num) \
   struct \
   { \
       HASH_KEY_FUNC hash; \
       HASH_CMP_FUNC cmp; \
       int size; \
       int count; \
       elementtype buckets[num]; \
   } varname

#define HASH_INIT(name, varname, hashfunc, cmpfunc) \
  do { \
       int i; \
       (varname).size = sizeof((varname).buckets)/sizeof((varname).buckets[0]); \
       (varname).hash = hashfunc; \
       (varname).cmp = cmpfunc; \
       for (i = 0; i < (varname).size; i++) { \
           DLL_INIT(name, &((varname).buckets[i])); \
       } \
     } while (0)


#define HASH_ADD(name, varname, key, element) \
  do { \
      element->name##bucket = (varname).hash(key) % (varname).size; \
      DLL_PUSH_TAIL(name, &((varname).buckets[element->name##bucket]), element); \
      (varname).count++; \
  } while(0)

#define HASH_REMOVE(name, varname, element) \
  do { \
      DLL_REMOVE(name, &((varname).buckets[element->name##bucket]), element); \
      (varname).count--; \
  } while(0)

#define HASH_FIND(name, varname, key, element) \
  do { \
      int b = (varname).hash(key) % (varname).size; \
      for (element = (varname).buckets[b].name##next; element != (void*)&((varname).buckets[b]); element = element->name##next) { \
          if ((varname).cmp(key, element)) \
              break; \
      } \
      if (element == (void *)&((varname).buckets[b])) element = 0; \
  } while(0)

#define HASH_COUNT(name, varname) ((varname).count)

#define HASH_FORALL_POP_HEAD(name, varname, element, code) \
  do { \
      int  _##name##b; \
      for ( _##name##b = 0;  _##name##b < (varname).size;  _##name##b++) {\
          while((varname).buckets[_##name##b].name##count) { \
              DLL_POP_HEAD(name, &((varname).buckets[_##name##b]), element); \
              (varname).count--; \
              code \
          } \
      } \
  } while(0)

#define HASH_FORALL(name, varname, element, code) \
  do { \
      int  _##name##b; \
      for ( _##name##b = 0;  _##name##b < (varname).size;  _##name##b++) {\
          for (element = (varname).buckets[_##name##b].name##next; element != (void*)&((varname).buckets[_##name##b]); element = element->name##next) { \
              code \
          } \
      } \
  } while(0)

#endif
