"""
Python Arkime Session Module

The Python Arkime Session module has methods for dealing with sessions. The API is very unpythonic and treats the session as a opaque object that needs to be passed around.
"""

from typing import Any

from .types import ArkimeSession, ParserCb

# === Methods ===
def add_int(session: ArkimeSession, fieldPosOrExp: int | str, value: int) -> None:
    """
    Add an integer value to a session field.

    Args:
        session: The session object from the classifyCb or parserCb.
        fieldPosOrExp: The field position returned by field_define/field_get or the field expression
        value: The integer value to add to the session field.
    """
    ...
def add_protocol(session: ArkimeSession, protocol: str) -> None:
    """
    Optimized version of add_string(session, 'protocol', protocol).

    Args:
        session: The session object from the classifyCb or parserCb.
        protocol: The protocol string to add to the session.
    """
    ...
def add_string(session: ArkimeSession, fieldPosOrExp: int | str, value: str) -> None:
    """
    Add a string value to a session field.

    Args:
        session: The session object from the classifyCb or parserCb.
        fieldPosOrExp: The field position returned by field_define/field_get or the field expression
        value: The string value to add to the session field.
    """
    ...
def add_tag(session: ArkimeSession, tag: str) -> None:
    """
    Optimized version of add_string(session, 'tags', tag).

    Args:
        session: The session object from the classifyCb or parserCb.
        tag: The tag string to add to the session.
    """
    ...

def decref(session: ArkimeSession) -> None:
    """
    Decrement the reference count of a session.

    Args:
        session: The session object from the classifyCb or parserCb.
    """
    ...
def incref(session: ArkimeSession) -> None:
    """
    Increment the reference count of a session.

    Args:
        session: The session object from the classifyCb or parserCb.
    """
    ...

def get(session: ArkimeSession, fieldPosOrExp: int | str) -> Any | list[Any]:
    """
    Retrieve the value of a session field. Can be a list of values or a single value depending on the field.

    Args:
        session: The session object from the classifyCb or parserCb.
        fieldPosOrExp: The field position returned by field_define/field_get or the field expression
    """
    ...

def has_protocol(session: ArkimeSession, protocol: str) -> bool:
    """
    Optimized version of get(session, “protocol”) and checking if the list contains the protocol.

    Args:
        session: The session object from the classifyCb or parserCb.
        protocol: The protocol string to check for in the session.
    """
    ...

def register_parser(session: ArkimeSession, parserCb: ParserCb) -> None:
    """
    Register a parser callback for every packet of the session.

    Args:
        session: The session object from the classifyCb or parserCb.
        parserCb: The callback to call for every packet of the session in each direction.
    """
    ...
