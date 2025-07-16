/******************************************************************************/
/* python.c  -- Python integration for Arkime
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkimeconfig.h"

#ifndef HAVE_PYTHON
void arkime_python_init() {}
void arkime_python_thread_init(int thread) {}
void arkime_python_exit() {}
#else
#include "arkime.h"
#include "Python.h"

extern ArkimeConfig_t        config;


LOCAL PyThreadState *mainThreadState;
LOCAL PyThreadState *threadState[ARKIME_MAX_PACKET_THREADS];
/******************************************************************************/
typedef struct {
    int dummy;
} ArkimeState;

/******************************************************************************/
LOCAL void arkime_python_classify_cb(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *uw)
{
    PyEval_RestoreThread(threadState[session->thread]);

    PyObject *py_callback_obj = (PyObject *)uw;
    PyObject *py_packet_bytes = PyBytes_FromStringAndSize((const char *)data, len);
    PyObject *py_session_opaque_ptr = PyLong_FromVoidPtr(session);

    PyObject *py_args = Py_BuildValue("(OOii)", py_session_opaque_ptr, py_packet_bytes, len, which);

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

    Py_XDECREF(py_packet_bytes);
    Py_XDECREF(py_args);
    Py_XDECREF(py_session_opaque_ptr);

    PyEval_SaveThread();
}
/******************************************************************************/
LOCAL PyObject *arkime_python_register_tcp_classifier(PyObject UNUSED(*self), PyObject *args)
{
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

    arkime_parsers_classifier_register_tcp (
        name_str,
        (void *)py_callback_obj,
        offset,
        match_bytes,
        (int)match_len,
        arkime_python_classify_cb
    );

    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_register_udp_classifier(PyObject UNUSED(*self), PyObject *args)
{
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

    arkime_parsers_classifier_register_udp (
        name_str,
        (void *)py_callback_obj,
        offset,
        match_bytes,
        (int)match_len,
        arkime_python_classify_cb
    );

    Py_RETURN_NONE;
}

LOCAL PyMethodDef arkime_methods[] = {
    { "register_tcp_classifier", arkime_python_register_tcp_classifier, METH_VARARGS, NULL },
    { "register_udp_classifier", arkime_python_register_udp_classifier, METH_VARARGS, NULL },
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
/******************************************************************************/
typedef struct {
    int dummy_value; // Example placeholder for session-specific data
} ArkimeSessionState;

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
LOCAL PyObject *arkime_python_session_incr_outstanding(PyObject UNUSED(*self), PyObject *args)
{
    PyObject *py_session_obj;

    if (!PyArg_ParseTuple(args, "O", &py_session_obj)) {
        return NULL;
    }

    arkime_session_incr_outstanding((ArkimeSession_t *)PyLong_AsVoidPtr(py_session_obj));
    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyObject *arkime_python_session_decr_outstanding(PyObject UNUSED(*self), PyObject *args)
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
    PyObject *py_session_obj;
    const char *field;

    if (!PyArg_ParseTuple(args, "Os", &py_session_obj, &field)) {
        return NULL;
    }

    int pos;
    if (isdigit(field[0]))
        pos = atoi(field);
    else {
        pos = arkime_field_by_exp(field);
    }

    Py_RETURN_NONE;
}
/******************************************************************************/
LOCAL PyMethodDef arkime_session_methods[] = {
    { "add_tag", arkime_python_session_add_tag, METH_VARARGS, NULL },
    { "add_protocol", arkime_python_session_add_protocol, METH_VARARGS, NULL },
    { "has_protocol", arkime_python_session_has_protocol, METH_VARARGS, NULL },
    { "add_int", arkime_python_session_add_int, METH_VARARGS, NULL },
    { "add_string", arkime_python_session_add_string, METH_VARARGS, NULL },
    { "incr_outstanding", arkime_python_session_incr_outstanding, METH_VARARGS, NULL },
    { "decr_outstanding", arkime_python_session_decr_outstanding, METH_VARARGS, NULL },
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
void arkime_python_load_file(const char *file)
{
    // Make sure all the threads have been initialized before proceeding
    for (int i = 0; i < config.packetThreads; i++) {
        while (threadState[i] == NULL) {
            // Wait for the thread state to be initialized
            usleep(1000); // Sleep for 1ms to avoid busy waiting
        }
    }

    // Acquire the GIL for this thread's initial interpreter (main interpreter)
    PyGILState_STATE gstate = PyGILState_Ensure();

    for (int i = 0; i < config.packetThreads; i++) {
        PyThreadState* target_tstate = threadState[i];

        // Temporarily swap to the target sub-interpreter's state
        PyThreadState* current_tstate_before_swap = PyThreadState_Swap(target_tstate);

        FILE *fp = fopen(file, "r");
        if (PyRun_SimpleFileExFlags(fp, file, 1, NULL)) {
            PyErr_Print();
            LOG("Thread %p: Error creating %s name.\n", (void*)pthread_self(), file);
        }

        // Swap back to the script loader thread's original interpreter state
        PyThreadState_Swap(current_tstate_before_swap);
    }

    PyGILState_Release(gstate); // Release GIL acquired at the start of this thread
}
/******************************************************************************/
int arkime_python_pp_load(const char *path)
{
    arkime_python_load_file(path);
    return 0;
}
/******************************************************************************/
void arkime_python_init()
{
    Py_InitializeEx(0);
    if (!Py_IsInitialized())
        LOGEXIT("Failed to initialize Python interpreter.\n");

    mainThreadState = PyEval_SaveThread();

    arkime_parsers_register_load_extension(".py", arkime_python_pp_load);
    arkime_plugins_register_load_extension(".py", arkime_python_pp_load);
}
/******************************************************************************/
void arkime_python_thread_init(int thread)
{
    PyInterpreterConfig pconfig = _PyInterpreterConfig_INIT;

    PyStatus status = Py_NewInterpreterFromConfig(&threadState[thread], &pconfig);
    if (PyStatus_Exception(status)) {
        LOGEXIT("Error creating new Python interpreter from config in thread %p: %s",
                pthread_self(), status.err_msg ? status.err_msg : "Unknown error");
    }

    PyObject *p_arkime_module_obj = PyModule_Create(&arkime_module);
    if (!p_arkime_module_obj) {
        PyErr_Print();
        LOGEXIT("Failed to create arkime module for sub-interpreter %p.", pthread_self());
    }

    PyObject *p_arkime_session_module_obj = PyModule_Create(&arkime_session_module);
    if (!p_arkime_session_module_obj) {
        PyErr_Print();
        LOGEXIT("Failed to create arkime_session module for sub-interpreter %p.", pthread_self());
    }

    // Get the sub-interpreter's module dictionary (sys.modules)
    PyObject* sys_modules = PyImport_GetModuleDict();
    if (!sys_modules) {
        PyErr_Print();
        LOGEXIT("Failed to get sys.modules for sub-interpreter %p.", pthread_self());
    }

    if (PyDict_SetItemString(sys_modules, "arkime", p_arkime_module_obj) < 0) {
        PyErr_Print();
        LOGEXIT("Thread %p: Failed to add arkime module to sys.modules.\n", pthread_self());
    }
    Py_DECREF(p_arkime_module_obj); // Decrement our local reference, as sys.modules now owns it.

    if (PyDict_SetItemString(sys_modules, "arkime_session", p_arkime_session_module_obj) < 0) {
        PyErr_Print();
        LOGEXIT("Thread %p: Failed to add arkime_session module to sys.modules.\n", pthread_self());
    }
    Py_DECREF(p_arkime_session_module_obj); // Decrement our local reference, as sys.modules now owns it.


    if (!PyDict_GetItemString(sys_modules, "arkime")) {
        LOGEXIT("Thread %p: C Debug: 'arkime' module NOT found in sys.modules after insertion.", pthread_self());
    }

    if (!PyDict_GetItemString(sys_modules, "arkime_session")) {
        LOGEXIT("Thread %p: C Debug: 'arkime_session' module NOT found in sys.modules after insertion.", pthread_self());
    }

    PyEval_SaveThread();
}
/******************************************************************************/
void arkime_python_exit()
{
    //PyEval_RestoreThread(mainThreadState);
    //Py_FinalizeEx();
}
#endif
