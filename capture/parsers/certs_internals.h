#include "arkime.h"

void certinfo_save(BSB *jbsb, ArkimeFieldObject_t *object, ArkimeSession_t *session);
void certinfo_free(ArkimeFieldObject_t *object);
uint32_t certinfo_hash(const void *key);
int certinfo_cmp(const void *keyv, const void *elementv);