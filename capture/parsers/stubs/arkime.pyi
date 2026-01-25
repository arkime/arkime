"""
Python Arkime Module

The Python Arkime module has high level methods to register callbacks for packet processing.
"""

from .types import ClassifyCb, SaveCb, PortKind

# === Constants ===
VERSION: str           # The Arkime version as a string
CONFIG_PREFIX: str     # The Arkime install prefix, usually /opt/arkime
API_VERSION: int       # The Arkime API version from arkime.h

# === PortKind constants for register_port_classifier ===
PORT_UDP_SRC: PortKind   # Match UDP source port
PORT_UDP_DST: PortKind   # Match UDP destination port
PORT_TCP_SRC: PortKind   # Match TCP source port
PORT_TCP_DST: PortKind   # Match TCP destination port
PORT_SCTP_SRC: PortKind  # Match SCTP source port
PORT_SCTP_DST: PortKind  # Match SCTP destination port

# === Methods ===
def field_define(fieldExpression: str, fieldDefinition: str) -> int:
    """
    Create a new field that can be used in sessions. Must be called at startup, not from callbacks.

    Args:
        fieldExpression: The expression used in viewer to access the field (e.g., "myproto.field").
        fieldDefinition: The field definition in custom-fields format.
            Format: "db:<esfield>;kind:<type>;friendly:<name>;count:<bool>;help:<text>"
            Example: "db:myproto.field;kind:termfield;friendly:My Field;count:true;help:Description"
            Types: termfield, integer, ip, lotermfield, uptermfield, seconds, textfield

    Returns:
        int: The field position for use with add_string/add_int (faster than using expression string).
    """
    ...
def field_get(fieldExpression: str) -> int:
    """
    Retrieve the field position for a previously defined field expression.

    Args:
        fieldExpression: The expression used in viewer to access the field (e.g., "myproto.field").

    Returns:
        int: The field position, or -1 if the field does not exist.
    """
    ...

def register_pre_save(saveCb: SaveCb) -> None:
    """
    Register a callback to be called before a session is saved to the database.
    Called before housekeeping such as running save rules, so fields added here can trigger rules.

    Args:
        saveCb: The callback function with signature (session, final) -> None.
    """
    ...
def register_save(saveCb: SaveCb) -> None:
    """
    Register a callback to be called when a session is being saved to the database.
    This is the final opportunity to add fields or tags before the session is written.

    Args:
        saveCb: The callback function with signature (session, final) -> None.
    """
    ...

def register_port_classifier(name: str, port: int, portKind: PortKind, classifyCb: ClassifyCb) -> None:
    """
    Register a classifier that matches on a specific port and protocol type. This usually isn't recommended since most protocols can run on any port.

    Args:
        name: The short name of the classifier, used internally to identify the classifier.
        port: The IP port to match on.
        portKind: Bitwise OR the values from the PortKind constants to match on.
        classifyCb: The callback to call when the classifier matches.
    """
    ...
def register_tcp_classifier(name: str, matchOffset: int, matchBytes: bytes, classifyCb: ClassifyCb) -> None:
    """
    Register a TCP classifier that will call the classifyCb callback for the first packet
    of a session in each direction that matches the matchBytes starting at the matchOffset.

    Args:
        name: The short name of the classifier, used internally to identify the classifier.
        matchOffset: The byte offset in the packet where the matchBytes should be found.
        matchBytes: The bytes to match in the packet.
        classifyCb: The callback to call when the classifier matches.
    """
    ...
def register_udp_classifier(name: str, matchOffset: int, matchBytes: bytes, classifyCb: ClassifyCb) -> None:
    """
    Register a UDP classifier that will call the classifyCb callback for the first packet
    of a session in each direction that matches the matchBytes starting at the matchOffset.

    Args:
        name: The short name of the classifier, used internally to identify the classifier.
        matchOffset: The byte offset in the packet where the matchBytes should be found.
        matchBytes: The bytes to match in the packet.
        classifyCb: The callback to call when the classifier matches.
    """
    ...
def register_sctp_classifier(name: str, matchOffset: int, matchBytes: bytes, classifyCb: ClassifyCb) -> None:
    """
    Register a SCTP classifier that will call the classifyCb callback for the first packet
    of a session in each direction that matches the matchBytes starting at the matchOffset.

    Args:
        name: The short name of the classifier, used internally to identify the classifier.
        matchOffset: The byte offset in the packet where the matchBytes should be found.
        matchBytes: The bytes to match in the packet.
        classifyCb: The callback to call when the classifier matches. The which field will contain the direction AND sctp stream id. Arkime will send full messages to the callback.
    """
    ...
def register_sctp_protocol_classifier(name: str, protocol: int, classifyCb: ClassifyCb) -> None:
    """
    Register a SCTP protocol classifier that will call the classifyCb callback for the first packet
    of a session in each direction that matches the protocolId in the SCTP header.

    Args:
        name: The short name of the classifier, used internally to identify the classifier.
        protocol: The protocol id in the SCTP header to match.
        classifyCb: The callback to call when the classifier matches. The which field will contain the direction AND sctp stream id. Arkime will send full messages to the callback.
    """
    ...
