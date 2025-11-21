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
PORT_UDP_SRC: PortKind
PORT_UDP_DST: PortKind
PORT_TCP_SRC: PortKind
PORT_TCP_DST: PortKind
PORT_SCTP_SRC: PortKind
PORT_SCTP_DST: PortKind

# === Methods ===
def field_define(fieldExpression: str, fieldDefinition: str) -> int:
    """
    Create a new field that can be used in sessions. This method returns a fieldPosition that can be used in other calls for faster field access.

    Args:
        fieldExpression: The expression used in viewer to access the field.
        fieldDefinition: The definition of the field from custom-fields.
    """
    ...
def field_get(fieldExpression: str) -> int:
    """
    Retrieve the field position for a field expression.

    Args:
        fieldExpression: The expression used in viewer to access the field.
    """
    ...

def register_pre_save(saveCb: SaveCb) -> None:
    """
    saveCb: The callback to call when the session is going to be saved to the database but before some housekeeping is done, such as running the save rules.
    """
    ...
def register_save(saveCb: SaveCb) -> None:
    """
    saveCb: The callback to call when the session is being saved to the database.
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
