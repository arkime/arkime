import arkime
import arkime_session

# Repro for Py_DECREF without thread state in
# arkime_python_session_parsers_free_cb: register a per-session closure so
# arkime holds the ONLY reference; when the session is freed the DECREF
# deallocs the closure with no current Python thread state.


def classify(session, data, remaining, which):
    marker = list(range(1000))  # force a real closure cell with GC-tracked refs

    def parser(session, data, remaining, which):
        _ = marker
        return 0

    arkime_session.register_parser(session, parser)
    arkime_session.add_protocol(session, "pygilrepro")


arkime.register_tcp_classifier("pygilrepro", 0, b"GET ", classify)
