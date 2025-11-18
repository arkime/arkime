/******************************************************************************/
/* python.c  -- Python integration for Arkime
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkimeconfig.h"

#ifndef HAVE_PYTHON
void arkime_python_init() {}
void arkime_python_exit() {}
#else
#include "arkime.h"
#include "Python.h"
#include <arpa/inet.h>

extern ArkimeConfig_t        config;

typedef struct ArkimePyCbMap {
    PyObject *cb[MAX_INTERFACES * MAX_THREADS_PER_INTERFACE];
} ArkimePyCbMap_t;

GHashTable *arkimePyCbMap = NULL;

LOCAL ARKIME_LOCK_DEFINE(singleLock);

LOCAL PyThreadState *mainThreadState;
LOCAL PyThreadState *packetThreadState[ARKIME_MAX_PACKET_THREADS];
LOCAL PyThreadState *readerThreadState[MAX_INTERFACES * MAX_THREADS_PER_INTERFACE];

LOCAL gboolean disablePython;
LOCAL GPtrArray *filesLoaded;
/******************************************************************************/
typedef struct {
    int dummy;
} ArkimeState;

extern __thread int arkimePacketThread;
LOCAL __thread int arkimeReaderThread = -1;
LOCAL int loadingThread = -1;

/******************************************************************************/
/**
 * Need to save the py callback per thread. We use the register name + the callback name as the key.
 */
