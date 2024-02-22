#include "arkime.h"


#define SAVE_STRING_HEAD(HEAD, STR) \
if (HEAD.s_count > 0) { \
    BSB_EXPORT_cstr(*jbsb, "\"" STR "\":["); \
    while (HEAD.s_count > 0) { \
	DLL_POP_HEAD(s_, &HEAD, string); \
	arkime_db_js0n_str(&*jbsb, (uint8_t *)string->str, string->utf8); \
	BSB_EXPORT_u08(*jbsb, ','); \
	g_free(string->str); \
	ARKIME_TYPE_FREE(ArkimeString_t, string); \
    } \
    BSB_EXPORT_rewind(*jbsb, 1); \
    BSB_EXPORT_u08(*jbsb, ']'); \
    BSB_EXPORT_u08(*jbsb, ','); \
}
/******************************************************************************/
LOCAL void arkime_db_js0n_str(BSB *bsb, uint8_t *in, gboolean utf8)
{
    BSB_EXPORT_u08(*bsb, '"');
    while (*in) {
        switch(*in) {
        case '\b':
            BSB_EXPORT_cstr(*bsb, "\\b");
            break;
        case '\n':
            BSB_EXPORT_cstr(*bsb, "\\n");
            break;
        case '\r':
            BSB_EXPORT_cstr(*bsb, "\\r");
            break;
        case '\f':
            BSB_EXPORT_cstr(*bsb, "\\f");
            break;
        case '\t':
            BSB_EXPORT_cstr(*bsb, "\\t");
            break;
        case '"':
            BSB_EXPORT_cstr(*bsb, "\\\"");
            break;
        case '\\':
            BSB_EXPORT_cstr(*bsb, "\\\\");
            break;
        case '/':
            BSB_EXPORT_cstr(*bsb, "\\/");
            break;
        default:
            if(*in < 32) {
                BSB_EXPORT_sprintf(*bsb, "\\u%04x", *in);
            } else if (utf8) {
                if ((*in & 0xf0) == 0xf0) {
                    if (!in[1] || !in[2] || !in[3]) goto end;
                    BSB_EXPORT_ptr(*bsb, in, 4);
                    in += 3;
                } else if ((*in & 0xf0) == 0xe0) {
                    if (!in[1] || !in[2]) goto end;
                    BSB_EXPORT_ptr(*bsb, in, 3);
                    in += 2;
                } else if ((*in & 0xf0) == 0xd0) {
                    if (!in[1]) goto end;
                    BSB_EXPORT_ptr(*bsb, in, 2);
                    in += 1;
                } else {
                    BSB_EXPORT_u08(*bsb, *in);
                }
            } else {
                if(*in & 0x80) {
                    BSB_EXPORT_u08(*bsb, (0xc0 | (*in >> 6)));
                    BSB_EXPORT_u08(*bsb, (0x80 | (*in & 0x3f)));
                } else {
                    BSB_EXPORT_u08(*bsb, *in);
                }
            }
            break;
        }
        in++;
    }

end:
    BSB_EXPORT_u08(*bsb, '"');
}
/******************************************************************************/
void certinfo_save(BSB *jbsb, ArkimeFieldObject_t *object, ArkimeSession_t *session) {
    if (object->object == NULL) {
        return;
    }

    ArkimeCertsInfo_t *ci = (ArkimeCertsInfo_t *)object->object;

    ArkimeString_t *string;

    BSB_EXPORT_u08(*jbsb, '{');

    BSB_EXPORT_sprintf(*jbsb, "\"hash\":\"%s\",", ci->hash);

    if (ci->publicAlgorithm)
        BSB_EXPORT_sprintf(*jbsb, "\"publicAlgorithm\":\"%s\",", ci->publicAlgorithm);
    if (ci->curve)
        BSB_EXPORT_sprintf(*jbsb, "\"curve\":\"%s\",", ci->curve);

    SAVE_STRING_HEAD(ci->issuer.commonName, "issuerCN");
    SAVE_STRING_HEAD(ci->issuer.orgName, "issuerON");
    SAVE_STRING_HEAD(ci->issuer.orgUnit, "issuerOU");
    SAVE_STRING_HEAD(ci->subject.commonName, "subjectCN");
    SAVE_STRING_HEAD(ci->subject.orgName, "subjectON");
    SAVE_STRING_HEAD(ci->subject.orgUnit, "subjectOU");

    if (ci->serialNumber) {
        int k;
        BSB_EXPORT_cstr(*jbsb, "\"serial\":\"");
        for (k = 0; k < ci->serialNumberLen; k++) {
            BSB_EXPORT_sprintf(*jbsb, "%02x", ci->serialNumber[k]);
        }
        BSB_EXPORT_u08(*jbsb, '"');
        BSB_EXPORT_u08(*jbsb, ',');
    }

    if (ci->alt.s_count > 0) { \
        BSB_EXPORT_sprintf(*jbsb, "\"altCnt\":%d,", ci->alt.s_count); \
    }
    SAVE_STRING_HEAD(ci->alt, "alt");

    BSB_EXPORT_sprintf(*jbsb, "\"notBefore\":%" PRId64 ",", ci->notBefore * 1000);
    BSB_EXPORT_sprintf(*jbsb, "\"notAfter\":%" PRId64 ",", ci->notAfter * 1000);
    if (ci->notAfter < ((uint64_t)session->firstPacket.tv_sec)) {
        BSB_EXPORT_sprintf(*jbsb, "\"remainingDays\":%" PRId64 ",", ((int64_t)0));
        BSB_EXPORT_sprintf(*jbsb, "\"remainingSeconds\":%" PRId64 ",", ((int64_t)0));
    } else {
        BSB_EXPORT_sprintf(*jbsb, "\"remainingDays\":%" PRId64 ",", ((int64_t)ci->notAfter - ((uint64_t)session->firstPacket.tv_sec)) / (60 * 60 * 24));
        BSB_EXPORT_sprintf(*jbsb, "\"remainingSeconds\":%" PRId64 ",", ((int64_t)ci->notAfter - ((uint64_t)session->firstPacket.tv_sec)));
    }
    BSB_EXPORT_sprintf(*jbsb, "\"validDays\":%" PRId64 ",", ((int64_t)ci->notAfter - (int64_t)ci->notBefore) / (60 * 60 * 24));
    BSB_EXPORT_sprintf(*jbsb, "\"validSeconds\":%" PRId64 ",", ((int64_t)ci->notAfter - (int64_t)ci->notBefore));

    if (ci->extra) {
        GHashTableIter         iter;
        gpointer               ikey;
        gpointer               ival;
        g_hash_table_iter_init (&iter, ci->extra);
        while (g_hash_table_iter_next (&iter, &ikey, &ival)) {
            BSB_EXPORT_sprintf(*jbsb, "\"%s\":\"%s\",", (char *)ikey, (char *)ival);
        }
    }

    BSB_EXPORT_rewind(*jbsb, 1); // Remove last comma

    BSB_EXPORT_u08(*jbsb, '}');
}

