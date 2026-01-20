import arkime
import arkime_session
import arkime_packet
import sys

# Create a new field in the session we will be setting
pythonPos = arkime.field_define("test.python", "kind:lotermfield;db:test.python")

def my_pre_save_callback(session, final):
    ipSrc = arkime_session.get(session, "ip.src")
    if (ipSrc == "10.0.0.1"):
        arkime_session.add_string(session, pythonPos, "my value")
        arkime_session.add_int(session, "http.statuscode", 12345)


arkime.register_pre_save(my_pre_save_callback)