LOCAL ArkimePyCbMap_t *arkime_python_save_callback(const char *name, PyObject *py_callback_obj)
{
    char key[100];

    PyObject *name_obj = PyObject_GetAttrString(py_callback_obj, "__name__");
    if (name_obj != NULL) {
        const char *pyname = PyUnicode_AsUTF8(name_obj);
        snprintf(key, sizeof(key), "%s:%s", name, pyname);
    } else {
        g_strlcpy(key, name, sizeof(key));
    }
    Py_XDECREF(name_obj);

    ARKIME_LOCK(singleLock);
    ArkimePyCbMap_t *map = g_hash_table_lookup(arkimePyCbMap, key);
    int isNew = map == NULL;

    if (!map) {
        map = ARKIME_TYPE_ALLOC0(ArkimePyCbMap_t);
        g_hash_table_insert(arkimePyCbMap, g_strdup(key), map);
    }
    ARKIME_UNLOCK(singleLock);

    // Support registering callbacks in the main thread, packet threads, and reader threads.
    if (arkimeReaderThread >= 0) {
        map->cb[arkimeReaderThread] = py_callback_obj;
    } else if (arkimePacketThread >= 0) {
        map->cb[arkimePacketThread] = py_callback_obj;
    } else {
        map->cb[loadingThread] = py_callback_obj;
    }
    if (isNew)
        return map;
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Arkime
///////////////////////////////////////////////////////////////////////////////
/******************************************************************************/
LOCAL void arkime_python_classify_cb(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *uw)
{
    PyEval_RestoreThread(packetThreadState[arkimePacketThread]);

    ArkimePyCbMap_t *map = (ArkimePyCbMap_t *)uw;
    PyObject *py_callback_obj = map->cb[arkimePacketThread];
    PyObject *py_packet_memview = PyMemoryView_FromMemory((char *)data, len, PyBUF_READ);
    PyObject *py_session_opaque_ptr = PyLong_FromVoidPtr(session);

    PyObject *py_args = Py_BuildValue("(OOii)", py_session_opaque_ptr, py_packet_memview, len, which);

    if (!py_args) {
        PyErr_Print();
        LOGEXIT("Error building arguments tuple for Python callback");
    }

    PyObject *result = PyObject_CallObject(py_callback_obj, py_args);
    if (result == NULL) {
        PyErr_Print(); // Print any unhandled Python exceptions from the callback
        LOG("Error calling Python callback function from C");
    } else {
        Py_DECREF(result); // Decrement reference count of the Python result object
    }

    Py_XDECREF(py_packet_memview);
    Py_XDECREF(py_args);
    Py_XDECREF(py_session_opaque_ptr);

    PyEval_SaveThread();
}
/******************************************************************************/
LOCAL PyObject *arkime_python_register_tcp_classifier(PyObject UNUSED(*self), PyObject *args)
{
    if (arkimePacketThread == -1 && loadingThread == -1) {
        Py_RETURN_NONE;
    }

    const char *name_str;
    int offset;
    PyObject *py_match_bytes_obj;
    PyObject *py_callback_obj;

    const uint8_t *match_bytes = NULL;
    Py_ssize_t match_len = 0;

    // s: name (Python string -> C char*)
    // i: offset (Python int -> C int)
    // S: py_match_bytes_obj (Python bytes object -> C PyObject*)
    // O: py_callback_obj (Python object -> C PyObject*)
    if (!PyArg_ParseTuple(args, "siSO", &name_str, &offset, &py_match_bytes_obj, &py_callback_obj)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    if (!PyBytes_Check(py_match_bytes_obj)) {
        PyErr_SetString(PyExc_TypeError, "Match argument must be a bytes object.");
        return NULL;
    }

    if (PyBytes_AsStringAndSize(py_match_bytes_obj, (char **)&match_bytes, &match_len) == -1) {
        return NULL;
    }

    if (!PyCallable_Check(py_callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback must be a callable Python object.");
        return NULL;
    }
    Py_INCREF(py_callback_obj);

    ArkimePyCbMap_t *map = arkime_python_save_callback(name_str, py_callback_obj);

    if (map) {
        arkime_parsers_classifier_register_tcp (
            name_str,
            map,
            offset,
            match_bytes,
            (int)match_len,
            arkime_python_classify_cb
        );
    }

    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_register_udp_classifier(PyObject UNUSED(*self), PyObject *args)
{
    if (arkimePacketThread == -1 && loadingThread == -1) {
        Py_RETURN_NONE;
    }

    const char *name_str;
    int offset;
    PyObject *py_match_bytes_obj;
    PyObject *py_callback_obj;

    const uint8_t *match_bytes = NULL;
    Py_ssize_t match_len = 0;

    // s: name (Python string -> C char*)
    // i: offset (Python int -> C int)
    // S: py_match_bytes_obj (Python bytes object -> C PyObject*)
    // O: py_callback_obj (Python object -> C PyObject*)
    if (!PyArg_ParseTuple(args, "siSO", &name_str, &offset, &py_match_bytes_obj, &py_callback_obj)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    if (!PyBytes_Check(py_match_bytes_obj)) {
        PyErr_SetString(PyExc_TypeError, "Match argument must be a bytes object.");
        return NULL;
    }

    if (PyBytes_AsStringAndSize(py_match_bytes_obj, (char **)&match_bytes, &match_len) == -1) {
        return NULL;
    }

    if (!PyCallable_Check(py_callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback must be a callable Python object.");
        return NULL;
    }
    Py_INCREF(py_callback_obj);

    ArkimePyCbMap_t *map = arkime_python_save_callback(name_str, py_callback_obj);

    if (map)
        arkime_parsers_classifier_register_udp (
            name_str,
            map,
            offset,
            match_bytes,
            (int)match_len,
            arkime_python_classify_cb
        );

    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_register_sctp_classifier(PyObject UNUSED(*self), PyObject *args)
{
    if (arkimePacketThread == -1 && loadingThread == -1) {
        Py_RETURN_NONE;
    }

    const char *name_str;
    int offset;
    PyObject *py_match_bytes_obj;
    PyObject *py_callback_obj;

    const uint8_t *match_bytes = NULL;
    Py_ssize_t match_len = 0;

    // s: name (Python string -> C char*)
    // i: offset (Python int -> C int)
    // S: py_match_bytes_obj (Python bytes object -> C PyObject*)
    // O: py_callback_obj (Python object -> C PyObject*)
    if (!PyArg_ParseTuple(args, "siSO", &name_str, &offset, &py_match_bytes_obj, &py_callback_obj)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    if (!PyBytes_Check(py_match_bytes_obj)) {
        PyErr_SetString(PyExc_TypeError, "Match argument must be a bytes object.");
        return NULL;
    }

    if (PyBytes_AsStringAndSize(py_match_bytes_obj, (char **)&match_bytes, &match_len) == -1) {
        return NULL;
    }

    if (!PyCallable_Check(py_callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback must be a callable Python object.");
        return NULL;
    }
    Py_INCREF(py_callback_obj);

    ArkimePyCbMap_t *map = arkime_python_save_callback(name_str, py_callback_obj);

    if (map)
        arkime_parsers_classifier_register_sctp (
            name_str,
            map,
            offset,
            match_bytes,
            (int)match_len,
            arkime_python_classify_cb
        );

    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_register_sctp_protocol_classifier(PyObject UNUSED(*self), PyObject *args)
{
    if (arkimePacketThread == -1 && loadingThread == -1) {
        Py_RETURN_NONE;
    }

    const char *name_str;
    int protocol;
    PyObject *py_callback_obj;

    // s: name (Python string -> C char*)
    // i: protocol (Python int -> C int)
    // O: py_callback_obj (Python object -> C PyObject*)
    if (!PyArg_ParseTuple(args, "siO", &name_str, &protocol, &py_callback_obj)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    if (!PyCallable_Check(py_callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback must be a callable Python object.");
        return NULL;
    }
    Py_INCREF(py_callback_obj);

    ArkimePyCbMap_t *map = arkime_python_save_callback(name_str, py_callback_obj);

    if (map)
        arkime_parsers_classifier_register_sctp_protocol (
            name_str,
            map,
            protocol,
            arkime_python_classify_cb
        );

    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_register_port_classifier(PyObject UNUSED(*self), PyObject *args)
{
    if (arkimePacketThread == -1 && loadingThread == -1) {
        Py_RETURN_NONE;
    }

    const char *name_str;
    int port;
    int type;
    PyObject *py_callback_obj;

    // s: name
    // i: port
    // i: type
    // O: py_callback_obj (Python object -> C PyObject*)
    if (!PyArg_ParseTuple(args, "siiO", &name_str, &port, &type, &py_callback_obj)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    if (!PyCallable_Check(py_callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback must be a callable Python object.");
        return NULL;
    }
    Py_INCREF(py_callback_obj);

    ArkimePyCbMap_t *map = arkime_python_save_callback(name_str, py_callback_obj);

    if (map)
        arkime_parsers_classifier_register_port (
            name_str,
            map,
            port,
            type,
            arkime_python_classify_cb
        );

    Py_RETURN_NONE;
}

/******************************************************************************/
// Both presave/save use same callback
LOCAL uint32_t arkime_python_session_save_cb (ArkimeSession_t *session, const uint8_t UNUSED(*data), int len, void UNUSED(*uw), void *cbuw)
{
    PyEval_RestoreThread(packetThreadState[arkimePacketThread]);

    ArkimePyCbMap_t *map = cbuw;
    PyObject *py_callback_obj = map->cb[arkimePacketThread];
    PyObject *py_session_opaque_ptr = PyLong_FromVoidPtr(session);

    PyObject *py_args = Py_BuildValue("(Oi)", py_session_opaque_ptr, len);

    if (!py_args) {
        PyErr_Print();
        LOGEXIT("Error building arguments tuple for Python callback");
    }

    PyObject *result = PyObject_CallObject(py_callback_obj, py_args);
    if (result == NULL) {
        PyErr_Print(); // Print any unhandled Python exceptions from the callback
        LOG("Error calling Python callback function from C");
    } else {
        Py_DECREF(result); // Decrement reference count of the Python result object
    }

    Py_XDECREF(py_args);
    Py_XDECREF(py_session_opaque_ptr);

    PyEval_SaveThread();
    return 0;
}

/******************************************************************************/
LOCAL PyObject *arkime_python_register_save(PyObject UNUSED(*self), PyObject *args)
{
    if (arkimePacketThread == -1 && loadingThread == -1) {
        Py_RETURN_NONE;
    }

    PyObject *py_callback_obj;

    // O: py_callback_obj (Python object -> C PyObject*)
    if (!PyArg_ParseTuple(args, "O", &py_callback_obj)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    if (!PyCallable_Check(py_callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback must be a callable Python object.");
        return NULL;
    }
    Py_INCREF(py_callback_obj);

    ArkimePyCbMap_t *map = arkime_python_save_callback(":save", py_callback_obj);

    if (map)
        arkime_parsers_add_named_func2("arkime_session_save", arkime_python_session_save_cb, map);
    Py_RETURN_NONE;
}

/******************************************************************************/
LOCAL PyObject *arkime_python_register_pre_save(PyObject UNUSED(*self), PyObject *args)
{
    if (arkimePacketThread == -1 && loadingThread == -1) {
        Py_RETURN_NONE;
    }

    PyObject *py_callback_obj;

    // O: py_callback_obj (Python object -> C PyObject*)
    if (!PyArg_ParseTuple(args, "O", &py_callback_obj)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    if (!PyCallable_Check(py_callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback must be a callable Python object.");
        return NULL;
    }
    Py_INCREF(py_callback_obj);

    ArkimePyCbMap_t *map = arkime_python_save_callback(":pre_save", py_callback_obj);

    if (map)
        arkime_parsers_add_named_func2("arkime_session_pre_save", arkime_python_session_save_cb, map);
    Py_RETURN_NONE;
}

/******************************************************************************/
LOCAL PyObject *arkime_python_field_define(PyObject UNUSED(*self), PyObject *args)
{
    const char *field;
    const char *def;

    // s: field
    // s: def
    if (!PyArg_ParseTuple(args, "ss", &field, &def)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    if (arkimePacketThread != -1) {
        PyErr_SetString(PyExc_RuntimeError, "arkime.field_define() can only be called at start up.");
        return NULL;
    }
    if (loadingThread != 0) {
        return PyLong_FromLong(arkime_field_by_exp(field));
    }
    return PyLong_FromLong(arkime_field_define_text_full((char *)field, def, NULL));
}
/******************************************************************************/
LOCAL PyObject *arkime_python_field_get(PyObject UNUSED(*self), PyObject *args)
{
    const char *field;

    // s: field
    if (!PyArg_ParseTuple(args, "s", &field)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    return PyLong_FromLong(arkime_field_by_exp(field));
}
/******************************************************************************/
LOCAL PyMethodDef arkime_methods[] = {
    { "register_tcp_classifier", arkime_python_register_tcp_classifier, METH_VARARGS, NULL },
    { "register_udp_classifier", arkime_python_register_udp_classifier, METH_VARARGS, NULL },
    { "register_sctp_classifier", arkime_python_register_sctp_classifier, METH_VARARGS, NULL },
    { "register_sctp_protocol_classifier", arkime_python_register_sctp_protocol_classifier, METH_VARARGS, NULL },
    { "register_port_classifier", arkime_python_register_port_classifier, METH_VARARGS, NULL },
    { "register_save", arkime_python_register_save, METH_VARARGS, NULL },
    { "register_pre_save", arkime_python_register_pre_save, METH_VARARGS, NULL },
    { "field_define", arkime_python_field_define, METH_VARARGS, NULL },
    { "field_get", arkime_python_field_get, METH_VARARGS, NULL },
    {NULL, NULL, 0, NULL}
};

LOCAL struct PyModuleDef arkime_module = {
    PyModuleDef_HEAD_INIT,
    "arkime",
    NULL,     // module documentation, may be NULL
    sizeof(ArkimeState),
    arkime_methods,
    NULL,     // m_slots
    NULL,     // m_traverse
    NULL,     // m_clear
    NULL      // m_free
};

// Function to initialize our arkime C module
PyMODINIT_FUNC PyInit_arkime(void)
{
    PyObject* m = PyModule_Create(&arkime_module);
    if (m == NULL) {
        return NULL;
    }
    // Get the per-interpreter state pointer and initialize it
    ArkimeState* state = (ArkimeState*)PyModule_GetState(m);
    if (state == NULL) {
        // This should not happen if m_size is set correctly
        Py_DECREF(m);
        return NULL;
    }
    state->dummy = 0;

    return m;
}
///////////////////////////////////////////////////////////////////////////////
// Arkime Session
///////////////////////////////////////////////////////////////////////////////
/******************************************************************************/
typedef struct {
    int dummy_value; // Example placeholder for session-specific data
} ArkimeSessionState;

/******************************************************************************/
LOCAL int arkime_python_session_parsers_cb(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    PyEval_RestoreThread(packetThreadState[arkimePacketThread]);

    PyObject *py_callback_obj = (PyObject *)uw;
    PyObject *py_packet_memview = PyMemoryView_FromMemory((char *)data, remaining, PyBUF_READ);
    PyObject *py_session_opaque_ptr = PyLong_FromVoidPtr(session);

    PyObject *py_args = Py_BuildValue("(OOii)", py_session_opaque_ptr, py_packet_memview, remaining, which);

    if (!py_args) {
        PyErr_Print();
        LOGEXIT("Error building arguments tuple for Python callback");
    }

    PyObject *result = PyObject_CallObject(py_callback_obj, py_args);
    int resultn = 0;
    if (result == NULL) {
        PyErr_Print(); // Print any unhandled Python exceptions from the callback
        LOG("Error calling Python callback function from C");
        resultn = -1; // Indicate an error
    } else {
        if (PyLong_Check(result))
            resultn = PyLong_AsLong(result);
        Py_DECREF(result); // Decrement reference count of the Python result object
    }

    Py_XDECREF(py_packet_memview);
    Py_XDECREF(py_args);
    Py_XDECREF(py_session_opaque_ptr);

    PyEval_SaveThread();

    return resultn;
}

/******************************************************************************/
LOCAL void arkime_python_session_parsers_free_cb(ArkimeSession_t UNUSED(*session), void *uw)
{
    PyObject *py_callback_obj = (PyObject *)uw;
    Py_DECREF(py_callback_obj);
}
/******************************************************************************/
LOCAL PyObject *arkime_python_session_register_parser(PyObject UNUSED(*self), PyObject *args)
{
    PyObject *py_session_obj;
    PyObject *py_callback_obj;

    // O: session
    // O: callback
    if (!PyArg_ParseTuple(args, "OO", &py_session_obj, &py_callback_obj)) {
        return NULL;
    }

    if (!PyCallable_Check(py_callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback must be a callable Python object.");
        return NULL;
    }
    Py_INCREF(py_callback_obj);

    arkime_parsers_register2(
        (ArkimeSession_t *)PyLong_AsVoidPtr(py_session_obj), // Convert PyObject* to ArkimeSession_t*
        arkime_python_session_parsers_cb,
        py_callback_obj,
        arkime_python_session_parsers_free_cb,
        NULL);
    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_session_add_tag(PyObject UNUSED(*self), PyObject *args)
{
    const char *tag_str;
    PyObject *py_session_obj;

    // O: session
    // s: tag
    if (!PyArg_ParseTuple(args, "Os", &py_session_obj, &tag_str)) {
        return NULL;
    }
    arkime_session_add_tag(
        (ArkimeSession_t *)PyLong_AsVoidPtr(py_session_obj), // Convert PyObject* to ArkimeSession_t*
        tag_str
    );
    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_session_add_protocol(PyObject UNUSED(*self), PyObject *args)
{
    const char *protocol_str;
    PyObject *py_session_obj;

    if (!PyArg_ParseTuple(args, "Os", &py_session_obj, &protocol_str)) {
        return NULL;
    }
    arkime_session_add_protocol(
        (ArkimeSession_t *)PyLong_AsVoidPtr(py_session_obj), // Convert PyObject* to ArkimeSession_t*
        protocol_str
    );
    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_session_has_protocol(PyObject UNUSED(*self), PyObject *args)
{
    const char *protocol_str;
    PyObject *py_session_obj;

    if (!PyArg_ParseTuple(args, "Os", &py_session_obj, &protocol_str)) {
        return NULL;
    }
    if (arkime_session_has_protocol( (ArkimeSession_t *)PyLong_AsVoidPtr(py_session_obj), protocol_str)) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}
/******************************************************************************/
LOCAL PyObject *arkime_python_session_add_int(PyObject UNUSED(*self), PyObject *args)
{
    PyObject *py_session_obj;
    const char *field;
    int value;

    if (!PyArg_ParseTuple(args, "Osi", &py_session_obj, &field, &value)) {
        return NULL;
    }

    int pos;
    if (isdigit(field[0]))
        pos = atoi(field);
    else
        pos = arkime_field_by_exp(field);

    gboolean result = arkime_field_int_add(pos, (ArkimeSession_t *)PyLong_AsVoidPtr(py_session_obj), value);

    if (result) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}
/******************************************************************************/
LOCAL PyObject *arkime_python_session_add_string(PyObject UNUSED(*self), PyObject *args)
{
    PyObject *py_session_obj;
    const char *field;
    const char *value;

    if (!PyArg_ParseTuple(args, "Oss", &py_session_obj, &field, &value)) {
        return NULL;
    }

    int pos;
    if (isdigit(field[0]))
        pos = atoi(field);
    else
        pos = arkime_field_by_exp(field);

    const char *result = arkime_field_string_add(pos, (ArkimeSession_t *)PyLong_AsVoidPtr(py_session_obj), value, -1, TRUE);

    if (result) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}
/******************************************************************************/
LOCAL PyObject *arkime_python_session_incref(PyObject UNUSED(*self), PyObject *args)
{
    PyObject *py_session_obj;

    if (!PyArg_ParseTuple(args, "O", &py_session_obj)) {
        return NULL;
    }

    arkime_session_incr_outstanding((ArkimeSession_t *)PyLong_AsVoidPtr(py_session_obj));
    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_session_decref(PyObject UNUSED(*self), PyObject *args)
{
    PyObject *py_session_obj;

    if (!PyArg_ParseTuple(args, "O", &py_session_obj)) {
        return NULL;
    }

    arkime_session_decr_outstanding((ArkimeSession_t *)PyLong_AsVoidPtr(py_session_obj));
    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_session_get(PyObject UNUSED(*self), PyObject *args)
{
    PyObject                    *py_list;
    const GArray                *iarray;
    GHashTable                  *ghash;
    GHashTableIter               iter;
    gpointer                     ikey;
    PyObject                    *py_session_obj;
    const char                  *field;
    const ArkimeStringHashStd_t *shash;
    ArkimeString_t              *hstring = NULL;
    const ArkimeIntHashStd_t    *ihash;
    ArkimeInt_t                 *hint;
    struct in6_addr             *ip6;
    char                         ipstr[INET6_ADDRSTRLEN + 10];

    if (!PyArg_ParseTuple(args, "Os", &py_session_obj, &field)) {
        return NULL;
    }

    ArkimeSession_t *session = (ArkimeSession_t *)PyLong_AsVoidPtr(py_session_obj);

    int pos;
    if (isdigit(field[0]))
        pos = atoi(field);
    else {
        pos = arkime_field_by_exp(field);
    }

    if (pos >= config.minInternalField && config.fields[pos] && config.fields[pos]->getCb) {
        void *value = config.fields[pos]->getCb(session, pos);

        if (!value)
            Py_RETURN_NONE;

        switch (config.fields[pos]->type) {
        case ARKIME_FIELD_TYPE_IP: {
            ip6 = (struct in6_addr *)value;

            if (IN6_IS_ADDR_V4MAPPED(ip6)) {
                arkime_ip4tostr(ARKIME_V6_TO_V4(*ip6), ipstr, sizeof(ipstr));
            } else {
                inet_ntop(AF_INET6, ip6, ipstr, sizeof(ipstr));
            }
            return PyUnicode_FromString(ipstr);
        }

        case ARKIME_FIELD_TYPE_INT:
            return PyLong_FromLong((long)value);

        case ARKIME_FIELD_TYPE_STR:
            return PyUnicode_FromString((const char *)value);

        case ARKIME_FIELD_TYPE_STR_ARRAY: {
            GPtrArray *sarray = (GPtrArray *)value;

            py_list = PyList_New(sarray->len);
            for (int i = 0; i < (int)sarray->len; i++) {
                const gchar *c_str = (const char *)g_ptr_array_index(sarray, i);
                PyObject *py_str = PyUnicode_DecodeUTF8(c_str, strlen(c_str), "strict");
                PyList_SetItem(py_list, i, py_str);
            }
            return py_list;
        }
        case ARKIME_FIELD_TYPE_STR_GHASH: {
            ghash = (GHashTable *)value;
            g_hash_table_iter_init (&iter, ghash);

            py_list = PyList_New(g_hash_table_size(ghash));
            int i = 0;
            while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
                PyObject *py_str = PyUnicode_DecodeUTF8(ikey, strlen(ikey), "strict");
                PyList_SetItem(py_list, i, py_str);
                i++;
            }
            return py_list;
        }
        default:
            // Unsupported
            break;
        } /* switch */
        Py_RETURN_NONE;
    }

    // This session doesn't have this many fields or field isnt set
    if (pos < 0 || pos > session->maxFields || !session->fields[pos])
        Py_RETURN_NONE;

    switch (config.fields[pos]->type) {
    case ARKIME_FIELD_TYPE_INT:
        return PyLong_FromLong((long)session->fields[pos]->i);

    case ARKIME_FIELD_TYPE_INT_ARRAY:
    case ARKIME_FIELD_TYPE_INT_ARRAY_UNIQUE:
        iarray = session->fields[pos]->iarray;

        py_list = PyList_New(iarray->len);
        for (int i = 0; i < (int)iarray->len; i++) {
            PyList_SetItem(py_list, i, PyLong_FromUnsignedLong(g_array_index(iarray, uint32_t, i)));
        }
        return py_list;

    case ARKIME_FIELD_TYPE_INT_HASH: {
        ihash = session->fields[pos]->ihash;
        py_list = PyList_New(HASH_COUNT(i_, *ihash));
        int i = 0;
        HASH_FORALL2(i_, *ihash, hint) {
            PyList_SetItem(py_list, i, PyLong_FromUnsignedLong(hint->i_hash));
            i++;
        }
        return py_list;
    }

    case ARKIME_FIELD_TYPE_INT_GHASH: {
        ghash = session->fields[pos]->ghash;
        g_hash_table_iter_init (&iter, ghash);

        py_list = PyList_New(g_hash_table_size(ghash));
        int i = 0;
        while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
            PyList_SetItem(py_list, i, PyLong_FromUnsignedLong((unsigned long)ikey));
            i++;
        }
        return py_list;
    }

    case ARKIME_FIELD_TYPE_FLOAT:
        return PyFloat_FromDouble(session->fields[pos]->f);

    case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
        py_list = PyList_New(session->fields[pos]->farray->len);
        for (int i = 0; i < (int)session->fields[pos]->farray->len; i++) {
            PyList_SetItem(py_list, i, PyFloat_FromDouble(g_array_index(session->fields[pos]->farray, float, i)));
        }
        return py_list;

    case ARKIME_FIELD_TYPE_FLOAT_GHASH: {
        ghash = session->fields[pos]->ghash;
        g_hash_table_iter_init (&iter, ghash);

        py_list = PyList_New(g_hash_table_size(ghash));
        int i = 0;
        while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
            PyList_SetItem(py_list, i, PyFloat_FromDouble(*(double *)ikey));
            i++;
        }
        return py_list;
    }

    case ARKIME_FIELD_TYPE_IP:
        ip6 = (struct in6_addr *)session->fields[pos]->ip;
        if (IN6_IS_ADDR_V4MAPPED(ip6)) {
            arkime_ip4tostr(ARKIME_V6_TO_V4(*ip6), ipstr, sizeof(ipstr));
        } else {
            inet_ntop(AF_INET6, ip6, ipstr, sizeof(ipstr));
        }
        return PyUnicode_FromString(ipstr);

    case ARKIME_FIELD_TYPE_IP_GHASH: {
        ghash = session->fields[pos]->ghash;
        g_hash_table_iter_init (&iter, ghash);

        py_list = PyList_New(g_hash_table_size(ghash));
        int i = 0;
        while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
            ip6 = (struct in6_addr *)ikey;
            if (IN6_IS_ADDR_V4MAPPED(ip6)) {
                arkime_ip4tostr(ARKIME_V6_TO_V4(*ip6), ipstr, sizeof(ipstr));
            } else {
                inet_ntop(AF_INET6, ip6, ipstr, sizeof(ipstr));
            }
            PyList_SetItem(py_list, i, PyUnicode_FromString(ipstr));
            i++;
        }
        return py_list;
    }

    case ARKIME_FIELD_TYPE_STR:
        return PyUnicode_FromString(session->fields[pos]->str);

    case ARKIME_FIELD_TYPE_STR_ARRAY:
        py_list = PyList_New(session->fields[pos]->sarray->len);
        for (int i = 0; i < (int)session->fields[pos]->sarray->len; i++) {
            PyList_SetItem(py_list, i, PyUnicode_DecodeUTF8((char *)g_ptr_array_index(session->fields[pos]->sarray, i), -1, "strict"));
        }
        return py_list;

    case ARKIME_FIELD_TYPE_STR_HASH: {
        shash = session->fields[pos]->shash;
        py_list = PyList_New(HASH_COUNT(s_, *shash));
        int i = 0;

        HASH_FORALL2(s_, *shash, hstring) {
            PyList_SetItem(py_list, i, PyUnicode_DecodeUTF8(hstring->str, -1, "strict"));
            i++;
        }
        return py_list;
    }

    case ARKIME_FIELD_TYPE_STR_GHASH: {
        ghash = session->fields[pos]->ghash;
        g_hash_table_iter_init (&iter, ghash);

        py_list = PyList_New(g_hash_table_size(ghash));
        int i = 0;
        while (g_hash_table_iter_next (&iter, &ikey, NULL)) {
            PyList_SetItem(py_list, i, PyUnicode_DecodeUTF8((char *)ikey, -1, "strict"));
            i++;
        }

        return py_list;
    }
    case ARKIME_FIELD_TYPE_OBJECT:
        // Unsupported
        break;
    } /* switch */

    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyMethodDef arkime_session_methods[] = {
    { "register_parser", arkime_python_session_register_parser, METH_VARARGS, NULL },
    { "add_tag", arkime_python_session_add_tag, METH_VARARGS, NULL },
    { "add_protocol", arkime_python_session_add_protocol, METH_VARARGS, NULL },
    { "has_protocol", arkime_python_session_has_protocol, METH_VARARGS, NULL },
    { "add_int", arkime_python_session_add_int, METH_VARARGS, NULL },
    { "add_string", arkime_python_session_add_string, METH_VARARGS, NULL },
    { "incref", arkime_python_session_incref, METH_VARARGS, NULL },
    { "decref", arkime_python_session_decref, METH_VARARGS, NULL },
    { "get", arkime_python_session_get, METH_VARARGS, NULL },
    {NULL, NULL, 0, NULL} // Sentinel
};
/******************************************************************************/
LOCAL struct PyModuleDef arkime_session_module = {
    PyModuleDef_HEAD_INIT,
    "arkime_session", // name of module (lowercase with underscore)
    NULL,            // module documentation, may be NULL
    sizeof(ArkimeSessionState), // m_size: Size of per-interpreter state for this module
    arkime_session_methods,
    NULL,     // m_slots
    NULL,     // m_traverse
    NULL,     // m_clear
    NULL      // m_free
};
/******************************************************************************/
// Function to initialize our arkime_session C module
PyMODINIT_FUNC PyInit_arkime_session(void)
{
    PyObject* m = PyModule_Create(&arkime_session_module);
    if (m == NULL) {
        return NULL;
    }
    // Get the per-interpreter state pointer and initialize it
    ArkimeSessionState* state = (ArkimeSessionState*)PyModule_GetState(m);
    if (state == NULL) {
        Py_DECREF(m);
        return NULL;
    }
    state->dummy_value = 0; // Initialize dummy value

    return m;
}
/******************************************************************************/
LOCAL void arkime_python_packet_load_file(const char *file)
{
    // Make sure all the threads have been initialized before proceeding
    for (int i = 0; i < config.packetThreads; i++) {
        while (packetThreadState[i] == NULL) {
            // Wait for the thread state to be initialized
            usleep(1000); // Sleep for 1ms to avoid busy waiting
        }
    }

    // Acquire the GIL for this thread's initial interpreter (main interpreter)
    PyGILState_STATE gstate = PyGILState_Ensure();

    for (int i = 0; i < config.packetThreads; i++) {
        loadingThread = i;
        PyThreadState* target_tstate = packetThreadState[i];

        // Temporarily swap to the target sub-interpreter's state
        PyThreadState* current_tstate_before_swap = PyThreadState_Swap(target_tstate);

        FILE *fp = fopen(file, "r");
        if (PyRun_SimpleFileExFlags(fp, file, 1, NULL)) {
            LOG("Error loading '%s'\n", file);
            PyErr_Print();
        }

        // Swap back to the script loader thread's original interpreter state
        PyThreadState_Swap(current_tstate_before_swap);
    }
    loadingThread = -1;

    PyGILState_Release(gstate); // Release GIL acquired at the start of this thread
}
///////////////////////////////////////////////////////////////////////////////
// Arkime Packet
///////////////////////////////////////////////////////////////////////////////
/******************************************************************************/
typedef struct {
    int dummy_value; // Example placeholder for packet-specific data
} ArkimePacketState;
/******************************************************************************/
LOCAL void arkime_python_reader_load_files(int thread)
{
    if (!filesLoaded || filesLoaded->len == 0) {
        return;
    }

    // Acquire the GIL for this thread's initial interpreter (main interpreter)
    PyGILState_STATE gstate = PyGILState_Ensure();

    // Temporarily swap to the target sub-interpreter's state
    PyThreadState* target_tstate = readerThreadState[thread];
    PyThreadState* current_tstate_before_swap = PyThreadState_Swap(target_tstate);

    for (guint i = 0; i < filesLoaded->len; i++) {
        gchar *file = g_ptr_array_index(filesLoaded, i);
        FILE *fp = fopen(file, "r");
        if (PyRun_SimpleFileExFlags(fp, file, 1, NULL)) {
            LOG("Error loading '%s'\n", file);
            PyErr_Print();
        }

    }

    // Swap back to the script loader thread's original interpreter state
    PyThreadState_Swap(current_tstate_before_swap);
    PyGILState_Release(gstate); // Release GIL acquired at the start of this thread
}
/******************************************************************************/
LOCAL PyObject *arkime_python_packet_get(PyObject UNUSED(*self), PyObject *args)
{
    PyObject                    *py_packet_obj;
    const char                  *field;

    if (!PyArg_ParseTuple(args, "Os", &py_packet_obj, &field)) {
        return NULL;
    }

    const ArkimePacket_t *packet = (ArkimePacket_t *)PyLong_AsVoidPtr(py_packet_obj);

    switch (field[0]) {
    case 'c':
        if (strcmp(field, "copied") == 0) {
            return PyLong_FromUnsignedLong(packet->copied);
        }
        break;
    case 'd':
        if (strcmp(field, "direction") == 0) {
            return PyLong_FromUnsignedLong(packet->direction);
        }
        break;
    case 'e':
        if (strcmp(field, "etherOffset") == 0) {
            return PyLong_FromUnsignedLong(packet->etherOffset);
        }
        break;
    case 'i':
        if (strcmp(field, "ipOffset") == 0) {
            return PyLong_FromUnsignedLong(packet->ipOffset);
        }
        if (strcmp(field, "ipProtocol") == 0) {
            return PyLong_FromUnsignedLong(packet->ipProtocol);
        }
        break;
    case 'm':
        if (strcmp(field, "mProtocol") == 0) {
            return PyLong_FromUnsignedLong(packet->mProtocol);
        }
        break;
    case 'o':
        if (strcmp(field, "outerEtherOffset") == 0) {
            return PyLong_FromUnsignedLong(packet->outerEtherOffset);
        }
        if (strcmp(field, "outerIpOffset") == 0) {
            return PyLong_FromUnsignedLong(packet->outerIpOffset);
        }
        if (strcmp(field, "outerv6") == 0) {
            return PyLong_FromUnsignedLong(packet->outerv6);
        }
        break;
    case 'p':
        if (strcmp(field, "payloadLen") == 0) {
            return PyLong_FromUnsignedLong(packet->payloadLen);
        }
        if (strcmp(field, "payloadOffset") == 0) {
            return PyLong_FromUnsignedLong(packet->payloadOffset);
        }
        if (strcmp(field, "pktlen") == 0) {
            return PyLong_FromUnsignedLong(packet->pktlen);
        }
        break;
    case 'r':
        if (strcmp(field, "readerFilePos") == 0) {
            return PyLong_FromUnsignedLong(packet->readerFilePos);
        }
        if (strcmp(field, "readerPos") == 0) {
            return PyLong_FromUnsignedLong(packet->readerPos);
        }
        break;
    case 't':
        if (strcmp(field, "tunnel") == 0) {
            return PyLong_FromUnsignedLong(packet->tunnel);
        }
        break;
    case 'v':
        if (strcmp(field, "v6") == 0) {
            return PyLong_FromUnsignedLong(packet->v6);
        }
        if (strcmp(field, "vlan") == 0) {
            return PyLong_FromUnsignedLong(packet->vlan);
        }
        if (strcmp(field, "vni") == 0) {
            return PyLong_FromUnsignedLong(packet->vni);
        }
        break;
    case 'w':
        if (strcmp(field, "wasfrag") == 0) {
            return PyLong_FromUnsignedLong(packet->wasfrag);
        }
        if (strcmp(field, "writerFileNum") == 0) {
            return PyLong_FromUnsignedLong(packet->writerFileNum);
        }
        if (strcmp(field, "writerFilePos") == 0) {
            return PyLong_FromUnsignedLong(packet->writerFilePos);
        }
        break;
    }

    LOG("Unknown field '%s' in arkime_packet.get()\n", field);

    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_packet_set(PyObject UNUSED(*self), PyObject *args)
{
    PyObject                    *py_packet_obj;
    const char                  *field;
    uint32_t                     value;

    if (!PyArg_ParseTuple(args, "Osk", &py_packet_obj, &field, &value)) {
        return NULL;
    }

    ArkimePacket_t *packet = (ArkimePacket_t *)PyLong_AsVoidPtr(py_packet_obj);

    switch (field[0]) {
    case 'e':
        if (strcmp(field, "etherOffset") == 0) {
            packet->etherOffset = value;
            Py_RETURN_NONE;
        }
        break;
    case 'm':
        if (strcmp(field, "mProtocol") == 0) {
            packet->mProtocol = value;
            Py_RETURN_NONE;
        }
        break;
    case 'o':
        if (strcmp(field, "outerEtherOffset") == 0) {
            packet->outerEtherOffset = value;
            Py_RETURN_NONE;
        }
        if (strcmp(field, "outerIpOffset") == 0) {
            packet->outerIpOffset = value;
            Py_RETURN_NONE;
        }
        if (strcmp(field, "outerv6") == 0) {
            packet->outerv6 = value;
            Py_RETURN_NONE;
        }
        break;
    case 'p':
        if (strcmp(field, "payloadLen") == 0) {
            packet->payloadLen = value;
            Py_RETURN_NONE;
        }
        if (strcmp(field, "payloadOffset") == 0) {
            packet->payloadOffset = value;
            Py_RETURN_NONE;
        }
        break;
    case 't':
        if (strcmp(field, "tunnel") == 0) {
            packet->tunnel = value;
            Py_RETURN_NONE;
        }
        break;
    case 'v':
        if (strcmp(field, "v6") == 0) {
            packet->v6 = value;
            Py_RETURN_NONE;
        }
        if (strcmp(field, "vlan") == 0) {
            packet->vlan = value;
            Py_RETURN_NONE;
        }
        if (strcmp(field, "vni") == 0) {
            packet->vni = value;
            Py_RETURN_NONE;
        }
        break;
    }

    LOG("Unknown field '%s' in arkime_packet.set()\n", field);
    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL ArkimePacketRC arkime_python_packet_cb(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len, void *cbuw)
{
    PyEval_RestoreThread(readerThreadState[arkimeReaderThread]);

    ArkimePyCbMap_t *map = cbuw;
    PyObject *py_callback_obj = map->cb[arkimeReaderThread];

    PyObject *py_batch_opaque_ptr = PyLong_FromVoidPtr(batch);
    PyObject *py_packet_opaque_ptr = PyLong_FromVoidPtr(packet);
    PyObject *py_packet_memview = PyMemoryView_FromMemory((char *)data, len, PyBUF_READ);

    PyObject *py_args = Py_BuildValue("(OOOi)", py_batch_opaque_ptr, py_packet_opaque_ptr, py_packet_memview, len);

    if (!py_args) {
        PyErr_Print();
        LOGEXIT("Error building arguments tuple for Python callback");
    }

    PyObject *result = PyObject_CallObject(py_callback_obj, py_args);
    int r = 0;
    if (result == NULL) {
        PyErr_Print(); // Print any unhandled Python exceptions from the callback
        LOG("Error calling Python callback function from C");
    } else {
        r = PyLong_AsLong(result);
        Py_DECREF(result); // Decrement reference count of the Python result object
    }

    Py_XDECREF(py_args);
    Py_XDECREF(py_batch_opaque_ptr);
    Py_XDECREF(py_packet_opaque_ptr);
    Py_XDECREF(py_packet_memview);

    PyEval_SaveThread();
    return r;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_set_ethernet_cb(PyObject UNUSED(*self), PyObject *args)
{
    if (arkimeReaderThread == -1) {
        Py_RETURN_NONE;
    }

    int       type;
    PyObject *py_callback_obj;

    // i: type
    // O: py_callback_obj (Python object -> C PyObject*)
    if (!PyArg_ParseTuple(args, "iO", &type, &py_callback_obj)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    if (!PyCallable_Check(py_callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback must be a callable Python object.");
        return NULL;
    }
    Py_INCREF(py_callback_obj);

    ArkimePyCbMap_t *map = arkime_python_save_callback(":ethernet_cb", py_callback_obj);

    if (map)
        arkime_packet_set_ethernet_cb2(type, arkime_python_packet_cb, map);

    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_set_ip_cb(PyObject UNUSED(*self), PyObject *args)
{
    if (arkimeReaderThread == -1) {
        Py_RETURN_NONE;
    }

    int       type;
    PyObject *py_callback_obj;

    // i: type
    // O: py_callback_obj (Python object -> C PyObject*)
    if (!PyArg_ParseTuple(args, "iO", &type, &py_callback_obj)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    if (!PyCallable_Check(py_callback_obj)) {
        PyErr_SetString(PyExc_TypeError, "Callback must be a callable Python object.");
        return NULL;
    }
    Py_INCREF(py_callback_obj);

    ArkimePyCbMap_t *map = arkime_python_save_callback(":ip_cb", py_callback_obj);

    if (map)
        arkime_packet_set_ip_cb2(type, arkime_python_packet_cb, map);

    Py_RETURN_NONE;
}

/******************************************************************************/
LOCAL PyObject *arkime_python_run_ethernet_cb(PyObject UNUSED(*self), PyObject *args)
{
    PyObject   *py_batch_obj;
    PyObject   *py_packet_obj;
    PyObject   *py_packet_memview;
    int         type;
    const char *str;

    if (!PyArg_ParseTuple(args, "OOOis",  &py_batch_obj, &py_packet_obj, &py_packet_memview, &type, &str)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    ArkimePacketBatch_t *batch = (ArkimePacketBatch_t *)PyLong_AsVoidPtr(py_batch_obj);
    ArkimePacket_t *packet = (ArkimePacket_t *)PyLong_AsVoidPtr(py_packet_obj);

    Py_buffer py_data_buf;
    if (PyObject_GetBuffer(py_packet_memview, &py_data_buf, PyBUF_SIMPLE) == -1) {
        PyErr_Print();
        LOGEXIT("Error getting buffer from Python memoryview object");
    }

    ArkimePacketRC result = arkime_packet_run_ethernet_cb(batch, packet, py_data_buf.buf, py_data_buf.len, type, str);

    return PyLong_FromLong(result);
}

/******************************************************************************/
LOCAL PyObject *arkime_python_run_ip_cb(PyObject UNUSED(*self), PyObject *args)
{
    PyObject   *py_batch_obj;
    PyObject   *py_packet_obj;
    PyObject   *py_packet_memview;
    int         type;
    const char *str;

    if (!PyArg_ParseTuple(args, "OOOis",  &py_batch_obj, &py_packet_obj, &py_packet_memview, &type, &str)) {
        // PyArg_ParseTuple sets an appropriate Python exception on failure
        return NULL;
    }

    ArkimePacketBatch_t *batch = (ArkimePacketBatch_t *)PyLong_AsVoidPtr(py_batch_obj);
    ArkimePacket_t *packet = (ArkimePacket_t *)PyLong_AsVoidPtr(py_packet_obj);

    Py_buffer py_data_buf;
    if (PyObject_GetBuffer(py_packet_memview, &py_data_buf, PyBUF_SIMPLE) == -1) {
        PyErr_Print();
        LOGEXIT("Error getting buffer from Python memoryview object");
    }

    ArkimePacketRC result = arkime_packet_run_ip_cb(batch, packet, py_data_buf.buf, py_data_buf.len, type, str);

    return PyLong_FromLong(result);
}
/******************************************************************************/
LOCAL PyMethodDef arkime_packet_methods[] = {
    { "get", arkime_python_packet_get, METH_VARARGS, NULL },
    { "set", arkime_python_packet_set, METH_VARARGS, NULL },
    { "run_ethernet_cb", arkime_python_run_ethernet_cb, METH_VARARGS, NULL },
    { "run_ip_cb", arkime_python_run_ip_cb, METH_VARARGS, NULL },
    { "set_ethernet_cb", arkime_python_set_ethernet_cb, METH_VARARGS, NULL },
    { "set_ip_cb", arkime_python_set_ip_cb, METH_VARARGS, NULL },
    {NULL, NULL, 0, NULL} // Sentinel
};
/******************************************************************************/
LOCAL struct PyModuleDef arkime_packet_module = {
    PyModuleDef_HEAD_INIT,
    "arkime_packet", // name of module (lowercase with underscore)
    NULL,            // module documentation, may be NULL
    sizeof(ArkimePacketState), // m_size: Size of per-interpreter state for this module
    arkime_packet_methods,
    NULL,     // m_slots
    NULL,     // m_traverse
    NULL,     // m_clear
    NULL      // m_free
};
/******************************************************************************/
// Function to initialize our arkime_packet C module
PyMODINIT_FUNC PyInit_arkime_packet(void)
{
    PyObject* m = PyModule_Create(&arkime_packet_module);
    if (m == NULL) {
        return NULL;
    }
    // Get the per-interpreter state pointer and initialize it
    ArkimePacketState* state = (ArkimePacketState*)PyModule_GetState(m);
    if (state == NULL) {
        Py_DECREF(m);
        return NULL;
    }
    state->dummy_value = 0; // Initialize dummy value

    return m;
}
///////////////////////////////////////////////////////////////////////////////
// Common
///////////////////////////////////////////////////////////////////////////////
/******************************************************************************/
LOCAL int arkime_python_pp_load(const char *path)
{
    arkime_python_packet_load_file(path);
    if (!filesLoaded) {
        filesLoaded = g_ptr_array_new();
    }
    g_ptr_array_add(filesLoaded, g_strdup(path));
    return 0;
}
/******************************************************************************/
LOCAL int threads;
LOCAL void arkime_python_thread_init(PyThreadState **threadState)
{
    PyInterpreterConfig pconfig = _PyInterpreterConfig_INIT;

    PyStatus status = Py_NewInterpreterFromConfig(threadState, &pconfig);
    if (PyStatus_Exception(status)) {
        LOGEXIT("Error creating new Python interpreter from config: %s",
                status.err_msg ? status.err_msg : "Unknown error");
    }

    PyObject *p_arkime_module_obj = PyModule_Create(&arkime_module);
    if (!p_arkime_module_obj) {
        PyErr_Print();
        LOGEXIT("Failed to create arkime module for sub-interpreter.");
    }

    PyObject *p_arkime_session_module_obj = PyModule_Create(&arkime_session_module);
    if (!p_arkime_session_module_obj) {
        PyErr_Print();
        LOGEXIT("Failed to create arkime_session module for sub-interpreter.");
    }

    PyObject *p_arkime_packet_module_obj = PyModule_Create(&arkime_packet_module);
    if (!p_arkime_packet_module_obj) {
        PyErr_Print();
        LOGEXIT("Failed to create arkime_packet module for sub-interpreter.");
    }

    // Get the sub-interpreter's module dictionary (sys.modules)
    PyObject* sys_modules = PyImport_GetModuleDict();
    if (!sys_modules) {
        PyErr_Print();
        LOGEXIT("Failed to get sys.modules for sub-interpreter.");
    }

    if (PyDict_SetItemString(sys_modules, "arkime", p_arkime_module_obj) < 0) {
        PyErr_Print();
        LOGEXIT("Failed to add arkime module to sys.modules.\n");
    }
    Py_DECREF(p_arkime_module_obj); // Decrement our local reference, as sys.modules now owns it.

    if (PyDict_SetItemString(sys_modules, "arkime_session", p_arkime_session_module_obj) < 0) {
        PyErr_Print();
        LOGEXIT("Failed to add arkime_session module to sys.modules.\n");
    }
    Py_DECREF(p_arkime_session_module_obj); // Decrement our local reference, as sys.modules now owns it.

    if (PyDict_SetItemString(sys_modules, "arkime_packet", p_arkime_packet_module_obj) < 0) {
        PyErr_Print();
        LOGEXIT("Failed to add arkime_packet module to sys.modules.\n");
    }
    Py_DECREF(p_arkime_packet_module_obj); // Decrement our local reference, as sys.modules now owns it.

    if (!PyDict_GetItemString(sys_modules, "arkime")) {
        LOGEXIT("C Debug: 'arkime' module NOT found in sys.modules after insertion.");
    }

    if (!PyDict_GetItemString(sys_modules, "arkime_session")) {
        LOGEXIT("C Debug: 'arkime_session' module NOT found in sys.modules after insertion.");
    }

    if (!PyDict_GetItemString(sys_modules, "arkime_packet")) {
        LOGEXIT("C Debug: 'arkime_packet' module NOT found in sys.modules after insertion.");
    }

    Py_DECREF(sys_modules);

    PyModule_AddStringConstant(p_arkime_module_obj, "VERSION", VERSION);
    PyModule_AddStringConstant(p_arkime_module_obj, "CONFIG_PREFIX", CONFIG_PREFIX);
    PyModule_AddIntConstant(p_arkime_module_obj, "API_VERSION", ARKIME_API_VERSION);
    PyModule_AddIntConstant(p_arkime_module_obj, "PORT_UDP_SRC", ARKIME_PARSERS_PORT_UDP_SRC);
    PyModule_AddIntConstant(p_arkime_module_obj, "PORT_UDP_DST", ARKIME_PARSERS_PORT_UDP_DST);
    PyModule_AddIntConstant(p_arkime_module_obj, "PORT_TCP_SRC", ARKIME_PARSERS_PORT_TCP_SRC);
    PyModule_AddIntConstant(p_arkime_module_obj, "PORT_TCP_DST", ARKIME_PARSERS_PORT_TCP_DST);
    PyModule_AddIntConstant(p_arkime_module_obj, "PORT_SCTP_SRC", ARKIME_PARSERS_PORT_SCTP_SRC);
    PyModule_AddIntConstant(p_arkime_module_obj, "PORT_SCTP_DST", ARKIME_PARSERS_PORT_SCTP_DST);

    PyModule_AddIntConstant(p_arkime_packet_module_obj, "DO_PROCESS", ARKIME_PACKET_DO_PROCESS);
    PyModule_AddIntConstant(p_arkime_packet_module_obj, "IP_DROPPED", ARKIME_PACKET_IP_DROPPED);
    PyModule_AddIntConstant(p_arkime_packet_module_obj, "OVERLOAD_DROPPED", ARKIME_PACKET_OVERLOAD_DROPPED);
    PyModule_AddIntConstant(p_arkime_packet_module_obj, "CORRUPT", ARKIME_PACKET_CORRUPT);
    PyModule_AddIntConstant(p_arkime_packet_module_obj, "UNKNOWN", ARKIME_PACKET_UNKNOWN);
    PyModule_AddIntConstant(p_arkime_packet_module_obj, "IPPORT_DROPPED", ARKIME_PACKET_IPPORT_DROPPED);
    PyModule_AddIntConstant(p_arkime_packet_module_obj, "DONT_PROCESS", ARKIME_PACKET_DONT_PROCESS);
    PyModule_AddIntConstant(p_arkime_packet_module_obj, "DONT_PROCESS_OR_FREE", ARKIME_PACKET_DONT_PROCESS_OR_FREE);
    PyModule_AddIntConstant(p_arkime_packet_module_obj, "DUPLICATE_DROPPED", ARKIME_PACKET_DUPLICATE_DROPPED);

    PyEval_SaveThread();
}
/******************************************************************************/
LOCAL uint32_t arkime_python_packet_thread_init(int thread, void UNUSED(*uw), void UNUSED(*cbuw))
{
    if (disablePython) {
        return 0;
    }

    threads++;
    arkime_python_thread_init(&packetThreadState[thread]);

    return 0;
}
/******************************************************************************/
LOCAL uint32_t arkime_python_packet_thread_exit(int thread, void UNUSED(*uw), void UNUSED(*cbuw))
{
    if (config.debug)
        LOG("Exiting Python interpreter for thread %d.", thread);
    PyEval_RestoreThread(packetThreadState[thread]);
    Py_EndInterpreter(packetThreadState[thread]);
    threads--;
    return 0;
}
/******************************************************************************/
LOCAL uint32_t arkime_python_reader_thread_init(int thread, void UNUSED(*uw), void UNUSED(*cbuw))
{
    if (disablePython) {
        return 0;
    }

    arkimeReaderThread = thread;
    threads++;
    arkime_python_thread_init(&readerThreadState[thread]);
    arkime_python_reader_load_files(thread);

    return 0;
}
/******************************************************************************/
LOCAL uint32_t arkime_python_reader_thread_exit(int thread, void UNUSED(*uw), void UNUSED(*cbuw))
{
    if (config.debug)
        LOG("Exiting Python interpreter for thread %d.", thread);
    PyEval_RestoreThread(readerThreadState[thread]);
    Py_EndInterpreter(readerThreadState[thread]);
    threads--;
    return 0;
}
/******************************************************************************/
void arkime_python_init()
{
    disablePython = arkime_config_boolean(NULL, "disablePython", FALSE);

    if (disablePython) {
        return;
    }

    Py_InitializeEx(0);
    if (!Py_IsInitialized())
        LOGEXIT("Failed to initialize Python interpreter.\n");

    mainThreadState = PyEval_SaveThread();

    arkime_parsers_register_load_extension(".py", arkime_python_pp_load);
    arkime_plugins_register_load_extension(".py", arkime_python_pp_load);

    arkimePyCbMap = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

    arkime_add_named_func("arkime_packet_thread_init", arkime_python_packet_thread_init, NULL);
    arkime_add_named_func("arkime_packet_thread_exit", arkime_python_packet_thread_exit, NULL);

    arkime_add_named_func("arkime_reader_thread_init", arkime_python_reader_thread_init, NULL);
    arkime_add_named_func("arkime_reader_thread_exit", arkime_python_reader_thread_exit, NULL);
}
/******************************************************************************/
void arkime_python_exit()
{
    if (disablePython) {
        return;
    }

    if (config.debug)
        LOG("Exiting Python");
    arkime_packet_thread_wake(-1);

    while (threads > 0) {
        if (config.debug > 1)
            LOG("Waiting for %d Python threads to exit", threads);
        usleep(10000);
    }

    PyEval_RestoreThread(mainThreadState);
    Py_FinalizeEx();
}
#endif