void certinfo_free(ArkimeFieldObject_t *object) {
    if (object->object == NULL) {
        ARKIME_TYPE_FREE(ArkimeFieldObject_t, object);
        return;
    }

    ArkimeCertsInfo_t *ci = (ArkimeCertsInfo_t *)object->object;

    ArkimeString_t *string;

    while (DLL_POP_HEAD(s_, &ci->alt, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &ci->issuer.commonName, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &ci->issuer.orgName, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &ci->issuer.orgUnit, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &ci->subject.commonName, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &ci->subject.orgName, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    while (DLL_POP_HEAD(s_, &ci->subject.orgUnit, string)) {
        g_free(string->str);
        ARKIME_TYPE_FREE(ArkimeString_t, string);
    }

    if (ci->serialNumber)
        free(ci->serialNumber);

    ARKIME_TYPE_FREE(ArkimeCertsInfo_t, ci);
    ARKIME_TYPE_FREE(ArkimeFieldObject_t, object);
}

SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
SUPPRESS_SHIFT
SUPPRESS_INT_CONVERSION
uint32_t certinfo_hash(const void *key) {
    ArkimeFieldObject_t *obj = (ArkimeFieldObject_t *)key;
    
    ArkimeCertsInfo_t *ci;

    if (obj->object == NULL) {
        return 0;
    }
    
    ci = (ArkimeCertsInfo_t *)obj->object;

    if (ci->serialNumberLen == 0) {
        return ((ci->issuer.commonName.s_count << 18) |
                (ci->issuer.orgName.s_count << 12) |
                (ci->subject.commonName.s_count << 6) |
                (ci->subject.orgName.s_count));
    }
    return ((ci->serialNumber[0] << 28) |
            (ci->serialNumber[ci->serialNumberLen - 1] << 24) |
            (ci->issuer.commonName.s_count << 18) |
            (ci->issuer.orgName.s_count << 12) |
            (ci->subject.commonName.s_count << 6) |
            (ci->subject.orgName.s_count));
}

int certinfo_cmp(const void *keyv, const void *elementv) {
    ArkimeFieldObject_t *key      = (ArkimeFieldObject_t *)keyv;
    ArkimeFieldObject_t *element = (ArkimeFieldObject_t *)elementv;

    ArkimeCertsInfo_t *keyCI, *elementCI;

    if (key->object == NULL || element->object == NULL) {
        return 0;
    }

    keyCI = (ArkimeCertsInfo_t *)key->object;
    elementCI = (ArkimeCertsInfo_t *)element->object;

    // Make sure all the easy things to check are the same
    if ( !((keyCI->serialNumberLen == elementCI->serialNumberLen) &&
           (memcmp(keyCI->serialNumber, elementCI->serialNumber, elementCI->serialNumberLen) == 0) &&
           (keyCI->issuer.commonName.s_count == elementCI->issuer.commonName.s_count) &&
           (keyCI->issuer.orgName.s_count == elementCI->issuer.orgName.s_count) &&
           (keyCI->issuer.orgUnit.s_count == elementCI->issuer.orgUnit.s_count) &&
           (keyCI->subject.commonName.s_count == elementCI->subject.commonName.s_count) &&
           (keyCI->subject.orgName.s_count == elementCI->subject.orgName.s_count) &&
           (keyCI->subject.orgUnit.s_count == elementCI->subject.orgUnit.s_count)
          )
       ) {

        return 0;
    }

    // Now see if all the other items are the same

    ArkimeString_t *kstr, *estr;
    for (kstr = keyCI->issuer.commonName.s_next, estr = elementCI->issuer.commonName.s_next;
         kstr != (void *) & (keyCI->issuer.commonName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = keyCI->issuer.orgName.s_next, estr = elementCI->issuer.orgName.s_next;
         kstr != (void *) & (keyCI->issuer.orgName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = keyCI->issuer.orgUnit.s_next, estr = elementCI->issuer.orgUnit.s_next;
         kstr != (void *) & (keyCI->issuer.orgUnit);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = keyCI->subject.commonName.s_next, estr = elementCI->subject.commonName.s_next;
         kstr != (void *) & (keyCI->subject.commonName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = keyCI->subject.orgName.s_next, estr = elementCI->subject.orgName.s_next;
         kstr != (void *) & (keyCI->subject.orgName);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    for (kstr = keyCI->subject.orgUnit.s_next, estr = elementCI->subject.orgUnit.s_next;
         kstr != (void *) & (keyCI->subject.orgUnit);
         kstr = kstr->s_next, estr = estr->s_next) {

        if (strcmp(kstr->str, estr->str) != 0)
            return 0;
    }

    return 1;
}