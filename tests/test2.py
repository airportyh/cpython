class TypeVar:
    def __init__(self, name):
        self.__name__ = name
    def __init_subclass__(self):
        pass
class _GenericAlias:
    def __init__(self, origin):
        self.__origin__ = origin
    def __mro_entries__(self, bases):
        return (self.__origin__,)
class Generic:
    def __class_getitem__(cls, params):
        return _GenericAlias(cls)
AnyStr = TypeVar('AnyStr')
class IO(Generic[AnyStr]):
    """Typed version of the return of open() in text mode."""
